package repository

import (
	"context"
	"database/sql"
	"encoding/json"

	"github.com/google/uuid"
	"endpoint-management-server/internal/models"
)

type AgentRepository struct {
	db *sql.DB
}

func NewAgentRepository(db *sql.DB) *AgentRepository {
	return &AgentRepository{db: db}
}

func (r *AgentRepository) Create(ctx context.Context, hostname, ipAddress, authToken string, metadata map[string]interface{}) (*models.Agent, error) {
	metaBytes, err := json.Marshal(metadata)
	if err != nil {
		return nil, err
	}

	agent := &models.Agent{}
	query := `
		INSERT INTO endpoint_mgmt.agents (hostname, ip_address, auth_token, metadata, status, last_seen)
		VALUES ($1, $2, $3, $4, 'online', CURRENT_TIMESTAMP)
		RETURNING agent_id, hostname, ip_address, status, last_seen, created_at, updated_at, metadata`

	row := r.db.QueryRowContext(ctx, query, hostname, ipAddress, authToken, metaBytes)
	err = row.Scan(&agent.AgentID, &agent.Hostname, &agent.IPAddress, &agent.Status,
		&agent.LastSeen, &agent.CreatedAt, &agent.UpdatedAt, &agent.Metadata)
	if err != nil {
		return nil, err
	}
	return agent, nil
}

func (r *AgentRepository) GetByToken(ctx context.Context, authToken string) (*models.Agent, error) {
	agent := &models.Agent{}
	query := `
		SELECT agent_id, hostname, ip_address, auth_token, status, last_seen, created_at, updated_at, metadata
		FROM endpoint_mgmt.agents
		WHERE auth_token = $1`

	row := r.db.QueryRowContext(ctx, query, authToken)
	err := row.Scan(&agent.AgentID, &agent.Hostname, &agent.IPAddress, &agent.AuthToken,
		&agent.Status, &agent.LastSeen, &agent.CreatedAt, &agent.UpdatedAt, &agent.Metadata)
	if err != nil {
		return nil, err
	}
	return agent, nil
}

func (r *AgentRepository) GetByID(ctx context.Context, agentID uuid.UUID) (*models.Agent, error) {
	agent := &models.Agent{}
	query := `
		SELECT agent_id, hostname, ip_address, auth_token, status, last_seen, created_at, updated_at, metadata
		FROM endpoint_mgmt.agents
		WHERE agent_id = $1`

	row := r.db.QueryRowContext(ctx, query, agentID)
	err := row.Scan(&agent.AgentID, &agent.Hostname, &agent.IPAddress, &agent.AuthToken,
		&agent.Status, &agent.LastSeen, &agent.CreatedAt, &agent.UpdatedAt, &agent.Metadata)
	if err != nil {
		return nil, err
	}
	return agent, nil
}

func (r *AgentRepository) UpdateLastSeen(ctx context.Context, agentID uuid.UUID) error {
	query := `
		UPDATE endpoint_mgmt.agents
		SET last_seen = CURRENT_TIMESTAMP, status = 'online'
		WHERE agent_id = $1`
	_, err := r.db.ExecContext(ctx, query, agentID)
	return err
}

func (r *AgentRepository) MarkStaleOffline(ctx context.Context, staleAfterSeconds int) (int64, error) {
	query := `
		UPDATE endpoint_mgmt.agents
		SET status = 'offline'
		WHERE status = 'online' AND last_seen < CURRENT_TIMESTAMP - ($1 || ' seconds')::interval`
	res, err := r.db.ExecContext(ctx, query, staleAfterSeconds)
	if err != nil {
		return 0, err
	}
	return res.RowsAffected()
}

func (r *AgentRepository) List(ctx context.Context, limit, offset int) ([]models.Agent, error) {
	query := `
		SELECT agent_id, hostname, ip_address, status, last_seen, created_at, updated_at, metadata
		FROM endpoint_mgmt.agents
		ORDER BY last_seen DESC
		LIMIT $1 OFFSET $2`

	rows, err := r.db.QueryContext(ctx, query, limit, offset)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var agents []models.Agent
	for rows.Next() {
		var a models.Agent
		if err := rows.Scan(&a.AgentID, &a.Hostname, &a.IPAddress, &a.Status,
			&a.LastSeen, &a.CreatedAt, &a.UpdatedAt, &a.Metadata); err != nil {
			return nil, err
		}
		agents = append(agents, a)
	}
	return agents, rows.Err()
}
