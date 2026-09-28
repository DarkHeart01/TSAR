# TSAR — JOCKY Adversary Detection Framework

TSAR is a cross-platform, evasion-resistant framework for deep-forensic system analysis and adversary detection. It is built around a custom domain-specific language — **JOCKY** — whose compiler pipeline produces analysis scripts that are structurally distinct on every build, rendering signature-based and heuristic-based AV/EDR detection ineffective against the tooling itself.

---

## Overview

Modern security tooling is detectable because it is predictable: standard compiler outputs, fixed API call sequences, and static binaries with stable hashes. TSAR addresses this by embedding evasion into the build process itself — every artifact produced by the framework is structurally unique, with altered control flow, obfuscated imports, and spoofed PE metadata.

The framework covers the full stack: language design, compilation, automated polymorphic delivery, central endpoint management, and kernel-level telemetry collection.

---

## Architecture

```
┌──────────────────────────────────────────────────────────────────────┐
│                          JOCKY Framework                             │
│                                                                      │
│  Operator                                                            │
│  ┌────────────────────┐                                              │
│  │  jocky-framework   │  CLI shell + encrypted source upload         │
│  │  (operator CLI)    │                                              │
│  └─────────┬──────────┘                                              │
│            │ .jky / .cpp source                                      │
│            ▼                                                         │
│  ┌──────────────┐  LLVM IR  ┌──────────────────────┐                │
│  │  JOCKY DSL   │ ────────► │   Polaris Compiler   │                │
│  │  Frontend    │           │  (obf + poly + PE)   │                │
│  └──────────────┘           └──────────┬───────────┘                │
│                                        │ binary artifact             │
│                                        ▼                             │
│                            ┌──────────────────────┐                 │
│                            │   CI/CD Pipeline     │  (auto regen)   │
│                            └──────────┬───────────┘                 │
│                                       │ deployed binary              │
│              ┌────────────────────────┼──────────────────────┐      │
│              ▼                        ▼                      ▼      │
│  ┌──────────────────┐   ┌─────────────────────┐  ┌──────────────┐  │
│  │  Endpoint Mgmt   │   │    BYOVD Module      │  │  Operator    │  │
│  │     Server       │   │  + Process Hollowing │  │  Dashboard   │  │
│  └──────────────────┘   └─────────────────────┘  └──────────────┘  │
└──────────────────────────────────────────────────────────────────────┘
```

---

## Components

| Directory | Description |
|---|---|
| [`jocky_v1/`](jocky_v1/) | JOCKY DSL compiler frontend — lexer, parser, AST, semantic analysis, LLVM IR codegen |
| [`compiler/`](compiler/) | Polaris-based compilation pipeline — obfuscation passes, polymorphic engine, PE spoofing |
| [`jocky-framework/`](jocky-framework/) | Operator CLI and build server — interactive shell, encrypted source upload, compiler invocation |
| [`cicd/`](cicd/) | Automated build service — FastAPI gateway + Redis queue + containerised build workers |
| [`endpoint-management-server/`](endpoint-management-server/) | Central management backend — Go/Gin, PostgreSQL, Redis; agent registration, task dispatch, telemetry |
| [`byovd/`](byovd/) | Kernel-mode driver component — process enumeration, privilege escalation, EDR callback removal |
| [`processhollowing/`](processhollowing/) | In-memory execution module — process hollowing via direct NT syscalls with Hell's Gate SSN resolution |
| [`c2-client/`](c2-client/) | Operator dashboard — React/Vite frontend + Go backend; payload builder, live telemetry, endpoint view |

---

## Problem Statement

**SIH 2026 — PS 26148 (NTRO)**

The goal is to build a next-generation scripting and analysis framework capable of defeating AV/EDR restrictions that would otherwise prevent forensic tooling from executing on target systems. Key requirements:

1. **Language-independent IR** — a custom language frontend that alters control-flow graphs, token generation, and binary structures, making signature-based detection ineffective.
2. **Polymorphic delivery** — every build iteration produces a structurally distinct binary through automated obfuscation and a polymorphic engine; no two deployments share the same hash, entry point, or import table.
3. **Living-off-the-Land execution** — in-memory execution techniques (process hollowing, direct syscalls) and BYOVD for kernel-level telemetry collection without touching disk.
4. **Central management** — a management interface routing traffic through trusted cloud infrastructure, supporting simultaneous analysis of multiple endpoints.

---

## Quick Start

### Prerequisites

- Windows x64 (compiler and BYOVD modules)
- Docker (CI/CD pipeline and management server)
- Python 3.10+, Go 1.22+, Node.js 20+
- LLVM 16: `winget install LLVM.LLVM`

### Compile a JOCKY script

```powershell
cd compiler
.\jocky\driver\jocky.exe payload.jky -o payload.exe
```

### Start the management server

```bash
cd endpoint-management-server
cp .env.example .env
docker compose up --build
```

### Start the CI/CD build service

```bash
cd cicd
cp .env.example .env
docker compose up --build
```

### Start the operator dashboard

```bash
cd c2-client
docker compose up --build
# UI: http://localhost:3000
```

---

## Repository Layout

```
TSAR/
├── jocky_v1/                     JOCKY DSL language frontend
├── compiler/                     Polaris compiler + runtime + tools
├── jocky-framework/              Operator CLI shell and build server
├── cicd/                         Automated polymorphic build service
├── endpoint-management-server/   Central management backend
├── byovd/                        Kernel driver + user-mode client
├── processhollowing/             In-memory process hollowing module
└── c2-client/                    Operator dashboard (frontend + backend)
```
