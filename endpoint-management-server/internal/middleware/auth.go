package middleware

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"net/http"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/redis/go-redis/v9"

	"endpoint-management-server/internal/repository"
)

const AgentContextKey = "agent"

// HashToken produces a stable, non-reversible lookup key for a bearer
// token so the raw secret is never stored or logged.
func HashToken(token string) string {
	sum := sha256.Sum256([]byte(token))
	return hex.EncodeToString(sum[:])
}

// AgentAuth validates the Bearer token against the agents table and
// attaches the resolved agent to the request context.
func AgentAuth(agentRepo *repository.AgentRepository) gin.HandlerFunc {
	return func(c *gin.Context) {
		header := c.GetHeader("Authorization")
		if !strings.HasPrefix(header, "Bearer ") {
			c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "missing or malformed bearer token"})
			return
		}
		token := strings.TrimPrefix(header, "Bearer ")
		if len(token) < 32 {
			c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "invalid token"})
			return
		}

		agent, err := agentRepo.GetByToken(c.Request.Context(), HashToken(token))
		if err != nil {
			c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "invalid or expired token"})
			return
		}

		c.Set(AgentContextKey, agent)
		c.Next()
	}
}

// RateLimiter implements a Redis-backed sliding-window limiter keyed by
// client IP, suitable for multi-instance horizontal scaling.
func RateLimiter(client *redis.Client, limit int, window time.Duration) gin.HandlerFunc {
	return func(c *gin.Context) {
		ip := c.ClientIP()
		key := "ratelimit:" + ip

		ctx := context.Background()
		count, err := client.Incr(ctx, key).Result()
		if err != nil {
			// Fail open: do not block traffic on a Redis outage, but log it upstream.
			c.Next()
			return
		}
		if count == 1 {
			client.Expire(ctx, key, window)
		}
		if count > int64(limit) {
			c.Header("Retry-After", window.String())
			c.AbortWithStatusJSON(http.StatusTooManyRequests, gin.H{"error": "rate limit exceeded"})
			return
		}
		c.Next()
	}
}

// SecurityHeaders enforces baseline transport and response hardening.
// TLS termination itself happens at the reverse proxy (see nginx.conf);
// HSTS here ensures browsers/clients never silently downgrade.
func SecurityHeaders() gin.HandlerFunc {
	return func(c *gin.Context) {
		c.Header("Strict-Transport-Security", "max-age=63072000; includeSubDomains")
		c.Header("X-Content-Type-Options", "nosniff")
		c.Header("X-Frame-Options", "DENY")
		c.Next()
	}
}
