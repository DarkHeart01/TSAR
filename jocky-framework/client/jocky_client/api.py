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

    def submit_build(self, source_path: Path, output_name: str, passes: str) -> str:
        plaintext = source_path.read_bytes()
        key = crypto.derive_key(self.token)
        encrypted = crypto.encrypt(key, plaintext, associated_data=source_path.name.encode())

        files = {"file": (source_path.name, encrypted, "application/octet-stream")}
        data = {"output": output_name, "passes": passes}
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

    def sessions(self) -> list[dict]:
        resp = requests.get(
            f"{self.base_url}/api/sessions", headers=self._headers(), timeout=10
        )
        resp.raise_for_status()
        return resp.json()

    def download_artifact(self, build_id: str) -> bytes:
        resp = requests.get(
            f"{self.base_url}/api/build/{build_id}/artifact",
            headers=self._headers(),
            timeout=120,
            stream=True,
        )
        resp.raise_for_status()
        return resp.content
