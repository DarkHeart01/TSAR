# Endpoint Management Server

> High-concurrency central management backend for the JOCKY framework. Handles agent registration, task dispatch, telemetry ingestion, operator authentication, payload delivery, and a full audit trail. Built in Go (Gin) with PostgreSQL and Redis, deployed behind Nginx TLS termination.

---

## Overview

The endpoint management server is the control plane that connects all running agents back to the operator. It is designed to handle many simultaneous agent connections without contention — task queues are per-agent, backed by Redis, and poll operations use atomic queue drain to prevent double-delivery. All writes are audited. All tokens are one-time-issued and stored only as hashes.

The server also implements a DoH-style (DNS-over-HTTPS) payload delivery pipeline. Operators upload a binary; the server encrypts it, splits it into chunks, and writes a CoreDNS zone file. Agents can retrieve payload chunks either through DNS TXT record lookups (blending with ordinary DNS traffic) or via a direct HTTPS fallback endpoint.

---

## Architecture

```
Internet
    │  TLS 1.3
    ▼
┌──────────────────────────────────────────────────────────────┐
│  Nginx                                                       │
│  TLS termination (1.3 only)                                  │
│  Rate limiting: limit_req_zone per client IP                 │
│  No published ports for PostgreSQL or Redis                  │
└───────────────────────────┬──────────────────────────────────┘
                            │
                            ▼
┌──────────────────────────────────────────────────────────────┐
│  Go / Gin HTTP server  (3 replicas via docker-compose)       │
│                                                              │
│  ┌────────────────┐ ┌──────────────────┐ ┌────────────────┐ │
│  │ Agent handlers │ │Operator handlers │ │Payload handler │ │
│  │ /api/v1/agent/ │ │/api/v1/operator/ │ │  DoH chunks    │ │
│  └────────────────┘ └──────────────────┘ └────────────────┘ │
│                                                              │
│  ┌────────────────────────────────────────────────────────┐  │
│  │ Background goroutines                                  │  │
│  │  - staleAgentSweeper  (30s)  marks offline agents      │  │
│  │  - taskExpirySweeper  (60s)  expires stale tasks       │  │
│  └────────────────────────────────────────────────────────┘  │
└──────────┬────────────────────────────────────┬──────────────┘
           │                                    │
           ▼                                    ▼
  ┌──────────────────┐                ┌───────────────────────┐
  │   PostgreSQL     │                │        Redis          │
  │                  │                │                       │
  │  agents          │                │  task:queue:<agent_id>│
  │  tasks           │                │  agent:lock:<id>      │
  │  telemetry       │                │  payload:manifest     │
  │  audit_logs      │                │  payload:chunk:<n>    │
  │  operators       │                │  rate limiting        │
  └──────────────────┘                └───────────────────────┘
```

---

## API Endpoints

### Agent Endpoints

Agents interact with these endpoints autonomously after registration.

| Method | Path | Auth | Description |
|---|---|---|---|
| `POST` | `/api/v1/agent/register` | None (rate-limited) | Register a new agent. Returns a bearer token exactly once. |
| `GET` | `/api/v1/agent/poll` | Bearer | Atomically drain the agent's pending task queue. Returns all queued tasks in one response. |
| `POST` | `/api/v1/agent/telemetry` | Bearer | Submit logs, metrics, scan results, and task completion reports. |

### Operator Endpoints

Operators authenticate with a JWT and manage agents through these endpoints.

| Method | Path | Auth | Description |
|---|---|---|---|
| `POST` | `/api/v1/operator/login` | Credentials | Authenticate; returns a signed JWT valid for the configured duration |
| `GET` | `/api/v1/operator/agents` | Operator JWT | List all registered agents with status and last-seen time |
| `POST` | `/api/v1/operator/agents/:id/tasks` | Operator JWT | Enqueue one or more tasks for a specific agent |
| `GET` | `/api/v1/operator/agents/:id/telemetry` | Operator JWT | Retrieve telemetry records submitted by an agent |
| `POST` | `/api/v1/operator/payload/upload` | Operator JWT | Encrypt, chunk, and write a binary to the zone file and Redis |
| `GET` | `/api/v1/operator/payload/status` | Operator JWT | Read the current payload manifest (chunk count, hashes, TTL) |
| `POST` | `/api/v1/operator/payload/webhook` | HMAC-SHA256 | GitHub Actions CI/CD trigger — triggers a new build and auto-uploads the result |

