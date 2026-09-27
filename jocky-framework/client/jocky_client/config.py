import json
from pathlib import Path

CONFIG_PATH = Path.home() / ".jocky" / "config.json"


def save(addr: str, token: str, target_id: str | None = None) -> None:
    CONFIG_PATH.parent.mkdir(parents=True, exist_ok=True)
    data: dict = {"addr": addr, "token": token}
    if target_id is not None:
        data["target_id"] = target_id
    elif CONFIG_PATH.exists():
        existing = load()
        if existing and "target_id" in existing:
            data["target_id"] = existing["target_id"]
    CONFIG_PATH.write_text(json.dumps(data))


def save_target(target_id: str | None) -> None:
    existing = load() or {}
    existing["target_id"] = target_id
    CONFIG_PATH.parent.mkdir(parents=True, exist_ok=True)
    CONFIG_PATH.write_text(json.dumps(existing))


def load() -> dict | None:
    if not CONFIG_PATH.exists():
        return None
    try:
        return json.loads(CONFIG_PATH.read_text())
    except (json.JSONDecodeError, OSError):
        return None
