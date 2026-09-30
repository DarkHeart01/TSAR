import json
from pathlib import Path

CONFIG_PATH = Path.home() / ".jocky" / "config.json"


def save(addr: str, token: str) -> None:
    CONFIG_PATH.parent.mkdir(parents=True, exist_ok=True)
    data = load() or {}
    data["addr"]  = addr
    data["token"] = token
    CONFIG_PATH.write_text(json.dumps(data))


def save_c2(c2_addr: str, c2_jwt: str) -> None:
    CONFIG_PATH.parent.mkdir(parents=True, exist_ok=True)
    data = load() or {}
    data["c2_addr"] = c2_addr
    data["c2_jwt"]  = c2_jwt
    CONFIG_PATH.write_text(json.dumps(data))


def load() -> dict | None:
    if not CONFIG_PATH.exists():
        return None
    try:
        return json.loads(CONFIG_PATH.read_text())
    except (json.JSONDecodeError, OSError):
        return None
