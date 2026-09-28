# CI/CD Build Pipeline

> An asynchronous, containerised build automation service for the JOCKY compiler pipeline. Accepts source submissions over a REST API, queues them in Redis, compiles them in isolated worker containers, and serves the output binaries for download.

---

## Overview

A core property of TSAR is that each deployed binary must be structurally unique. Manually recompiling before every deployment does not scale. The CI/CD service automates this: it exposes a simple HTTP API for triggering builds, runs each compilation in a sandboxed worker process with strict resource limits, and stores the output artifact for retrieval.

The service is designed to be trigger-friendly. It can be invoked by the operator CLI, a webhook from GitHub Actions, the endpoint management server's payload handler, or any HTTP client. The caller submits source and options, gets back a job ID, polls for completion, and downloads the artifact.

Because each build runs through the full Polaris pipeline — including the polymorphic engine with a fresh random seed — no two build jobs produce the same binary, even from identical source.

---

## Architecture

```
HTTP client (operator CLI / webhook / management server)
        │
        │  POST /v1/build/trigger
        ▼
┌───────────────────────────────────────────────────────┐
│  API Gateway (FastAPI)                                │
│                                                       │
│  - Validates request (Pydantic models)                │
│  - Generates UUID job ID                              │
│  - Writes job metadata to Redis hash                  │
│  - Pushes job payload to Redis list                   │
│  - Returns {job_id, status: QUEUED}                   │
└──────────────────────┬────────────────────────────────┘
                       │  Redis list: build_queue:pending
                       │  (LPUSH / BRPOP)
                       ▼
┌───────────────────────────────────────────────────────┐
│  Build Worker (Python subprocess runner)              │
│                                                       │
│  - BRPOP loop — blocks until a job appears            │
│  - Validates all inputs (filenames, flags, cmake)     │
│  - Writes source files to isolated workspace          │
│  - Invokes cmake+ninja or clang/clang++ directly      │
│  - Copies output binary to artifact store             │
│  - Updates job status in Redis                        │
│  - Cleans up workspace on completion                  │
│                                                       │
│  Resource limits (per job):                           │
│    CPU: 2 cores   Memory: 2 GB   Timeout: 120–600s    │
└──────────────────────┬────────────────────────────────┘
                       │
                       ▼
              /build_artifacts/<job_id>/output_binary
                       │
                       ▼
┌───────────────────────────────────────────────────────┐
│  API Gateway                                          │
│  GET /v1/build/artifact/{job_id}                      │
│  Serves output binary as application/octet-stream     │
└───────────────────────────────────────────────────────┘
```

All three services run as Docker containers on an isolated bridge network (`build_net`). Only the API gateway exposes a host port. Workers and Redis are not reachable from outside the network.

---

## API Reference

### `POST /v1/build/trigger`

Submit a build job. Returns immediately with a `job_id`. Compilation runs asynchronously in a worker.

**Request body:**

```json
{
  "source_files": {
    "payload.cpp": "// source content here",
    "helper.h": "// optional header",
    "CMakeLists.txt": "// optional — triggers cmake build"
  },
  "build_flags": ["-march=x86-64", "-Wall"],
  "cmake_flags": ["-DCMAKE_BUILD_TYPE=Release"],
  "optimization": "O2",
  "timeout_seconds": 120
}
```

**Response `202 Accepted`:**

```json
{
  "job_id": "a1b2c3d4-1234-5678-abcd-ef0123456789",
  "status": "QUEUED"
}
```

---

### `GET /v1/build/status/{job_id}`

Poll job state. Call this until `status` is `SUCCESS` or `FAILED`.

**Response `200 OK`:**

```json
{
  "job_id": "a1b2c3d4-...",
  "status": "SUCCESS",
  "created_at": "2026-09-29T10:00:00Z",
  "started_at": "2026-09-29T10:00:01Z",
  "finished_at": "2026-09-29T10:00:09Z",
  "error": null,
  "log_tail": "clang++ -O2 payload.cpp -o output_binary\n..."
}
```

**Status lifecycle:**

```
QUEUED  →  COMPILING  →  SUCCESS
                      →  FAILED
```

---

### `GET /v1/build/artifact/{job_id}`

Download the compiled binary. Returns `409 Conflict` if the job has not completed successfully.

**Response `200 OK`:** `Content-Type: application/octet-stream`

Returns `404` if the job does not exist. Returns `409` if status is not `SUCCESS`.

---

### `GET /healthz`

Liveness check for load balancers and orchestrators.

**Response `200 OK`:** `{"status": "ok"}`

Returns `503` if Redis is unreachable.

---

## Build Worker

The worker (`worker/worker.py`) runs a continuous `BRPOP` loop against the `build_queue:pending` Redis list, blocking for up to 5 seconds per iteration.

