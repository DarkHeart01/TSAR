# JOCKY Framework — Operator CLI and Build Server

> The primary operator-facing toolchain for the JOCKY framework. A Python FastAPI build server and a terminal-based interactive shell client for writing, submitting, and managing JOCKY analysis scripts.

---

## Overview

The `jocky-framework` is the day-to-day workflow interface for operators working with TSAR. It separates the build infrastructure from the operator's machine: the operator writes JOCKY scripts locally, submits them to the build server over an encrypted channel, and streams real-time compiler output back to their terminal. The compiled binary never touches the operator's disk in plaintext — source files are encrypted before upload and the output is retrieved separately.

The framework has two components:

- **Build server** (`server/`) — a FastAPI application that receives encrypted source uploads, invokes the Polaris compiler pipeline, and exposes build logs and artifacts over HTTP
- **Operator CLI** (`client/`) — an interactive terminal shell (`cmd.Cmd`-based) for connecting to a build server, submitting builds, and reviewing build history

---

## Architecture

```
Operator machine
┌────────────────────────────────────────────────────────┐
│                                                        │
│  $ python -m jocky_client                              │
│  jocky > connect --addr 10.0.0.5:8000 --token <token> │
│  jocky > build --template recon.jky --output agent.exe │
│                                                        │
│  ┌─────────────────────────────────────────────────┐  │
│  │  JockyShell (cmd.Cmd)                           │  │
│  │  ├── do_connect  → ApiClient.connect()          │  │
│  │  ├── do_build    → ApiClient.submit_build()     │  │
│  │  │               → ApiClient.poll_build_logs()  │  │
│  │  └── do_sessions → ApiClient.sessions()         │  │
│  └───────────────────────┬─────────────────────────┘  │
│                           │ AES-256-GCM encrypted      │
│                           │ multipart/form-data        │
└───────────────────────────┼────────────────────────────┘
                            │ HTTPS
                            ▼
Build server (remote)
┌────────────────────────────────────────────────────────┐
│                                                        │
│  FastAPI application (uvicorn)                         │
│  ├── POST /api/connect      token validation           │
│  ├── POST /api/build        decrypt source, queue job  │
│  ├── GET  /api/build/{id}/logs  stream compiler output │
│  └── GET  /api/sessions     list past builds           │
│                                                        │
│  ┌───────────────────────────────────────────────┐    │
│  │  compiler.py                                  │    │
│  │  asyncio.create_subprocess_exec               │    │
│  │  → jocky.exe <source> -o <output> -passes=.. │    │
│  │  → streams stdout line-by-line into log buf   │    │
│  └───────────────────────────────────────────────┘    │
│                                                        │
└────────────────────────────────────────────────────────┘
```

---

## Encrypted Transport

Source files submitted via the `build` command are encrypted before leaving the operator's machine. The encryption scheme is:

- **Algorithm**: AES-256-GCM (authenticated encryption — tampering or corruption fails the decrypt rather than silently returning garbage)
- **Key derivation**: HKDF-SHA256 over the shared connect token, with a fixed salt (`jocky-file-encryption-salt-v1`) and info string (`jocky-build-upload`)
- **Nonce**: 12-byte random nonce generated fresh per upload, prepended to the ciphertext
- **Associated data**: the source filename — a file submitted claiming a different name will fail authentication at the server

The key is derived from the connect token, which both sides already know. No separate key distribution is required. The server's `crypto.py` and the client's `crypto.py` are identical implementations — both sides must use the same derivation parameters or decryption fails.

```
plaintext + filename (AD)
        │
        ▼  HKDF-SHA256(token, salt, info) → 32-byte key
        │  random 12-byte nonce
        │
        ▼  AES-256-GCM.encrypt(key, nonce, plaintext, AD)
        │
        nonce || ciphertext || GCM tag
        └─── uploaded to server
```

---

## Build Server

### API Endpoints

| Method | Path | Auth | Description |
|---|---|---|---|
| `POST` | `/api/connect` | Token in body | Validate the bearer token; return the associated client name |
| `POST` | `/api/build` | Bearer | Receive an encrypted source upload; queue a compile job; return `build_id` |
| `GET` | `/api/build/{id}/logs` | Bearer | Return buffered compiler output lines from `offset`; include status and exit code |
| `GET` | `/api/sessions` | Bearer | Return the list of past build jobs with status, filenames, and timestamps |

### Compiler Invocation

The server invokes the Polaris compiler as an async subprocess:

```python
cmd = [COMPILER_PATH, source_path, "-o", output_path, f"-passes={passes}", "-v"]
process = await asyncio.create_subprocess_exec(*cmd, stdout=PIPE, stderr=STDOUT)
```

Each output line is appended to a log buffer in memory and made available to the client through the `/logs` endpoint. Clients poll at 400ms intervals until the status transitions to `success` or `failed`.

### Authentication Model

