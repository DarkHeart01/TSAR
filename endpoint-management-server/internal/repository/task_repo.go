package repository

import (
	"context"
	"database/sql"
	"encoding/json"
	"fmt"

	"github.com/google/uuid"
	"endpoint-management-server/internal/models"
)

type TaskRepository struct {
	db *sql.DB
}

func NewTaskRepository(db *sql.DB) *TaskRepository {
	return &TaskRepository{db: db}
}

func (r *TaskRepository) Create(ctx context.Context, agentID uuid.UUID, commandType string, payload map[string]interface{}) (*models.Task, error) {
	payloadBytes, err := json.Marshal(payload)
	if err != nil {
		return nil, err
	}

	task := &models.Task{}
	query := `
		INSERT INTO endpoint_mgmt.tasks (agent_id, command_type, payload, status)
		VALUES ($1, $2, $3, 'pending')
		RETURNING task_id, agent_id, command_type, payload, status, created_at, expires_at, retry_count, max_retries`

	row := r.db.QueryRowContext(ctx, query, agentID, commandType, payloadBytes)
	err = row.Scan(&task.TaskID, &task.AgentID, &task.CommandType, &task.Payload,
		&task.Status, &task.CreatedAt, &task.ExpiresAt, &task.RetryCount, &task.MaxRetries)
	if err != nil {
		return nil, err
	}
	return task, nil
}

func (r *TaskRepository) MarkSent(ctx context.Context, taskID uuid.UUID) error {
	query := `
		UPDATE endpoint_mgmt.tasks
		SET status = 'sent', sent_at = CURRENT_TIMESTAMP
		WHERE task_id = $1`
	_, err := r.db.ExecContext(ctx, query, taskID)
	return err
}

func (r *TaskRepository) MarkExecuted(ctx context.Context, taskID uuid.UUID, resultData json.RawMessage) error {
	query := `
		UPDATE endpoint_mgmt.tasks
		SET status = 'executed', executed_at = CURRENT_TIMESTAMP, result_data = $2
		WHERE task_id = $1`
	_, err := r.db.ExecContext(ctx, query, taskID, resultData)
	return err
}

func (r *TaskRepository) MarkFailed(ctx context.Context, taskID uuid.UUID, errorMessage string) error {
	query := `
		UPDATE endpoint_mgmt.tasks
		SET status = 'failed', error_message = $2, retry_count = retry_count + 1
		WHERE task_id = $1`
	_, err := r.db.ExecContext(ctx, query, taskID, errorMessage)
	return err
}

func (r *TaskRepository) GetByID(ctx context.Context, taskID uuid.UUID) (*models.Task, error) {
	task := &models.Task{}
	query := `
		SELECT task_id, agent_id, command_type, payload, status, result_data, error_message,
		       created_at, sent_at, executed_at, expires_at, retry_count, max_retries
		FROM endpoint_mgmt.tasks
		WHERE task_id = $1`

	row := r.db.QueryRowContext(ctx, query, taskID)
	err := row.Scan(&task.TaskID, &task.AgentID, &task.CommandType, &task.Payload, &task.Status,
		&task.ResultData, &task.ErrorMessage, &task.CreatedAt, &task.SentAt, &task.ExecutedAt,
		&task.ExpiresAt, &task.RetryCount, &task.MaxRetries)
	if err != nil {
		return nil, err
	}
	return task, nil
}

// IncrementRetryCount atomically bumps retry_count and returns the new value.
// Called in the poll handler before each task is dispatched so we know
// how many delivery attempts have been made.
func (r *TaskRepository) IncrementRetryCount(ctx context.Context, taskID uuid.UUID) (int, error) {
	var newCount int
	err := r.db.QueryRowContext(ctx, `
		UPDATE endpoint_mgmt.tasks
		SET retry_count = retry_count + 1
		WHERE task_id = $1
		RETURNING retry_count`,
		taskID,
	).Scan(&newCount)
	if err != nil {
		return 0, fmt.Errorf("increment retry count: %w", err)
	}
	return newCount, nil
}

// MarkFailedFinal sets status='failed' without touching retry_count
// (IncrementRetryCount already bumped it before this is called).
func (r *TaskRepository) MarkFailedFinal(ctx context.Context, taskID uuid.UUID, msg string) error {
	_, err := r.db.ExecContext(ctx, `
		UPDATE endpoint_mgmt.tasks
		SET status = 'failed', error_message = $2
		WHERE task_id = $1`,
		taskID, msg,
	)
	if err != nil {
		return fmt.Errorf("mark failed final: %w", err)
	}
	return nil
}

// ExpireStale batch-updates pending tasks whose expires_at has passed.
// Returns the UUIDs of every task that was expired so callers can write
// individual audit log entries.
func (r *TaskRepository) ExpireStale(ctx context.Context) ([]uuid.UUID, error) {
	rows, err := r.db.QueryContext(ctx, `
		UPDATE endpoint_mgmt.tasks
		SET status = 'expired'
		WHERE status = 'pending' AND expires_at < NOW()
		RETURNING task_id`)
	if err != nil {
		return nil, fmt.Errorf("expire stale tasks: %w", err)
	}
	defer rows.Close()

	var ids []uuid.UUID
	for rows.Next() {
		var id uuid.UUID
		if err := rows.Scan(&id); err != nil {
			return nil, fmt.Errorf("expire stale scan: %w", err)
		}
		ids = append(ids, id)
	}
	return ids, rows.Err()
}

func (r *TaskRepository) ListByAgent(ctx context.Context, agentID uuid.UUID, limit int) ([]models.Task, error) {
	query := `
		SELECT task_id, agent_id, command_type, payload, status, result_data, error_message,
		       created_at, sent_at, executed_at, expires_at, retry_count, max_retries
		FROM endpoint_mgmt.tasks
		WHERE agent_id = $1
		ORDER BY created_at DESC
		LIMIT $2`

	rows, err := r.db.QueryContext(ctx, query, agentID, limit)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var tasks []models.Task
	for rows.Next() {
		var t models.Task
		if err := rows.Scan(&t.TaskID, &t.AgentID, &t.CommandType, &t.Payload, &t.Status,
			&t.ResultData, &t.ErrorMessage, &t.CreatedAt, &t.SentAt, &t.ExecutedAt,
			&t.ExpiresAt, &t.RetryCount, &t.MaxRetries); err != nil {
			return nil, err
		}
		tasks = append(tasks, t)
	}
	return tasks, rows.Err()
}