### Payload Delivery (Agent-Facing)

| Method | Path | Auth | Description |
|---|---|---|---|
| `GET` | `/api/v1/payload/chunk/:index` | Bearer | Retrieve a single encrypted payload chunk over HTTPS (fallback from DNS) |

### Dashboard

| Method | Path | Auth | Description |
|---|---|---|---|
| `GET` | `/api/v1/dashboard/summary` | Operator JWT | Aggregate counts: agents online/offline, tasks pending/completed/failed, recent events |

---

## Authentication Model

### Agent Tokens

When an agent registers, the server:

1. Generates a 32-byte cryptographically random token using `crypto/rand`
2. Returns the raw token to the agent **exactly once** — it is never stored in plaintext
3. Stores only the SHA-256 hash of the token in PostgreSQL

On every subsequent request, the agent presents the raw token in the `Authorization: Bearer` header. The server hashes the incoming token and compares against the stored hash. If the token is lost, re-registration is required.

A Redis lease lock (`agent:lock:<id>`) prevents concurrent poll requests from the same agent from double-draining the task queue. The lock is acquired before the drain and released after.

### Operator JWTs

Operators authenticate with a username and bcrypt-hashed password. On successful login, the server issues a JWT signed with the configured `OPERATOR_SECRET`. The JWT payload includes the operator ID and an expiry. All operator-facing endpoints validate the JWT via the `OperatorAuth` middleware before the handler runs.

The initial `admin` operator is seeded from `ADMIN_PASSWORD` on first boot if the operators table is empty.

---

## Payload Delivery Pipeline (DoH)

The payload delivery system is designed to blend agent traffic with ordinary-looking DNS and HTTPS requests.

```
Operator
  │  POST /api/v1/operator/payload/upload  (binary)
  ▼
Server
  1. Receives binary
  2. Encrypts with AES-256-GCM using configured AES key
  3. Splits into fixed-size chunks
  4. Writes each chunk to Redis with 24h TTL:
       payload:chunk:0, payload:chunk:1, ...
  5. Writes a CoreDNS zone file with chunks encoded as TXT records
  6. Writes manifest to Redis:
       payload:manifest = {count, chunk_hashes, expires_at}

Agent (DNS path)
  ← queries TXT records from CoreDNS → retrieves chunks → reassembles → decrypts

Agent (HTTPS fallback)
  ← GET /api/v1/payload/chunk/:index → retrieves chunks → reassembles → decrypts
```

The DNS path makes payload retrieval indistinguishable from standard DNS traffic to a network observer. The HTTPS fallback is used when DNS-based delivery is blocked or impractical.

---

## Task Queue Design

Each agent has a dedicated Redis list key (`task:queue:<agent_id>`). The server enqueues tasks with `LPUSH` and agents drain with atomic `LMPOP` (Redis 7.0+) or a serial `LPOP` fallback.

Using per-agent queues rather than a shared queue means:
- A slow or offline agent does not block task delivery to other agents
- Queue length per agent is independently observable
- The lease lock prevents any race between concurrent poll requests from the same agent

Task state is persisted in PostgreSQL (`tasks` table). When a task expires (configurable TTL), the `taskExpirySweeper` goroutine marks it `EXPIRED` and writes an audit log entry.

---

## Background Goroutines

Two background goroutines run for the lifetime of the server process:

### Stale Agent Sweeper

Runs every 30 seconds. Queries PostgreSQL for agents whose `last_seen` timestamp is older than 120 seconds and marks them `OFFLINE`. This threshold should be tuned to match the real polling interval of deployed agents — a conservative default prevents false positives.

### Task Expiry Sweeper

Runs every 60 seconds. Finds tasks in `PENDING` state whose expiry timestamp has passed, marks them `EXPIRED`, and writes one audit log entry per expired task. Expired tasks are retained in the database for historical visibility.

---

## Security Properties

