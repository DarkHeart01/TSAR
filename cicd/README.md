# CI/CD Build Pipeline

The CI/CD module is an asynchronous build automation service that compiles JOCKY scripts and C/C++ sources through the Polaris pipeline on demand. It exposes a REST API for submitting build jobs, tracks job state in Redis, and returns compiled artifacts over HTTP.

Each build executes in an isolated, resource-constrained subprocess with a hardened environment — no inherited shell variables, strict filesystem sandboxing, and enforced CPU/memory limits.

---

## Architecture

```
Client
  │
  │  POST /v1/build/trigger
  ▼
┌─────────────────────┐
│   API Gateway       │   FastAPI (Python)
│   (cicd/api/)       │   Validates request, enqueues job
└──────────┬──────────┘
           │  Redis list: build_queue:pending
           ▼
┌─────────────────────┐
│   Build Worker      │   Python subprocess runner
│   (cicd/worker/)    │   Dequeues, compiles, stores artifact
└──────────┬──────────┘
           │  Writes to /build_artifacts/<job_id>/
           ▼
┌─────────────────────┐
│   API Gateway       │   GET /v1/build/artifact/<job_id>
│   (artifact serve)  │   Serves output binary on completion
└─────────────────────┘
```

All three services — `redis`, `api-gateway`, and `build-worker` — run as Docker containers on an isolated bridge network. Only the API gateway publishes a host port.

---

## API Reference

### `POST /v1/build/trigger`

Submit a build job. Returns immediately with a `job_id`; compilation runs asynchronously.

**Request body:**

```json
{
  "source_files": {
    "payload.cpp": "<source content>",
    "CMakeLists.txt": "<optional cmake>"
  },
  "build_flags": ["-O2", "-march=x86-64"],
  "cmake_flags": ["-DCMAKE_BUILD_TYPE=Release"],
  "optimization": "O2",
  "timeout_seconds": 120
}
```

**Response `202`:**

```json
{ "job_id": "a1b2c3d4-...", "status": "QUEUED" }
```

---

### `GET /v1/build/status/{job_id}`

Poll job state.

**Response `200`:**

```json
{
  "job_id": "a1b2c3d4-...",
  "status": "SUCCESS",
  "created_at": "2026-01-01T00:00:00Z",
  "started_at": "2026-01-01T00:00:01Z",
  "finished_at": "2026-01-01T00:00:08Z",
  "error": null,
  "log_tail": "..."
}
```

Status values: `QUEUED` → `COMPILING` → `SUCCESS` / `FAILED`

---

### `GET /v1/build/artifact/{job_id}`

Download the compiled binary. Returns `409` if the job has not yet succeeded.

---

### `GET /healthz`

Liveness check. Returns `200 {"status": "ok"}` when Redis is reachable.

---

## Build Worker

The worker (`worker/worker.py`) runs a blocking Redis `BRPOP` loop. For each dequeued job it:

1. Writes source files to an isolated workspace directory (`/build_workspace/<job_id>/`)
2. Validates all filenames and build flags against strict allowlists before passing them to any subprocess
3. Invokes `cmake` + `ninja` (if `CMakeLists.txt` is present) or `clang`/`clang++` directly
4. Copies the output binary to `/build_artifacts/<job_id>/output_binary`
5. Updates job state in Redis

The worker can be scaled horizontally: `docker compose up --scale build-worker=N`

---

## Input Sanitisation

All user-supplied inputs are validated before reaching a subprocess. Rules enforced by `worker/sanitize.py`:

| Input | Rule |
|---|---|
| Filenames | Must match `[A-Za-z0-9][A-Za-z0-9_\-./]{0,127}\.(c\|cc\|cpp\|h\|hpp\|txt)`, no `..` or leading `/` |
| Build flags | Must match `-[A-Za-z][A-Za-z0-9=_.\-]{0,64}`, no shell metacharacters |
| CMake defines | Must match `-D[A-Z_][A-Z0-9_]*=[A-Za-z0-9_./\- ]{0,128}` |
| Blocked flag prefixes | `-o`, `-I/`, `-L/`, `-B/`, `-Xclang`, `-Wl,`, `-fplugin`, and others |

Subprocess environments are fully hardened: `PATH=/usr/bin:/bin`, no inherited variables, `shell=False`, CPU and address-space `rlimit` set in the child before `exec()`.

---

## Deployment

```bash
cd cicd
cp .env.example .env   # set API_PORT, timeouts
docker compose up --build
```

The `docker-compose.yml` defines:

| Service | Image | Notes |
|---|---|---|
| `redis` | `redis:7-alpine` | Persistence with 60s/1-write RDB snapshot |
| `api-gateway` | Built from `api/` | Mounts artifact volume read-only |
| `build-worker` | Built from `worker/` | CPU limit 2 cores, memory limit 2 GB |

Scale workers: `docker compose up --scale build-worker=3`

---

## Directory Layout

```
cicd/
├── .env.example
├── docker-compose.yml
├── api/
│   ├── Dockerfile
│   ├── main.py          FastAPI app — trigger, status, artifact endpoints
│   ├── models.py        Pydantic request/response models
│   └── requirements.txt
└── worker/
    ├── Dockerfile
    ├── worker.py        Redis BRPOP loop + job dispatcher
    ├── build_executor.py  Subprocess compiler invocation
    ├── sanitize.py      Input validation and allowlisting
    ├── redis_client.py  Redis connection helper
    └── requirements.txt
```
