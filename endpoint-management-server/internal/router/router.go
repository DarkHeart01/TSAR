package router

import (
	"time"

	"github.com/gin-gonic/gin"
	"github.com/redis/go-redis/v9"

	"endpoint-management-server/internal/handlers"
	"endpoint-management-server/internal/middleware"
	"endpoint-management-server/internal/repository"
)

func New(agentHandler *handlers.AgentHandler, agentRepo *repository.AgentRepository, redisClient *redis.Client) *gin.Engine {
	r := gin.New()
	r.Use(gin.Recovery(), gin.Logger(), middleware.SecurityHeaders())

	// Trust only the reverse proxy for client IP resolution (X-Forwarded-For).
	r.SetTrustedProxies([]string{"127.0.0.1"})

	v1 := r.Group("/api/v1")
	{
		agent := v1.Group("/agent")
		agent.Use(middleware.RateLimiter(redisClient, 60, time.Minute))
		{
			agent.POST("/register", agentHandler.Register)

			authed := agent.Group("")
			authed.Use(middleware.AgentAuth(agentRepo))
			{
				authed.GET("/poll", agentHandler.Poll)
				authed.POST("/telemetry", agentHandler.Telemetry)
			}
		}

		// Operator-facing task creation; in production this should sit
		// behind separate operator authentication (e.g. mTLS or an admin
		// JWT), not the agent bearer token scheme.
		v1.POST("/agent/:agent_id/tasks", agentHandler.CreateTask)
	}

	r.GET("/healthz", func(c *gin.Context) {
		c.JSON(200, gin.H{"status": "ok"})
	})

	return r
}
