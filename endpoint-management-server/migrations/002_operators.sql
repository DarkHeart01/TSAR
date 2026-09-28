-- Operator accounts for the JOCKY C2 control plane.
-- Operators authenticate via POST /api/v1/operator/login and receive
-- a short-lived JWT.  Only bcrypt hashes are persisted; raw passwords
-- are never stored.
CREATE TABLE IF NOT EXISTS endpoint_mgmt.operators (
    operator_id   UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    username      TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    created_at    TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_operators_username ON endpoint_mgmt.operators(username);
