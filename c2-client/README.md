# JOCKY C2 Dashboard

A sophisticated Command & Control dashboard for the JOCKY evasive digital forensics framework.

## Features

- **Real-time Endpoint Monitoring**: Track active connections with detailed system information
- **Polymorphic Payload Builder**: Generate evasive payloads with LLVM obfuscation
- **Live Telemetry Stream**: Monitor operations in real-time with color-coded events
- **Professional Dark Theme**: Cybersecurity-focused UI with neon accents

## Quick Start

### Using Docker Compose (Recommended)

```bash
docker compose up --build
```

Open [http://localhost:3000](http://localhost:3000) for the dashboard. The backend API runs on [http://localhost:8080](http://localhost:8080).

### Local Development

**Backend:**

```bash
cd backend
go run .
```

**Frontend:**

```bash
cd frontend
npm install
npm run dev
```

The Vite dev server proxies `/api` requests to `http://localhost:8080`.

## Project Structure

```
C2/
├── docker-compose.yml    # Orchestrates backend + frontend
├── backend/              # Go/Gin API server
└── frontend/             # React + Vite + Tailwind UI
```
