# TSAR — JOCKY Adversary Detection Framework

> A full-stack, evasion-resistant framework for deep-forensic system analysis and adversary detection, built around a custom programming language and a polymorphic compilation pipeline.

---

## Overview

Modern endpoint security tools — antivirus engines, EDRs, and kernel-level monitoring systems — detect forensic tooling through a combination of static signature matching, behavioral heuristics, and API call interception. They assume that analysis tooling will look like tooling: predictable compiler output, known import tables, stable binary hashes, and standard Windows API sequences. TSAR breaks every one of those assumptions.

TSAR is built around **JOCKY**, a custom programming language with its own compiler frontend and LLVM-based backend. Every script written in JOCKY compiles to a structurally unique binary. No two builds share the same hash, control flow structure, import table layout, or PE metadata. The evasion is not a wrapper applied after the fact — it is baked into the compilation pipeline itself, making it impossible for signature databases to keep up.

The framework covers the entire operational stack:

- **Language** — A statically typed DSL that compiles to LLVM IR, designed from the ground up to produce binaries that resist static analysis
- **Compilation** — A patched LLVM 16 toolchain (Polaris) with custom obfuscation passes, a polymorphic source transformer, and PE metadata spoofing
- **Delivery** — An automated CI/CD build service that regenerates binaries on demand, ensuring every deployed instance is unique
- **Execution** — In-memory process hollowing using direct NT syscalls, bypassing EDR hooks in `ntdll.dll`
- **Kernel layer** — A BYOVD-based Windows kernel driver for deep system telemetry: process enumeration, privilege escalation, and EDR callback removal
- **Management** — A high-concurrency Go backend and React operator dashboard for managing multiple endpoints simultaneously

---

## Architecture

![TSAR Architecture Diagram](docs/architecture.png)

> *Architecture diagram — place `docs/architecture.png` in the repository root.*

```
  Operator
  ┌──────────────────────────────┐
  │  jocky-framework CLI         │  Interactive terminal shell
  │  Encrypted source upload     │  AES-256-GCM over HTTPS
  └──────────────┬───────────────┘
                 │ .jky / .cpp source
                 ▼
  ┌──────────────────────────────────────────────────────┐
  │  Build Pipeline                                      │
  │                                                      │
  │  ┌─────────────────┐   LLVM IR   ┌────────────────┐ │
  │  │  JOCKY DSL      │ ──────────► │   Polaris      │ │
  │  │  Frontend       │             │   Compiler     │ │
  │  │  (jocky_v1/)    │             │  obf+poly+PE   │ │
  │  └─────────────────┘             └───────┬────────┘ │
  │                                          │           │
  │                              ┌───────────▼─────────┐ │
  │                              │   CI/CD Service     │ │
  │                              │   (auto regen)      │ │
  │                              └───────────┬─────────┘ │
  └──────────────────────────────────────────┼───────────┘
                                             │ binary artifact
         ┌───────────────────────────────────┼─────────────────────┐
         ▼                                   ▼                     ▼
  ┌──────────────────┐        ┌──────────────────────┐   ┌──────────────────┐
  │  Endpoint Mgmt   │        │  In-Memory Execution │   │  Operator        │
  │  Server          │        │  Process Hollowing   │   │  Dashboard       │
  │  (Go + Postgres) │        │  + BYOVD Driver      │   │  (React + Go)    │
  └──────────────────┘        └──────────────────────┘   └──────────────────┘
```

---

## Core Design Principles

### 1. Language-Level Evasion

The JOCKY DSL is not just a scripting convenience — it is an evasion primitive. The language's AST is deliberately designed to map to LLVM IR constructs that the downstream Polaris passes can aggressively transform. Control flow is structurally flattened before it reaches the binary. String literals never appear in the output. API names are resolved at runtime via PEB walking and djb2 hashing — they do not exist in the import table.

### 2. Polymorphic-by-Default

Every build is different. The polymorphic engine (`poly_engine.py`) transforms source before compilation, injecting randomised variable names, dead code chains, and always-false conditional blocks at every function boundary. Combined with LLVM-level instruction substitution and CFG flattening, no two compiled binaries are structurally similar. This breaks both static signature matching and hash-based reputation systems.

### 3. Continuous Delivery of Unique Artifacts

The CI/CD pipeline integrates directly with the Polaris compiler. Every invocation produces a new artifact with a different hash, entry point offset, and import table layout. This means that even if one deployed instance is flagged, the next build is clean. Signatures written against one build are useless against the next.

### 4. Fileless Execution via Direct Syscalls

The process hollowing module never calls the Windows API directly for sensitive operations. It resolves NT syscall service numbers (SSNs) at runtime using a three-stage fallback: Hell's Gate (reading from loaded `ntdll.dll`), Halo's Gate (scanning adjacent stubs when the primary is hooked), and a fresh disk copy of `ntdll.dll` as a last resort. All writes, allocations, and thread redirections go through raw `syscall` instructions. EDR hook trampolines in `ntdll.dll` are never reached.

### 5. Kernel-Level Visibility

The BYOVD driver operates at ring 0, giving the framework access to system state that is invisible from user space. It can enumerate processes using `ZwQuerySystemInformation` directly, escalate privileges via token theft from the SYSTEM process, and remove EDR notification callbacks from the kernel's `PspCreateProcessNotifyRoutine` array — silencing security agents before they can observe subsequent activity.

### 6. Centralised Multi-Endpoint Management

