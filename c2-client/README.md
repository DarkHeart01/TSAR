# JOCKY Operator Dashboard

The operator dashboard is the management UI for the JOCKY framework. It provides a real-time view of registered endpoints, a payload builder that triggers the Polaris compiler pipeline, and a live telemetry stream.

Built with React + Vite + Tailwind CSS on the frontend and Go/Gin on the backend.

---

## Architecture

```
Browser
  │
  │  HTTP  (Vite dev proxy / nginx in prod)
  ▼
┌────────────────────────┐
│   React frontend       │
│   src/                 │   Vite + Tailwind + lucide-react
│   ├── Endpoint list    │
│   ├── Payload builder  │
│   └── Telemetry stream │
└───────────┬────────────┘
            │  /api/*  proxied
            ▼
┌────────────────────────┐
│   Go / Gin backend     │
│   backend/             │
│   ├── endpoints.go     │   Agent registration and status
│   ├── build.go         │   Payload build trigger → CI/CD pipeline
│   └── telemetry.go     │   Telemetry ingestion and streaming
└────────────────────────┘
```

---

## Features

- **Endpoint Monitoring** — real-time list of registered agents with status, last-seen time, and system metadata
- **Payload Builder** — form-driven interface for configuring and triggering a Polaris build job via the CI/CD API; displays job status and downloads the artifact on completion
- **Telemetry Stream** — live colour-coded event feed showing agent activity, task results, and system events

---

## Running

### Docker (recommended)

```bash
cd c2-client
docker compose up --build
```

- Dashboard: `http://localhost:3000`
- Backend API: `http://localhost:8080`

### Local development

**Backend:**

```bash
cd c2-client/backend
go run .
```

**Frontend:**

```bash
cd c2-client/frontend
npm install
npm run dev
```

The Vite dev server proxies `/api` requests to `http://localhost:8080`.

---

## Directory Layout

```
c2-client/
├── docker-compose.yml
├── backend/
│   ├── Dockerfile
│   ├── main.go              Gin server, route wiring
│   ├── handlers/
│   │   ├── endpoints.go     Agent registration and status endpoints
│   │   ├── build.go         Build trigger and artifact retrieval
│   │   └── telemetry.go     Telemetry ingestion and SSE stream
│   ├── go.mod
│   └── go.sum
└── frontend/
    ├── Dockerfile
    ├── nginx.conf           Production static file server + API proxy
    ├── index.html
    ├── src/                 React components and pages
    └── dist/                Production build output
```
