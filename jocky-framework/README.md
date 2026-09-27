# jocky

A remote build CLI for the JOCKY redteam framework. A client REPL uploads a `.jky` source file to a server, which compiles it through the JOCKY compiler pipeline and streams the compile log back live. Builds are associated with registered **targets** (machines under attack) for structured per-engagement tracking and multi-attack monitoring.

- **Client** (`client/`): Python, interactive shell built on `cmd`, packaged into a standalone `jocky.exe` with PyInstaller.
- **Server** (`server/`): Python, FastAPI over plain HTTP (no WebSockets). Receives encrypted `.jky` uploads, invokes the compiler, buffers stdout in memory, persists build history in SQLite. Clients poll for logs.
- **Compiler** (`compiler/`): The JOCKY compiler pipeline. The server calls `compiler/jocky/driver/jocky.exe`.

> Security note: the server shells out to the compiler with **no sandboxing** — private/trusted network only.

---

## Source encryption

Uploaded `.jky` source is AES-256-GCM encrypted client-side before sending. The key is derived from the shared connect token via HKDF-SHA256 (`client/jocky_client/crypto.py` / `server/app/crypto.py`). A tampered upload fails decryption with `400 failed to decrypt uploaded file` instead of silently compiling garbage.

The auth token, filenames, compile logs, and session history still travel as plain HTTP — add TLS if those need protecting.

---

## Server setup

```
cd server
pip install -r requirements.txt
copy .env.example .env   # edit JOCKY_TOKENS
```

```
uvicorn app.main:app --host 0.0.0.0 --port 8000
```

`JOCKY_TOKENS` is a comma-separated `token:client_name` list. The server expects `compiler/jocky/driver/jocky.exe` at the repo root by default; set `COMPILER_PATH` to override.

---

## Client setup

```
cd client
pip install -r requirements.txt
python -m jocky_client        # run directly
```

To build the standalone executable:

```
pyinstaller jocky.spec        # output: client/dist/jocky.exe
```

---

## REPL commands

### Connection
- `connect --addr <ip:port> --token <token>` — authenticate and link this shell to a server. Connection is restored automatically on next launch from `~/.jocky/config.json`.

### Targets
A **target** is a machine you are attacking. Builds are associated with targets for per-engagement history.

- `target add --name <name> --host <host> [--os <os>] [--notes <text>]` — register a new target machine.
- `target list` — list all registered targets (`*` marks the active one).
- `target select <id>` — set the active target; subsequent builds use it automatically.
- `target info` — show details of the active target.

### Builds
- `build --source <file.jky> --output <name.exe> [--passes <passes>] [--target-id <id>]` — encrypt and upload a `.jky` payload, then stream compiler logs until the build finishes. Uses the active target by default.

Default passes: `fla,sub,api-hash`. Available: `fla`, `sub`, `api-hash`, `mba`, `indcall`, `indbr` (avoid `bcf`, `gvenc` — both raise detections).

### History
- `sessions [--target-id <id>]` / `history` — list past builds with target info. Filters to the active target by default.

### Ops (multi-attack monitor)
- `ops [--status running|pending|success|failed] [--watch] [--interval N]` — live dashboard of all active builds across all targets. `--watch` refreshes automatically every N seconds (default 2).

### Misc
- `exit` / `quit` — leave the shell.
- `help` — list all commands.

---

## Quick end-to-end test

1. Start the server with `JOCKY_TOKENS=devtoken123:dev-client`.
2. In the REPL:
   ```
   connect --addr 127.0.0.1:8000 --token devtoken123
   target add --name DC-01 --host 192.168.1.10 --os "Windows Server 2022" --notes "primary DC"
   target select <id from above>
   build --source payload.jky --output payload.exe
   ops
   sessions
   exit
   ```

---

## API reference

| Method | Path | Description |
|--------|------|-------------|
| POST | `/api/connect` | Authenticate |
| POST | `/api/target` | Register a target |
| GET  | `/api/targets` | List targets |
| GET  | `/api/target/{id}` | Get target |
| POST | `/api/build` | Submit a `.jky` build |
| GET  | `/api/build/{id}/logs?offset=N` | Poll compile logs |
| GET  | `/api/sessions[?target_id=]` | List builds with target info |
| GET  | `/api/ops[?status=]` | All builds, live in-memory status for running ones |