The endpoint management server handles simultaneous connections from multiple agents. It uses a Redis-backed per-agent task queue, PostgreSQL for durable state, and a structured audit log for every operator action. Traffic between agents and the server is routed through TLS 1.3 termination at Nginx, with optional payload delivery over DNS TXT records (DoH-style) to blend with legitimate infrastructure traffic.

---

## Components

| Directory | Language | Description |
|---|---|---|
| [`jocky_v1/`](jocky_v1/) | C++ / LLVM | JOCKY DSL compiler frontend — lexer, parser, AST, semantic analysis, LLVM IR codegen |
| [`compiler/`](compiler/) | C++ / Python | Polaris obfuscation pipeline — CFG flattening, instruction substitution, API hashing, PE spoofing |
| [`jocky-framework/`](jocky-framework/) | Python | Operator CLI shell and build server — encrypted source upload, live compiler log streaming |
| [`cicd/`](cicd/) | Python | Automated build service — FastAPI gateway, Redis job queue, sandboxed build workers |
| [`endpoint-management-server/`](endpoint-management-server/) | Go | Central management backend — agent registration, task dispatch, telemetry, payload delivery |
| [`processhollowing/`](processhollowing/) | C++ | In-memory execution — process hollowing with Hell's Gate / Halo's Gate direct syscall engine |
| [`byovd/`](byovd/) | C (WDK) | Kernel driver — process enumeration, privilege escalation, EDR callback removal |
| [`c2-client/`](c2-client/) | React / Go | Operator dashboard — endpoint monitoring, payload builder, live telemetry stream |

---

## Problem Statement

**SIH 2026 — PS 26148 (NTRO)**

Existing AV and EDR solutions prevent forensic analysis tooling from running on target systems through a combination of:

- **Static signature matching** — fixed binary hashes and PE patterns
- **Behavioral heuristics** — known API call sequences and execution patterns
- **Compiler artifact fingerprinting** — recognising standard MSVC/GCC output structures
- **Kernel-level monitoring** — `PsCreateProcessNotifyRoutine` and similar callbacks that intercept process creation and module loading

TSAR addresses each of these attack surfaces directly:

| Detection method | TSAR countermeasure |
|---|---|
| Static signatures / hash databases | Polymorphic engine — unique binary on every build |
| Import table scanning | PEB-walk API hashing — no imports in the IAT |
| CFG-based behavioral analysis | LLVM CFG flattening — destroys recognisable control flow |
| API call sequence monitoring | Direct NT syscalls — EDR hooks in `ntdll.dll` are bypassed entirely |
| PE metadata fingerprinting | PE header spoofing — Rich Header, timestamps, version identity rotation |
| Kernel notification callbacks | BYOVD driver — callback array zeroing via kernel memory write |

---

## Quick Start

### Prerequisites

| Requirement | Purpose |
|---|---|
| Windows x64 | Compiler, BYOVD driver, process hollowing |
| Docker | CI/CD service and endpoint management server |
| Python 3.10+ | Operator CLI, CI/CD worker, build server |
| Go 1.22+ | Endpoint management server, operator dashboard backend |
| Node.js 20+ | Operator dashboard frontend |
| LLVM 16 | System linker for the Polaris pipeline |
| Visual Studio 2022 Build Tools | Windows SDK, MSVC toolchain, WDK |

Install LLVM on Windows:

```powershell
winget install LLVM.LLVM
```

---

### 1. Compile a JOCKY script

```powershell
cd compiler
.\jocky\driver\jocky.exe payload.jky -o payload.exe
```

For C/C++ source with full obfuscation:

```powershell
.\jocky\driver\jocky.exe payload.cpp -o payload.exe "-passes=fla,sub,api-hash"
```

See [`compiler/README.md`](compiler/README.md) for the full CLI reference and pass list.

---

### 2. Start the operator CLI and build server

```bash
cd jocky-framework/server
pip install -r requirements.txt
uvicorn app:app --host 0.0.0.0 --port 8000
```

```bash
cd jocky-framework/client
python -m jocky_client
# jocky > connect --addr localhost:8000 --token <token>
# jocky > build --template payload.jky --output agent.exe --passes fla,sub,api-hash
```

---

### 3. Start the CI/CD build service

```bash
cd cicd
cp .env.example .env
docker compose up --build
# API: http://localhost:8000
```

---

### 4. Start the endpoint management server

```bash
cd endpoint-management-server
cp .env.example .env
docker compose up --build
# API: https://localhost (Nginx TLS)
```

---

### 5. Start the operator dashboard

```bash
cd c2-client
docker compose up --build
# Dashboard: http://localhost:3000
```

---

## Repository Layout

```
TSAR/
├── jocky_v1/                     JOCKY DSL language frontend (C++ / LLVM)
├── compiler/                     Polaris compiler pipeline + modular runtime
├── jocky-framework/              Operator CLI shell + Python build server
├── cicd/                         Automated polymorphic build service
├── endpoint-management-server/   Central management backend (Go)
├── processhollowing/             In-memory process hollowing module
├── byovd/                        Windows kernel driver + user-mode client
├── c2-client/                    Operator dashboard (React + Go)
└── docs/                         Architecture diagrams and supplementary docs
```

---

## Contributing

This repository is part of an active research project. Component-level documentation lives in each subdirectory's `README.md`. For architectural questions or integration guidance, start with the component READMEs and the inline source comments.

---

## License

Research use only. See `LICENSE` for terms.
