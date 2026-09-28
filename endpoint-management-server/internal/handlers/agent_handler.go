package handlers

import (
	"encoding/json"
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"

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
}

func NewAgentHandler(
	agentRepo *repository.AgentRepository,
	taskRepo *repository.TaskRepository,
	telemetryRepo *repository.TelemetryRepository,
	auditRepo *repository.AuditRepository,
	taskQueue *queue.TaskQueue,
	maxRetries int,
) *AgentHandler {
	return &AgentHandler{
		agentRepo:     agentRepo,
		taskRepo:      taskRepo,
		telemetryRepo: telemetryRepo,
		auditRepo:     auditRepo,
		taskQueue:     taskQueue,
		maxRetries:    maxRetries,
	}
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
