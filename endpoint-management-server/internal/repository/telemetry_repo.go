package repository

import (
	"context"
	"database/sql"
	"encoding/json"

	"github.com/google/uuid"
	"endpoint-management-server/internal/models"
)

type TelemetryRepository struct {
	db *sql.DB
}

func NewTelemetryRepository(db *sql.DB) *TelemetryRepository {
	return &TelemetryRepository{db: db}
}

func (r *TelemetryRepository) Create(ctx context.Context, agentID uuid.UUID, taskID *uuid.UUID, logType string, data map[string]interface{}) (*models.Telemetry, error) {
	dataBytes, err := json.Marshal(data)
	if err != nil {
		return nil, err
	}

	log := &models.Telemetry{}
	query := `
		INSERT INTO endpoint_mgmt.telemetry (agent_id, task_id, log_type, result_data)
		VALUES ($1, $2, $3, $4)
		RETURNING log_id, agent_id, task_id, log_type, result_data, received_at, processed`

	row := r.db.QueryRowContext(ctx, query, agentID, taskID, logType, dataBytes)
	err = row.Scan(&log.LogID, &log.AgentID, &log.TaskID, &log.LogType, &log.ResultData,
		&log.ReceivedAt, &log.Processed)
	if err != nil {
		return nil, err
	}
	return log, nil
}

func (r *TelemetryRepository) ListByAgent(ctx context.Context, agentID uuid.UUID, limit int) ([]models.Telemetry, error) {
	query := `
		SELECT log_id, agent_id, task_id, log_type, result_data, received_at, processed
		FROM endpoint_mgmt.telemetry
		WHERE agent_id = $1
		ORDER BY received_at DESC
		LIMIT $2`

	rows, err := r.db.QueryContext(ctx, query, agentID, limit)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var logs []models.Telemetry
	for rows.Next() {
		var t models.Telemetry
		if err := rows.Scan(&t.LogID, &t.AgentID, &t.TaskID, &t.LogType, &t.ResultData,
			&t.ReceivedAt, &t.Processed); err != nil {
			return nil, err
		}
		logs = append(logs, t)
	}
	return logs, rows.Err()
}
