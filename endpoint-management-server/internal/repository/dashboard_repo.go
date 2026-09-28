package repository

import (
	"context"
	"database/sql"
	"fmt"
	"time"

	"github.com/google/uuid"

	"endpoint-management-server/internal/models"
)

// AgentDashRow is agent info plus computed counts used by the dashboard
// agent-list panel.
type AgentDashRow struct {
	AgentID        uuid.UUID `json:"agent_id"`
	Hostname       string    `json:"hostname"`
	IPAddress      string    `json:"ip_address"`
	Status         string    `json:"status"`
	LastSeen       time.Time `json:"last_seen"`
	PendingTasks   int64     `json:"pending_tasks"`
	TelemetryCount int64     `json:"telemetry_count"`
}

// TaskDashRow is the task-queue panel view.
type TaskDashRow struct {
	TaskID      uuid.UUID  `json:"task_id"`
	AgentID     uuid.UUID  `json:"agent_id"`
	CommandType string     `json:"command_type"`
	Status      string     `json:"status"`
	CreatedAt   time.Time  `json:"created_at"`
	ExpiresAt   time.Time  `json:"expires_at"`
	RetryCount  int        `json:"retry_count"`
	MaxRetries  int        `json:"max_retries"`
}

type DashboardRepository struct {
	db *sql.DB
}

func NewDashboardRepository(db *sql.DB) *DashboardRepository {
	return &DashboardRepository{db: db}
}

// ListAgentsWithStats runs a single JOIN query returning each agent with
// its pending task count and total telemetry count.
func (r *DashboardRepository) ListAgentsWithStats(ctx context.Context) ([]AgentDashRow, error) {
	rows, err := r.db.QueryContext(ctx, `
		SELECT
		    a.agent_id, a.hostname, a.ip_address, a.status, a.last_seen,
		    COUNT(t.task_id)   FILTER (WHERE t.status = 'pending') AS pending_tasks,
		    COUNT(tel.log_id)                                       AS telemetry_count
		FROM endpoint_mgmt.agents a
		LEFT JOIN endpoint_mgmt.tasks     t   ON t.agent_id   = a.agent_id
		LEFT JOIN endpoint_mgmt.telemetry tel ON tel.agent_id = a.agent_id
		GROUP BY a.agent_id, a.hostname, a.ip_address, a.status, a.last_seen
		ORDER BY a.last_seen DESC`)
	if err != nil {
		return nil, fmt.Errorf("dashboard agents: %w", err)
	}
	defer rows.Close()

	var out []AgentDashRow
	for rows.Next() {
		var r AgentDashRow
		if err := rows.Scan(
			&r.AgentID, &r.Hostname, &r.IPAddress, &r.Status, &r.LastSeen,
			&r.PendingTasks, &r.TelemetryCount,
		); err != nil {
			return nil, fmt.Errorf("dashboard agents scan: %w", err)
		}
		out = append(out, r)
	}
	return out, rows.Err()
}

// ListTasks returns tasks for the queue panel.  statusFilter is optional;
// passing "" or "all" returns every status.
func (r *DashboardRepository) ListTasks(ctx context.Context, statusFilter string, limit int) ([]TaskDashRow, error) {
	var (
		rows *sql.Rows
		err  error
	)
	base := `
		SELECT task_id, agent_id, command_type, status, created_at, expires_at, retry_count, max_retries
		FROM endpoint_mgmt.tasks`

	if statusFilter != "" && statusFilter != "all" {
		rows, err = r.db.QueryContext(ctx,
			base+` WHERE status = $1 ORDER BY created_at DESC LIMIT $2`,
			statusFilter, limit)
	} else {
		rows, err = r.db.QueryContext(ctx,
			base+` ORDER BY created_at DESC LIMIT $1`,
			limit)
	}
	if err != nil {
		return nil, fmt.Errorf("dashboard tasks: %w", err)
	}
	defer rows.Close()

	var out []TaskDashRow
	for rows.Next() {
		var t TaskDashRow
		if err := rows.Scan(
			&t.TaskID, &t.AgentID, &t.CommandType, &t.Status,
			&t.CreatedAt, &t.ExpiresAt, &t.RetryCount, &t.MaxRetries,
		); err != nil {
			return nil, fmt.Errorf("dashboard tasks scan: %w", err)
		}
		out = append(out, t)
	}
	return out, rows.Err()
}

// ListRecentTelemetry returns the latest telemetry entries across all agents.
func (r *DashboardRepository) ListRecentTelemetry(ctx context.Context, limit int) ([]models.Telemetry, error) {
	rows, err := r.db.QueryContext(ctx, `
		SELECT log_id, agent_id, task_id, log_type, result_data, received_at, processed
		FROM endpoint_mgmt.telemetry
		ORDER BY received_at DESC
		LIMIT $1`,
		limit,
	)
	if err != nil {
		return nil, fmt.Errorf("dashboard telemetry: %w", err)
	}
	defer rows.Close()

	var out []models.Telemetry
	for rows.Next() {
		var t models.Telemetry
		if err := rows.Scan(
			&t.LogID, &t.AgentID, &t.TaskID, &t.LogType,
			&t.ResultData, &t.ReceivedAt, &t.Processed,
		); err != nil {
			return nil, fmt.Errorf("dashboard telemetry scan: %w", err)
		}
		out = append(out, t)
	}
	return out, rows.Err()
}
