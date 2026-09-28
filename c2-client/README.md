# JOCKY Operator Dashboard

> A React-based operator dashboard for managing JOCKY framework endpoints. Provides real-time endpoint monitoring, an integrated polymorphic payload builder, and a live telemetry stream — all in a single-page application backed by a lightweight Go API server.

---

## Overview

The operator dashboard is the primary visual interface for TSAR. While the endpoint management server handles all backend state, the dashboard provides the operator-facing view: which agents are online, what tasks are running, what telemetry is coming in, and how to build and deploy new payloads.

The dashboard has two layers:

- **Frontend** (`frontend/`) — a React/Vite single-page application served by Nginx, communicating with the backend over `/api`
- **Backend** (`backend/`) — a Go/Gin API server that proxies operator requests to the endpoint management server and the CI/CD build service, and aggregates telemetry for the UI

This separation keeps the frontend stateless. The Go backend handles authentication, data aggregation, and integration with the rest of the TSAR stack.

---

## Architecture

```
Operator browser
        │
        │  HTTP   (Nginx in production, Vite dev server in development)
        ▼
┌──────────────────────────────────────────────────────────────────┐
│  React frontend (Vite + Tailwind + lucide-react)                 │
│                                                                  │
│  ┌─────────────────────┐  ┌─────────────────┐  ┌─────────────┐  │
│  │  Endpoint Monitor   │  │ Payload Builder  │  │  Telemetry  │  │
│  │  - Agent list       │  │ - Source input   │  │  Stream     │  │
│  │  - Online / offline │  │ - Pass selection │  │  - Live feed│  │
│  │  - Last seen        │  │ - Build trigger  │  │  - Coloured │  │
│  │  - System metadata  │  │ - Log streaming  │  │    by type  │  │
│  └─────────────────────┘  └─────────────────┘  └─────────────┘  │
└─────────────────────────────────┬────────────────────────────────┘
                                  │  /api/* proxied
                                  ▼
┌──────────────────────────────────────────────────────────────────┐
│  Go / Gin backend                                                │
│                                                                  │
│  handlers/endpoints.go    agent list, registration, status      │
│  handlers/build.go        payload build trigger + artifact fetch │
│  handlers/telemetry.go    telemetry ingestion + SSE stream       │
│                                                                  │
│  Integrates with:                                                │
│  - endpoint-management-server  (agent data, task management)    │
│  - cicd/                       (build job trigger + poll)        │
└──────────────────────────────────────────────────────────────────┘
```

---

## Features

### Endpoint Monitor

Displays all registered agents in real time with:
- Connection status (online / offline) with last-seen timestamp
- System metadata submitted at registration (OS version, hostname, architecture)
- Per-agent task queue depth
- Quick-action buttons to enqueue tasks or view telemetry history

Online/offline state is driven by the endpoint management server's stale-agent sweeper — an agent that has not polled within 120 seconds is marked offline and the dashboard reflects this within the next polling cycle.

### Payload Builder

A form-driven interface for triggering a Polaris build through the CI/CD service:

1. Source file input (paste or upload a `.jky` or `.cpp` source)
2. Obfuscation pass selection (checkboxes for `fla`, `sub`, `api-hash`, `mba`, `indcall`)
3. Build trigger — submits to the CI/CD API and polls for job status
4. Live compiler log display — streams build output as it arrives
5. Artifact download — once the build completes successfully, the compiled binary is available for download or direct deployment to a selected agent

Every build triggered from the dashboard goes through the full Polaris pipeline with a fresh random seed, producing a unique binary regardless of how many times the same source is compiled.

### Telemetry Stream

A live, colour-coded event feed showing:
- Agent check-ins and registrations
- Task enqueue and completion events
- Telemetry payloads submitted by agents (logs, scan results, process lists)
- System events (agents going offline, tasks expiring)

Events are streamed using Server-Sent Events (SSE) from the Go backend's `/api/telemetry/stream` endpoint, updating in real time without polling.

---

## Running

### Docker (recommended)

```bash
cd c2-client
docker compose up --build
```

| Service | URL |
|---|---|
| Dashboard | `http://localhost:3000` |
| Backend API | `http://localhost:8080` |

### Local Development

**Backend:**

```bash
cd c2-client/backend
go run .
# API server starts on :8080
```

**Frontend:**

```bash
cd c2-client/frontend
npm install
npm run dev
# Vite dev server starts on :5173
# /api requests are proxied to http://localhost:8080
```

The Vite proxy configuration in `vite.config.js` handles `/api` forwarding automatically. No CORS configuration is needed in development.

---

## Configuration

The backend reads configuration from environment variables (or a `.env` file):

| Variable | Default | Description |
|---|---|---|
| `PORT` | `8080` | Backend HTTP port |
| `MGMT_SERVER_URL` | — | URL of the endpoint management server |
| `CICD_API_URL` | — | URL of the CI/CD build service API |
| `OPERATOR_TOKEN` | — | Operator bearer token for the management server |

---

## Directory Layout

```
c2-client/
│
├── docker-compose.yml           Orchestrates backend + frontend containers
│
├── backend/
│   ├── Dockerfile
│   ├── main.go                  Gin server entry point, route wiring
│   ├── handlers/
│   │   ├── endpoints.go         Agent list, status, registration forwarding
│   │   ├── build.go             Build job trigger, status polling, artifact download
│   │   └── telemetry.go        Telemetry ingestion and SSE stream
│   ├── go.mod
│   └── go.sum
│
└── frontend/
    ├── Dockerfile
    ├── nginx.conf               Production static file server + /api reverse proxy
    ├── index.html               SPA entry point
    ├── vite.config.js           Vite config with /api proxy for development
    ├── tailwind.config.js
    ├── src/
    │   ├── main.jsx             React entry point
    │   ├── App.jsx              Top-level layout and routing
    │   ├── components/
    │   │   ├── EndpointList.jsx     Agent table with status indicators
    │   │   ├── PayloadBuilder.jsx   Build form with live log display
    │   │   └── TelemetryStream.jsx  SSE-fed live event feed
    │   └── api/
    │       └── client.js        Axios-based API client for the Go backend
    └── dist/                    Production build output (served by Nginx)
```

---

## Relationship to Other Components

| Component | Relationship |
|---|---|
| `endpoint-management-server/` | The primary data source — the backend proxies agent, task, and telemetry data from the management server's API |
| `cicd/` | The payload builder triggers build jobs on the CI/CD service and polls for completion |
| `jocky-framework/` | Alternative interface for the same build pipeline; the dashboard provides a visual equivalent of the CLI's `build` command |
| `compiler/` | The artifact produced by triggered builds is a Polaris-compiled binary, downloadable from the dashboard |
