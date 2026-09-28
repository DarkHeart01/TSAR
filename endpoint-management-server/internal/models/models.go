package models

import (
	"database/sql/driver"
	"encoding/json"
	"time"

	"github.com/google/uuid"
)

// Agent represents a registered endpoint/device
type Agent struct {
	AgentID   uuid.UUID      `db:"agent_id" json:"agent_id"`
	Hostname  string         `db:"hostname" json:"hostname"`
	IPAddress string         `db:"ip_address" json:"ip_address"`
	AuthToken string         `db:"auth_token" json:"-"`
	Status    string         `db:"status" json:"status"` // online, offline, inactive
	LastSeen  time.Time      `db:"last_seen" json:"last_seen"`
	CreatedAt time.Time      `db:"created_at" json:"created_at"`
	UpdatedAt time.Time      `db:"updated_at" json:"updated_at"`
	Metadata  json.RawMessage `db:"metadata" json:"metadata"`
}

// RegisterAgentRequest payload for agent registration
type RegisterAgentRequest struct {
	Hostname  string                 `json:"hostname" binding:"required"`
	IPAddress string                 `json:"ip_address" binding:"required"`
	Metadata  map[string]interface{} `json:"metadata"`
}

// RegisterAgentResponse returns auth token
type RegisterAgentResponse struct {
	AgentID   uuid.UUID `json:"agent_id"`
	AuthToken string    `json:"auth_token"`
	ExpiresIn int       `json:"expires_in"`
}

// Task represents a command to be executed on an agent
type Task struct {
	TaskID       uuid.UUID       `db:"task_id" json:"task_id"`
	AgentID      uuid.UUID       `db:"agent_id" json:"agent_id"`
	CommandType  string          `db:"command_type" json:"command_type"`
	Payload      json.RawMessage `db:"payload" json:"payload"`
	Status       string          `db:"status" json:"status"` // pending, sent, executed, failed, expired
	ResultData   json.RawMessage `db:"result_data" json:"result_data"`
	ErrorMessage string          `db:"error_message" json:"error_message"`
	CreatedAt    time.Time       `db:"created_at" json:"created_at"`
	SentAt       *time.Time      `db:"sent_at" json:"sent_at"`
	ExecutedAt   *time.Time      `db:"executed_at" json:"executed_at"`
	ExpiresAt    time.Time       `db:"expires_at" json:"expires_at"`
	RetryCount   int             `db:"retry_count" json:"retry_count"`
	MaxRetries   int             `db:"max_retries" json:"max_retries"`
}

// PollResponse sent to agents checking for tasks
type PollResponse struct {
	Tasks     []Task `json:"tasks"`
	Timestamp string `json:"timestamp"`
}

// Telemetry represents logs from agents
type Telemetry struct {
	LogID       uuid.UUID       `db:"log_id" json:"log_id"`
	AgentID     uuid.UUID       `db:"agent_id" json:"agent_id"`
	TaskID      *uuid.UUID      `db:"task_id" json:"task_id"`
	LogType     string          `db:"log_type" json:"log_type"` // metrics, event, error, heartbeat
	ResultData  json.RawMessage `db:"result_data" json:"result_data"`
	ReceivedAt  time.Time       `db:"received_at" json:"received_at"`
	Processed   bool            `db:"processed" json:"processed"`
}

// TelemetryRequest payload from agents
type TelemetryRequest struct {
	TaskID     *uuid.UUID             `json:"task_id"`
	LogType    string                 `json:"log_type" binding:"required"`
	Data       map[string]interface{} `json:"data" binding:"required"`
	Timestamp  string                 `json:"timestamp"`
}

// AuditLog tracks sensitive operations
type AuditLog struct {
	AuditID    uuid.UUID       `db:"audit_id" json:"audit_id"`
	Action     string          `db:"action" json:"action"`
	ResourceType string        `db:"resource_type" json:"resource_type"`
	ResourceID *uuid.UUID      `db:"resource_id" json:"resource_id"`
	ActorIP    string          `db:"actor_ip" json:"actor_ip"`
	Details    json.RawMessage `db:"details" json:"details"`
	CreatedAt  time.Time       `db:"created_at" json:"created_at"`
}

// NullTime wraps time.Time to handle NULL values
type NullTime struct {
	Time  time.Time
	Valid bool
}

func (nt NullTime) Value() (driver.Value, error) {
	if !nt.Valid {
		return nil, nil
	}
	return nt.Time, nil
}

func (nt *NullTime) Scan(value interface{}) error {
	if value == nil {
		nt.Valid = false
		return nil
	}
	nt.Time = value.(time.Time)
	nt.Valid = true
	return nil
}

// Operator represents a human operator who manages agents via the control plane.
type Operator struct {
	OperatorID   uuid.UUID `json:"operator_id"`
	Username     string    `json:"username"`
	PasswordHash string    `json:"-"`
	CreatedAt    time.Time `json:"created_at"`
}

// OperatorLoginRequest is the body for POST /api/v1/operator/login.
type OperatorLoginRequest struct {
	Username string `json:"username" binding:"required"`
	Password string `json:"password" binding:"required"`
}

// OperatorLoginResponse returns the signed JWT and when it expires.
type OperatorLoginResponse struct {
	Token     string    `json:"token"`
	ExpiresAt time.Time `json:"expires_at"`
}

// PayloadManifest is stored in Redis and served as a DNS TXT record so
// agents know how many chunks to fetch and can verify integrity.
type PayloadManifest struct {
	TotalChunks int    `json:"total_chunks"`
	TotalSize   int    `json:"total_size"`
	SHA256      string `json:"sha256"`
	Version     string `json:"version"`
	UploadedAt  string `json:"uploaded_at"`
}

// NullUUID wraps uuid.UUID to handle NULL values
type NullUUID struct {
	UUID  uuid.UUID
	Valid bool
}

func (nu NullUUID) Value() (driver.Value, error) {
	if !nu.Valid {
		return nil, nil
	}
	return nu.UUID[:], nil
}

func (nu *NullUUID) Scan(value interface{}) error {
	if value == nil {
		nu.Valid = false
		return nil
	}
	nu.UUID = value.(uuid.UUID)
	nu.Valid = true
	return nil
}
