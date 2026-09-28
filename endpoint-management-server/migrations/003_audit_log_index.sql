-- Additional indexes that make the audit log dashboard panel fast.
-- The base table and primary indexes were created in 001_init_schema.sql.
CREATE INDEX IF NOT EXISTS idx_audit_logs_action
    ON endpoint_mgmt.audit_logs(action);

CREATE INDEX IF NOT EXISTS idx_audit_logs_actor_ip
    ON endpoint_mgmt.audit_logs(actor_ip);
