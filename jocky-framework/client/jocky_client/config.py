import json
from pathlib import Path

CONFIG_PATH = Path.home() / ".jocky" / "config.json"


def save(addr: str, token: str) -> None:
    CONFIG_PATH.parent.mkdir(parents=True, exist_ok=True)
    CONFIG_PATH.write_text(json.dumps({"addr": addr, "token": token}))


def load() -> dict | None:
    if not CONFIG_PATH.exists():
        return None
    try:
        return json.loads(CONFIG_PATH.read_text())
    except (json.JSONDecodeError, OSError):
        return None
