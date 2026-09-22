# Endpoint Management Server

High-concurrency remote endpoint management backend: Go (Gin) + PostgreSQL + Redis, deployed behind Nginx TLS termination.

## Layout

```
cmd/server/main.go           Entry point, DI wiring, graceful shutdown, stale-agent sweeper
internal/db                  PostgreSQL connection pool
internal/models              Shared structs (Agent, Task, Telemetry, requests/responses)
internal/repository          SQL data-access layer per table
internal/queue               Redis-backed per-agent task queues
internal/auth                Bearer token generation
internal/middleware          Auth, rate limiting, security headers
internal/handlers            HTTP handlers
internal/router              Route wiring
migrations/001_init_schema.sql   Schema: agents, tasks, telemetry, audit_logs
deploy/nginx.conf             TLS 1.3 reverse proxy + rate limiting
docker-compose.yml             postgres, redis, api (3 replicas), nginx
```

## Endpoints

| Method | Path | Auth | Purpose |
|---|---|---|---|
| POST | `/api/v1/agent/register` | none (public, rate-limited) | Register a new agent, returns bearer token **once** |
| GET | `/api/v1/agent/poll` | Bearer | Agent drains its pending task queue |
| POST | `/api/v1/agent/telemetry` | Bearer | Agent submits logs/metrics/results |
| POST | `/api/v1/agent/:agent_id/tasks` | operator-only (see note) | Enqueue a task for an agent |

**Note:** `/tasks` is intentionally left without the agent bearer-token middleware — it's the operator/control-plane path and needs its own auth (mTLS, admin JWT, or an internal-only network segment). Wire that before exposing it publicly.

## Security notes

- Bearer tokens are generated with `crypto/rand`, returned once, and only their SHA-256 hash is stored (`internal/middleware/auth.go`, `internal/auth/token.go`).
- TLS 1.3 termination happens in `deploy/nginx.conf`; drop your cert/key into `deploy/certs/`.
- Redis-backed sliding-window rate limiting per client IP (`internal/middleware/auth.go: RateLimiter`), plus a second layer at the Nginx `limit_req_zone`.
- Postgres and Redis have no published host ports in `docker-compose.yml` — only Nginx is internet-facing.
- `agent:lock:<id>` in Redis prevents a duplicate/retried poll from double-draining a queue.

## Running locally

```bash
cp .env.example .env   # fill in real secrets
# generate a self-signed cert for local testing, or drop real certs into deploy/certs/
docker compose up --build
```

## Gaps to close before production

- Operator auth for the task-creation endpoint (currently unauthenticated by design — see note above).
- Token rotation/revocation endpoint (schema supports it; no handler yet).
- Structured logging/metrics export (Prometheus) — logging is currently Gin's default logger.
- The stale-agent sweeper and `MarkStaleOffline` interval (120s) should be tuned to your agents' real poll cadence.
