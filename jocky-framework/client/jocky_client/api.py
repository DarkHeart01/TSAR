from pathlib import Path

import requests

from . import crypto


class ApiClient:
    def __init__(self, addr: str, token: str):
        self.base_url = f"http://{addr}"
        self.token = token

    def _headers(self) -> dict:
        return {"Authorization": f"Bearer {self.token}"}

    def connect(self) -> dict:
        resp = requests.post(
            f"{self.base_url}/api/connect", json={"token": self.token}, timeout=10
        )
        resp.raise_for_status()
        return resp.json()

    # ── Targets ──────────────────────────────────────────────────────────────

    def add_target(self, name: str, host: str, os: str | None, notes: str | None) -> dict:
        resp = requests.post(
            f"{self.base_url}/api/target",
            headers=self._headers(),
            json={"name": name, "host": host, "os": os, "notes": notes},
            timeout=10,
        )
        resp.raise_for_status()
        return resp.json()

    def list_targets(self) -> list[dict]:
        resp = requests.get(
            f"{self.base_url}/api/targets", headers=self._headers(), timeout=10
        )
        resp.raise_for_status()
        return resp.json()

    def get_target(self, target_id: str) -> dict:
        resp = requests.get(
            f"{self.base_url}/api/target/{target_id}", headers=self._headers(), timeout=10
        )
        resp.raise_for_status()
        return resp.json()

    # ── Builds ───────────────────────────────────────────────────────────────

    def submit_build(
        self,
        source_path: Path,
        output_name: str,
        passes: str,
        target_id: str | None = None,
    ) -> str:
        if not source_path.suffix.lower() == ".jky":
            raise ValueError(f"only .jky files are accepted, got: {source_path.name}")

        plaintext = source_path.read_bytes()
        key = crypto.derive_key(self.token)
        encrypted = crypto.encrypt(key, plaintext, associated_data=source_path.name.encode())

        files = {"file": (source_path.name, encrypted, "application/octet-stream")}
        data: dict = {"output": output_name, "passes": passes}
        if target_id:
            data["target_id"] = target_id

        resp = requests.post(
            f"{self.base_url}/api/build",
            headers=self._headers(),
            files=files,
            data=data,
            timeout=30,
        )
        resp.raise_for_status()
        return resp.json()["build_id"]

    def poll_build_logs(self, build_id: str, offset: int) -> dict:
        resp = requests.get(
            f"{self.base_url}/api/build/{build_id}/logs",
            headers=self._headers(),
            params={"offset": offset},
            timeout=10,
        )
        resp.raise_for_status()
        return resp.json()

    def sessions(self, target_id: str | None = None) -> list[dict]:
        params: dict = {}
        if target_id:
            params["target_id"] = target_id
        resp = requests.get(
            f"{self.base_url}/api/sessions",
            headers=self._headers(),
            params=params,
            timeout=10,
        )
        resp.raise_for_status()
        return resp.json()

    def get_ops(self, status: str | None = None) -> list[dict]:
        params: dict = {}
        if status:
            params["status"] = status
        resp = requests.get(
            f"{self.base_url}/api/ops",
            headers=self._headers(),
            params=params,
            timeout=10,
        )
        resp.raise_for_status()
        return resp.json()
