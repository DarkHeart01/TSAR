-- Create schema
CREATE SCHEMA IF NOT EXISTS endpoint_mgmt;

-- Agents Table: Tracks registered endpoints/agents
CREATE TABLE IF NOT EXISTS endpoint_mgmt.agents (
    agent_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    hostname VARCHAR(255) NOT NULL,
    ip_address INET NOT NULL,
    auth_token VARCHAR(512) NOT NULL UNIQUE,
    status VARCHAR(50) NOT NULL DEFAULT 'offline' CHECK (status IN ('online', 'offline', 'inactive')),
    last_seen TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    metadata JSONB DEFAULT '{}'::jsonb
);

CREATE INDEX idx_agents_status ON endpoint_mgmt.agents(status);
CREATE INDEX idx_agents_last_seen ON endpoint_mgmt.agents(last_seen);
CREATE INDEX idx_agents_hostname ON endpoint_mgmt.agents(hostname);

-- Tasks Table: Queue and tracking for remote commands
CREATE TABLE IF NOT EXISTS endpoint_mgmt.tasks (
    task_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    agent_id UUID NOT NULL REFERENCES endpoint_mgmt.agents(agent_id) ON DELETE CASCADE,
    command_type VARCHAR(100) NOT NULL,
    payload JSONB NOT NULL,
    status VARCHAR(50) NOT NULL DEFAULT 'pending' CHECK (status IN ('pending', 'sent', 'executed', 'failed', 'expired')),
    result_data JSONB,
    error_message TEXT,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    sent_at TIMESTAMP WITH TIME ZONE,
    executed_at TIMESTAMP WITH TIME ZONE,
    expires_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP + INTERVAL '24 hours',
    retry_count INT DEFAULT 0 CHECK (retry_count >= 0),
    max_retries INT DEFAULT 3
);

CREATE INDEX idx_tasks_agent_id ON endpoint_mgmt.tasks(agent_id);
CREATE INDEX idx_tasks_status ON endpoint_mgmt.tasks(status);
CREATE INDEX idx_tasks_created_at ON endpoint_mgmt.tasks(created_at DESC);
CREATE INDEX idx_tasks_expires_at ON endpoint_mgmt.tasks(expires_at);

-- Telemetry Table: Logs and metrics from agents
CREATE TABLE IF NOT EXISTS endpoint_mgmt.telemetry (
    log_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    agent_id UUID NOT NULL REFERENCES endpoint_mgmt.agents(agent_id) ON DELETE CASCADE,
    task_id UUID REFERENCES endpoint_mgmt.tasks(task_id) ON DELETE SET NULL,
    log_type VARCHAR(50) NOT NULL DEFAULT 'metrics' CHECK (log_type IN ('metrics', 'event', 'error', 'heartbeat')),
    result_data JSONB NOT NULL,
    received_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    processed BOOLEAN DEFAULT FALSE
);

CREATE INDEX idx_telemetry_agent_id ON endpoint_mgmt.telemetry(agent_id);
CREATE INDEX idx_telemetry_received_at ON endpoint_mgmt.telemetry(received_at DESC);
CREATE INDEX idx_telemetry_log_type ON endpoint_mgmt.telemetry(log_type);
CREATE INDEX idx_telemetry_processed ON endpoint_mgmt.telemetry(processed);

-- Audit Log Table: Track all sensitive operations
CREATE TABLE IF NOT EXISTS endpoint_mgmt.audit_logs (
    audit_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    action VARCHAR(100) NOT NULL,
    resource_type VARCHAR(50),
    resource_id UUID,
    actor_ip INET,
    details JSONB DEFAULT '{}'::jsonb,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_audit_logs_created_at ON endpoint_mgmt.audit_logs(created_at DESC);
CREATE INDEX idx_audit_logs_resource_type ON endpoint_mgmt.audit_logs(resource_type, resource_id);

-- Auto-update timestamp function
CREATE OR REPLACE FUNCTION endpoint_mgmt.update_updated_at()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trigger_agents_updated_at
BEFORE UPDATE ON endpoint_mgmt.agents
FOR EACH ROW
EXECUTE FUNCTION endpoint_mgmt.update_updated_at();
