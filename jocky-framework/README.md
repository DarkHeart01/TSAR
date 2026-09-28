# JOCKY Framework — Operator CLI and Build Server

The `jocky-framework` directory contains the operator-facing toolchain for driving the JOCKY compiler pipeline: a Python FastAPI build server and a terminal-based interactive shell client. Operators write JOCKY scripts locally, submit them through the encrypted CLI, and stream real-time compiler output back to their terminal.

This is the primary workflow interface for the framework — the component a researcher interacts with directly when developing and deploying analysis scripts.

---

## Architecture

```
Operator machine                        Build server
┌──────────────────────────────┐        ┌──────────────────────────────┐
│  jocky CLI (Python)          │        │  FastAPI server (Python)     │
│  jocky_client/               │        │  server/app/                 │
│                              │        │                              │
│  JockyShell  ──connect──────►│─HTTPS─►│  POST /api/connect           │
│             │                │        │  (token auth)                │
│             └─build──────────│─HTTPS─►│  POST /api/build             │
│               (encrypted)   │        │  (AES-256-GCM source upload) │
│                              │        │       │                      │
│  <stream compiler logs>      │◄───────│  GET  /api/build/{id}/logs   │
│                              │        │       │                      │
└──────────────────────────────┘        │  invokes Polaris compiler    │
                                        │  (compiler/ pipeline)        │
                                        └──────────────────────────────┘
```

---

## Components

### Server (`server/`)

A FastAPI application that:

- Authenticates operators via a shared bearer token (`POST /api/connect`)
- Receives encrypted source file uploads (`POST /api/build`)
- Decrypts the source, invokes the Polaris compiler (`compiler/jocky/driver/jocky.exe`) with the specified obfuscation passes
- Streams compiler output line-by-line into a log buffer
- Exposes build logs and status for polling (`GET /api/build/{id}/logs`)
- Lists past build sessions (`GET /api/sessions`)

Key modules:

| File | Purpose |
|---|---|
| `app/auth.py` | Bearer token validation, `/api/connect` endpoint |
| `app/compiler.py` | Async subprocess wrapper around the Polaris compiler CLI |

The compiler is invoked as:

```
<COMPILER_PATH> <source> -o <output> "-passes=<passes>" -v
```

### Client (`client/`)

A `cmd.Cmd`-based interactive terminal shell distributed as a standalone executable (PyInstaller `.spec` provided). Key commands:

| Command | Description |
|---|---|
| `connect --addr <ip:port> --token <token>` | Authenticate and link to a build server |
| `build --template <path> --output <name> --passes <passes>` | Upload a source file and stream compile logs |
| `sessions` / `history` | List past build jobs with status and exit codes |
| `exit` / `quit` | Exit the shell |

Credentials are saved to disk after a successful `connect` so they persist across shell restarts.

---

## Encrypted Transport

Source files are encrypted before upload using **AES-256-GCM**:

- The key is derived from the shared connect token using **HKDF-SHA256** with a fixed salt and info string
- A fresh 12-byte random nonce is generated per upload
- The filename is passed as authenticated associated data — a file submitted with a different name will fail to decrypt
- The server and client share identical `crypto.py` implementations; the same derivation and construction must be used on both sides

This ensures source files are never transmitted in plaintext, even over plain HTTP in a local network context.

---

## Running

### Build server

```bash
cd jocky-framework/server
python -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt
uvicorn app:app --host 0.0.0.0 --port 8000
```

Set `COMPILER_PATH` in the environment (or `app/config.py`) to point to `compiler/jocky/driver/jocky.exe`.

### CLI client

```bash
cd jocky-framework/client
pip install -r requirements.txt
python -m jocky_client
```

Or build a standalone executable:

```bash
pyinstaller jocky.spec
# Output: dist/jocky.exe
```

### Connecting

```
jocky > connect --addr 192.168.1.10:8000 --token <your-token>
connected to 192.168.1.10:8000 as operator

jocky > build --template payload.jky --output agent.exe --passes fla,sub,api-hash
build a1b2c3d4 submitted, polling for logs...
[compiler] Stage 3: poly_engine.py — variable rename pass
[compiler] Stage 1A: Polaris clang — fla,sub,api-hash
[compiler] Stage 6: pe_header_spoofer.py
BUILD SUCCEEDED (exit 0)
```

---

## Directory Layout

```
jocky-framework/
├── server/
│   ├── app/
│   │   ├── __init__.py
│   │   ├── auth.py          Bearer token auth + /api/connect
│   │   └── compiler.py      Async Polaris compiler invocation
│   └── .venv/               Python virtual environment
└── client/
    ├── jocky.spec            PyInstaller build spec
    └── jocky_client/
        ├── __main__.py       Entry point, Windows ANSI setup
        ├── shell.py          cmd.Cmd interactive shell (JockyShell)
        ├── api.py            HTTP client — connect, build, poll, sessions
        ├── crypto.py         AES-256-GCM + HKDF-SHA256 source encryption
        ├── config.py         Credential persistence
        └── banner.py         ASCII art banner
```