For each dequeued job:

1. **Validate inputs** — all filenames and flags are validated against strict allowlists before any filesystem or subprocess operation
2. **Write sources** — source files are written to an isolated workspace at `/build_workspace/<job_id>/`
3. **Compile** — selects the appropriate build strategy:
   - If `CMakeLists.txt` is present: `cmake -G Ninja` configure + `cmake --build`
   - Otherwise: direct `clang` or `clang++` invocation
4. **Copy artifact** — output binary is copied to `/build_artifacts/<job_id>/output_binary` and the full build log to `build.log`
5. **Update Redis** — job status, timestamps, and log tail (last 4000 chars) are written atomically
6. **Clean workspace** — the workspace directory is deleted regardless of success or failure

Workers can be scaled horizontally:

```bash
docker compose up --scale build-worker=4
```

Each worker operates independently. Redis provides the coordination — a job dequeued by one worker is invisible to all others.

---

## Input Sanitisation

All user-supplied inputs are validated by `worker/sanitize.py` before reaching any subprocess. Validation runs in the worker (not just the API layer) — an attacker who writes directly to Redis bypasses the API but not the worker.

| Input | Rule | Blocked |
|---|---|---|
| **Filenames** | `[A-Za-z0-9][A-Za-z0-9_\-./]{0,127}\.(c\|cc\|cpp\|h\|hpp\|txt)` | `..` path traversal, absolute paths, non-source extensions |
| **Build flags** | `-[A-Za-z][A-Za-z0-9=_.\-]{0,64}` | Shell metacharacters (`;`, `\|`, `&`, `` ` ``, `$`, `>`, `<`) |
| **Build flag prefixes** | Allowlist-based | `-o`, `-I/`, `-L/`, `-B/`, `-Xclang`, `-Wl,`, `-fplugin`, `-fuse-ld`, etc. |
| **CMake defines** | `-D[A-Z_][A-Z0-9_]*=[A-Za-z0-9_./\- ]{0,128}` | Anything that is not a simple `-DVAR=value` define |

Additionally, every subprocess is launched with:

- `shell=False` — no shell interpretation of arguments
- A hardcoded minimal environment: `PATH=/usr/bin:/bin`, `HOME=/tmp`, `LANG=C` — no inherited variables
- `start_new_session=True` — subprocess gets its own process group; the entire group is killed on timeout
- `setrlimit` in the child before `exec()`: CPU time limit, address space limit (2 GB), open file descriptors (256)

---

## Deployment

```bash
cd cicd
cp .env.example .env   # configure API_PORT, timeouts
docker compose up --build
```

### Services

| Service | Image | Exposed | Notes |
|---|---|---|---|
| `redis` | `redis:7-alpine` | Internal only | RDB persistence (60s/1-write snapshot) |
| `api-gateway` | `./api` | `$API_PORT` (default 8000) | Mounts artifact volume read-only |
| `build-worker` | `./worker` | Internal only | CPU: 2 cores, RAM: 2 GB; scale with `--scale` |

### Environment Variables

| Variable | Default | Description |
|---|---|---|
| `API_PORT` | `8000` | Host port for the API gateway |
| `DEFAULT_BUILD_TIMEOUT` | `120` | Default job timeout in seconds |
| `MAX_BUILD_TIMEOUT` | `600` | Hard cap on any job timeout |
| `REDIS_HOST` | `redis` | Redis hostname (within Docker network) |
| `REDIS_PORT` | `6379` | Redis port |
| `ARTIFACT_DIR` | `/build_artifacts` | Where completed binaries are stored |
| `WORKSPACE_DIR` | `/build_workspace` | Ephemeral per-job workspace |

---

## Directory Layout

```
cicd/
├── .env.example
├── docker-compose.yml
│
├── api/
│   ├── Dockerfile
│   ├── main.py              FastAPI app: /trigger, /status, /artifact, /healthz
│   ├── models.py            Pydantic request/response models + UUID validation
│   └── requirements.txt
│
└── worker/
    ├── Dockerfile
    ├── worker.py            BRPOP event loop + job dispatcher
    ├── build_executor.py    Subprocess build runner (cmake or direct clang)
    ├── sanitize.py          Input validation and allowlisting
    ├── redis_client.py      Redis connection helper with retry
    └── requirements.txt
```

---

## Relationship to Other Components

| Component | Relationship |
|---|---|
| `compiler/` | The worker ultimately calls `jocky.exe` from the compiler pipeline to perform obfuscated builds |
| `jocky-framework/` | The operator CLI provides an interactive alternative to this service for human-driven builds |
| `endpoint-management-server/` | The payload handler can trigger builds via this service's webhook endpoint and retrieve the artifact for delivery to agents |
