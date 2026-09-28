package repository

import (
	"context"
	"database/sql"
	"fmt"

	"github.com/google/uuid"

	"endpoint-management-server/internal/models"
)

type OperatorRepository struct {
	db *sql.DB
}

func NewOperatorRepository(db *sql.DB) *OperatorRepository {
	return &OperatorRepository{db: db}
}

// Create inserts a new operator row.  passwordHash must already be a
// bcrypt digest — raw passwords are never passed into this layer.
func (r *OperatorRepository) Create(ctx context.Context, username, passwordHash string) (*models.Operator, error) {
	op := &models.Operator{}
	err := r.db.QueryRowContext(ctx, `
		INSERT INTO endpoint_mgmt.operators (username, password_hash)
		VALUES ($1, $2)
		RETURNING operator_id, username, created_at`,
		username, passwordHash,
	).Scan(&op.OperatorID, &op.Username, &op.CreatedAt)
	if err != nil {
		return nil, fmt.Errorf("operator create: %w", err)
	}
	return op, nil
}

// GetByUsername fetches an operator including the password hash so the
// login handler can verify the submitted password with bcrypt.
func (r *OperatorRepository) GetByUsername(ctx context.Context, username string) (*models.Operator, error) {
	op := &models.Operator{}
	err := r.db.QueryRowContext(ctx, `
		SELECT operator_id, username, password_hash, created_at
		FROM endpoint_mgmt.operators
		WHERE username = $1`,
		username,
	).Scan(&op.OperatorID, &op.Username, &op.PasswordHash, &op.CreatedAt)
	if err != nil {
		return nil, fmt.Errorf("operator get by username: %w", err)
	}
	return op, nil
}

// Count returns the number of operator rows, used at startup to decide
// whether to seed the initial admin account.
func (r *OperatorRepository) Count(ctx context.Context) (int64, error) {
	var n int64
	err := r.db.QueryRowContext(ctx, `SELECT COUNT(*) FROM endpoint_mgmt.operators`).Scan(&n)
	if err != nil {
		return 0, fmt.Errorf("operator count: %w", err)
	}
	return n, nil
}

// GetByID fetches an operator by primary key (used for token validation
// if we ever want to revoke individual operators).
func (r *OperatorRepository) GetByID(ctx context.Context, id uuid.UUID) (*models.Operator, error) {
	op := &models.Operator{}
	err := r.db.QueryRowContext(ctx, `
		SELECT operator_id, username, created_at
		FROM endpoint_mgmt.operators
		WHERE operator_id = $1`,
		id,
	).Scan(&op.OperatorID, &op.Username, &op.CreatedAt)
	if err != nil {
		return nil, fmt.Errorf("operator get by id: %w", err)
	}
	return op, nil
}