Authentication uses a shared bearer token validated against a backing store (`db.get_client_name(token)`). The token is passed in the `Authorization: Bearer <token>` header for all protected endpoints. The `/api/connect` endpoint accepts the token in the request body to allow the CLI to verify connectivity and retrieve the client name before caching credentials locally.

---

## Operator CLI

### Shell Commands

| Command | Description |
|---|---|
| `connect --addr <ip:port> --token <token>` | Authenticate to a build server; saves credentials locally on success |
| `build --template <path> --output <name> --passes <passes>` | Encrypt and upload a source file; stream compiler logs until completion |
| `sessions` | List all past build jobs: ID, filename, output name, status, exit code, timestamp |
| `history` | Alias for `sessions` |
| `exit` / `quit` | Exit the shell |

### Session Example

```
$ python -m jocky_client

     ██╗ ██████╗  ██████╗██╗  ██╗██╗   ██╗
     ██║██╔═══██╗██╔════╝██║ ██╔╝╚██╗ ██╔╝
     ██║██║   ██║██║     █████╔╝  ╚████╔╝
██   ██║██║   ██║██║     ██╔═██╗   ╚██╔╝
╚█████╔╝╚██████╔╝╚██████╗██║  ██╗   ██║
 ╚════╝  ╚═════╝  ╚═════╝╚═╝  ╚═╝   ╚═╝

jocky > connect --addr 10.0.0.5:8000 --token s3cr3t
connected to 10.0.0.5:8000 as operator

jocky > build --template recon.jky --output agent.exe --passes fla,sub,api-hash
build a1b2c3d4-... submitted, polling for logs...
[compiler] Stage 3: poly_engine.py — JOCKY-syntax transform (seed: 0x8f3a)
[compiler] Stage 3B: jocky_v1 frontend — recon.jky → /tmp/a1b2c3d4.ll
[compiler] Stage 1A: Polaris clang — passes: fla,sub,api-hash
[compiler] Stage 1B: lld — linking runtime: core, syscall, dummy_imports, entropy_pad
[compiler] Stage 6: pe_header_spoofer.py — identity: VLC media player 3.0.20.0
BUILD SUCCEEDED (exit 0)

jocky > sessions
ID                                   FILE              OUTPUT           STATUS   EXIT  CREATED
a1b2c3d4-...                         recon.jky         agent.exe        success  0     2026-09-29T14:22:01Z
```

### Credential Persistence

After a successful `connect`, the server address and token are saved to a local config file by `config.py`. On the next shell startup, these are loaded automatically so the operator does not need to re-authenticate.

---

## Standalone Executable

The CLI ships as a single standalone `.exe` built with PyInstaller. The `jocky.spec` file contains the build configuration.

```bash
cd client
pip install pyinstaller
pyinstaller jocky.spec
# Output: dist/jocky.exe
```

The resulting binary has no external Python dependencies and runs on any Windows x64 machine without a Python installation.

---

## Running

### Build server

```bash
cd jocky-framework/server
python -m venv .venv
.venv\Scripts\activate      # Windows
# source .venv/bin/activate  # Linux/macOS
pip install -r requirements.txt
uvicorn app:app --host 0.0.0.0 --port 8000
```

Set the `COMPILER_PATH` environment variable to point to `compiler/jocky/driver/jocky.exe` (or configure it in `app/config.py`).

### CLI client

```bash
cd jocky-framework/client
pip install -r requirements.txt
python -m jocky_client
```

---

## Directory Layout

```
jocky-framework/
├── server/
│   ├── app/
│   │   ├── __init__.py             FastAPI app initialisation
│   │   ├── auth.py                 Bearer token validation, /api/connect endpoint
│   │   ├── compiler.py             Async subprocess wrapper for Polaris compiler
│   │   ├── crypto.py               AES-256-GCM + HKDF-SHA256 (must match client)
│   │   ├── db.py                   Token-to-client-name lookup (SQLite)
│   │   └── models.py               Pydantic request/response models
│   └── .venv/                      Python virtual environment
│
└── client/
    ├── jocky.spec                  PyInstaller spec for standalone .exe build
    └── jocky_client/
        ├── __main__.py             Entry point; Windows ANSI mode setup
        ├── shell.py                JockyShell — cmd.Cmd interactive shell
        ├── api.py                  HTTP client: connect, submit_build, poll_build_logs, sessions
        ├── crypto.py               AES-256-GCM + HKDF-SHA256 (must match server)
        ├── config.py               Local credential persistence
        └── banner.py               ASCII art startup banner
```

---

## Relationship to Other Components

| Component | Relationship |
|---|---|
| `compiler/` | The build server invokes `compiler/jocky/driver/jocky.exe` to compile submitted sources |
| `jocky_v1/` | Invoked transitively by the compiler when the submitted file is `.jky` |
| `cicd/` | The CI/CD service provides an alternative automated build path; the operator CLI is for interactive, human-driven builds |
| `endpoint-management-server/` | The compiled artifact produced here is deployed to agents managed by the endpoint server |