| Property | Implementation |
|---|---|
| TLS 1.3 only | Enforced in `deploy/nginx.conf`; older versions rejected |
| Token storage | Bearer tokens stored only as SHA-256 hash; never in plaintext |
| Database isolation | PostgreSQL and Redis have no published host ports; only Nginx is internet-facing |
| Rate limiting | Sliding-window per client IP at both Nginx (`limit_req_zone`) and application layers |
| SQL injection | All queries use parameterised statements in the repository layer |
| Audit trail | Every operator action, agent registration, task enqueue, and expiry is written to `audit_logs` |
| Queue race prevention | Redis lease lock prevents concurrent poll requests from double-draining a queue |

---

## Running Locally

```bash
cd endpoint-management-server
cp .env.example .env   # fill in all required secrets
docker compose up --build
```

PostgreSQL, Redis, three Go API replicas, and Nginx all start together. Drop TLS certificates into `deploy/certs/` before starting:

```bash
# Self-signed for local testing
openssl req -x509 -newkey rsa:4096 -keyout deploy/certs/key.pem -out deploy/certs/cert.pem -days 365 -nodes
```

The admin operator is seeded automatically on first boot using `ADMIN_PASSWORD` from `.env`.

---

## Directory Layout

```
endpoint-management-server/
│
├── cmd/server/
│   └── main.go                     Entry point, DI wiring, graceful shutdown, background sweepers
│
├── internal/
│   ├── auth/
│   │   ├── token.go                crypto/rand token generation
│   │   └── jwt.go                  JWT sign and verify
│   │
│   ├── c2/
│   │   ├── payload.go              AES-256-GCM encryption, chunk splitting
│   │   └── zone.go                 CoreDNS zone file generation for TXT-record delivery
│   │
│   ├── config/
│   │   └── config.go               Environment-based configuration (godotenv)
│   │
│   ├── db/
│   │   └── db.go                   PostgreSQL connection pool (pgxpool)
│   │
│   ├── handlers/
│   │   ├── agent_handler.go        Register, poll, telemetry
│   │   ├── operator_handler.go     Login, agent list, task enqueue
│   │   ├── payload_handler.go      Upload, status, chunk delivery, webhook
│   │   └── dashboard_handler.go    Aggregate summary
│   │
│   ├── middleware/
│   │   ├── auth.go                 Bearer token validation + Redis rate limiter
│   │   └── operator_auth.go        JWT validation for operator routes
│   │
│   ├── models/
│   │   └── models.go               Agent, Task, Telemetry, AuditLog, Operator structs
│   │
│   ├── queue/
│   │   └── queue.go                Redis-backed per-agent task queue (LMPOP / LPOP)
│   │
│   └── repository/
│       ├── agent_repo.go           agents table CRUD + stale sweep
│       ├── task_repo.go            tasks table CRUD + expiry sweep
│       ├── telemetry_repo.go       telemetry table insert and query
│       ├── audit_repo.go           append-only audit log writes
│       ├── operator_repo.go        operators table CRUD + count
│       └── dashboard_repo.go       aggregate query for dashboard summary
│
├── migrations/
│   ├── 001_init_schema.sql         agents, tasks, telemetry, audit_logs tables
│   ├── 002_operators.sql           operators table
│   └── 003_audit_log_index.sql     Index on audit_logs(entity_type, entity_id)
│
├── coredns/
│   └── Corefile                    CoreDNS configuration for DNS TXT payload delivery
│
├── deploy/
│   └── nginx.conf                  TLS 1.3 reverse proxy, rate limiting, security headers
│
├── docker-compose.yml              PostgreSQL + Redis + 3× Go API + Nginx
├── .env.example
├── go.mod
└── go.sum
```

---

## Relationship to Other Components

| Component | Relationship |
|---|---|
| `compiler/` + `cicd/` | Produces the payload binaries this server delivers to agents |
| `jocky-framework/` | The operator CLI is an alternative management interface for human-driven workflows |
| `byovd/` + `processhollowing/` | These modules run on the agent side; their results are submitted back here via the telemetry endpoint |
| `c2-client/` | The operator dashboard consumes this server's API endpoints for its UI |
