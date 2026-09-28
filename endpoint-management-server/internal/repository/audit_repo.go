package repository

import (
	"context"
	"database/sql"
	"encoding/json"
	"fmt"

	"github.com/google/uuid"

	"endpoint-management-server/internal/models"
)

type AuditRepository struct {
	db *sql.DB
}

func NewAuditRepository(db *sql.DB) *AuditRepository {
	return &AuditRepository{db: db}
}

// Write records a single audit event.  actorIP is the client IP from the
// HTTP request; details holds any operation-specific context (actor_id,
// command type, etc.) as JSONB.  Errors are returned but callers
// typically log and continue — a failed audit write must not block the
// primary operation.
func (r *AuditRepository) Write(
	ctx context.Context,
	action, resourceType string,
	resourceID *uuid.UUID,
	actorIP string,
	details map[string]interface{},
) error {
	detailsBytes, err := json.Marshal(details)
	if err != nil {
		return fmt.Errorf("audit write: marshal details: %w", err)
	}
	_, err = r.db.ExecContext(ctx, `
		INSERT INTO endpoint_mgmt.audit_logs
		    (action, resource_type, resource_id, actor_ip, details)
		VALUES ($1, $2, $3, $4, $5)`,
		action, resourceType, resourceID, actorIP, detailsBytes,
	)
	if err != nil {
		return fmt.Errorf("audit write: %w", err)
	}
	return nil
}

// ListRecent returns the most recent audit entries for the dashboard.
func (r *AuditRepository) ListRecent(ctx context.Context, limit int) ([]models.AuditLog, error) {
	rows, err := r.db.QueryContext(ctx, `
		SELECT audit_id, action, resource_type, resource_id, actor_ip, details, created_at
		FROM endpoint_mgmt.audit_logs
		ORDER BY created_at DESC
		LIMIT $1`,
		limit,
	)
	if err != nil {
		return nil, fmt.Errorf("audit list recent: %w", err)
	}
	defer rows.Close()

	var logs []models.AuditLog
	for rows.Next() {
		var l models.AuditLog
		if err := rows.Scan(
			&l.AuditID, &l.Action, &l.ResourceType, &l.ResourceID,
			&l.ActorIP, &l.Details, &l.CreatedAt,
		); err != nil {
			return nil, fmt.Errorf("audit list scan: %w", err)
		}
		logs = append(logs, l)
	}
	return logs, rows.Err()
}
