# Endpoint Management Server

The endpoint management server is the central control plane for the JOCKY framework. It handles agent registration, task dispatch, telemetry ingestion, operator authentication, payload delivery, and audit logging. Built in Go (Gin) with PostgreSQL for persistence and Redis for task queuing.

---

## Architecture

```
Internet
    │  TLS 1.3
    ▼
┌──────────────────────────────────────────────────────┐
│  Nginx reverse proxy                                 │
│  TLS termination, rate limiting (limit_req_zone)     │
└──────────────────────┬───────────────────────────────┘
                       │
                       ▼
┌──────────────────────────────────────────────────────┐
│  Go / Gin HTTP server  (3 replicas)                  │
│  ├── Agent handlers      /api/v1/agent/...           │
│  ├── Operator handlers   /api/v1/operator/...        │
│  ├── Payload handlers    /api/v1/operator/payload/.. │
│  └── Dashboard handlers  /api/v1/dashboard/...       │
└────────┬───────────────────────────┬─────────────────┘
         │                           │
         ▼                           ▼
  ┌─────────────┐            ┌──────────────┐
  │ PostgreSQL  │            │    Redis     │
  │ agents      │            │ task queues  │
  │ tasks       │            │ rate limits  │
  │ telemetry   │            │ payload cache│
  │ audit_logs  │            │ lease locks  │
  │ operators   │            └──────────────┘
  └─────────────┘
```

---

## API Endpoints

### Agent endpoints

| Method | Path | Auth | Description |
|---|---|---|---|
| `POST` | `/api/v1/agent/register` | None (rate-limited) | Register a new agent; returns a bearer token once |
| `GET` | `/api/v1/agent/poll` | Bearer | Drain the agent's pending task queue |
| `POST` | `/api/v1/agent/telemetry` | Bearer | Submit logs, metrics, and task results |

### Operator endpoints

| Method | Path | Auth | Description |
|---|---|---|---|
| `POST` | `/api/v1/operator/login` | — | Authenticate; returns a signed JWT |
| `POST` | `/api/v1/operator/agents/:id/tasks` | Operator JWT | Enqueue a task for a specific agent |
| `POST` | `/api/v1/operator/payload/upload` | Operator JWT | Encrypt, chunk, and write a payload to the zone file |
| `GET` | `/api/v1/operator/payload/status` | Operator JWT | Read the payload manifest from Redis |
| `POST` | `/api/v1/operator/payload/webhook` | HMAC-signed | GitHub Actions CI/CD trigger |

### Payload delivery (agent-facing)

| Method | Path | Auth | Description |
|---|---|---|---|
| `GET` | `/api/v1/payload/chunk/:index` | Bearer | Agent retrieves an encrypted payload chunk directly over HTTPS |

### Dashboard

| Method | Path | Auth | Description |
|---|---|---|---|
| `GET` | `/api/v1/dashboard/summary` | Operator JWT | Agent counts, task stats, recent activity |

---

## Authentication Model

**Agent tokens** — generated with `crypto/rand`, returned once at registration, stored as a SHA-256 hash. A Redis lease lock (`agent:lock:<id>`) prevents concurrent poll requests from double-draining the same queue.

**Operator JWTs** — signed with a configurable HMAC secret (`OPERATOR_SECRET`). The initial `admin` operator is seeded from `ADMIN_PASSWORD` on first boot if the operators table is empty.

---

## Payload Delivery Pipeline

The payload handler implements a DoH-style (DNS-over-HTTPS) delivery mechanism:

1. Operator uploads a binary via `POST /api/v1/operator/payload/upload`
2. Server encrypts the payload with AES-256, splits it into fixed-size chunks, and writes a CoreDNS zone file via `internal/c2/zone.go`
3. Agents can retrieve chunks either over DNS TXT records (via CoreDNS) or directly via the HTTPS chunk endpoint as a fallback
4. Manifest (chunk count, hashes) is cached in Redis with a 24-hour TTL

---

## Background Sweepers

Two goroutines run continuously:

| Sweeper | Interval | Effect |
|---|---|---|
| Stale agent sweeper | 30s | Marks agents `OFFLINE` if they have not polled within 120s |
| Task expiry sweeper | 60s | Marks stale tasks `EXPIRED` and writes an audit log entry per task |

---

## Security Properties

- TLS 1.3 only; cipher suite configured in `deploy/nginx.conf`
- Bearer tokens are never stored in plaintext — only their SHA-256 hash is persisted
- PostgreSQL and Redis have no published host ports; only Nginx is internet-facing
- Sliding-window rate limiting per client IP at both the Nginx and application layers
- All database writes go through parameterised queries in the repository layer

---

## Running Locally

```bash
cd endpoint-management-server
cp .env.example .env   # fill in secrets
docker compose up --build
```

The compose file brings up PostgreSQL, Redis, three Go API replicas, and Nginx. Drop TLS certificates into `deploy/certs/` or generate a self-signed cert for local testing.

---

## Directory Layout

```
endpoint-management-server/
├── cmd/server/main.go              Entry point, DI wiring, graceful shutdown
├── internal/
│   ├── auth/                       Bearer token generation + JWT signing
│   ├── c2/                         Payload encryption and zone file generation
│   ├── config/                     Environment-based configuration
│   ├── db/                         PostgreSQL connection pool
│   ├── handlers/                   HTTP handler layer
│   ├── middleware/                  Auth, rate limiting, security headers
│   ├── models/                     Shared struct definitions
│   ├── queue/                      Redis-backed per-agent task queues
│   ├── repository/                 SQL data-access layer (one file per table)
│   └── router/                     Route wiring
├── migrations/
│   ├── 001_init_schema.sql         agents, tasks, telemetry, audit_logs
│   ├── 002_operators.sql           operators table
│   └── 003_audit_log_index.sql     Index on audit_logs
├── coredns/
│   └── Corefile                    CoreDNS config for DNS TXT payload delivery
├── deploy/
│   └── nginx.conf                  TLS 1.3 reverse proxy + rate limiting
├── docker-compose.yml
├── .env.example
├── go.mod
└── go.sum
```
