import os
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]

DATA_DIR = Path(os.environ.get("JOCKY_DATA_DIR", REPO_ROOT / "server" / "data"))
DB_PATH = Path(os.environ.get("JOCKY_DB_PATH", REPO_ROOT / "server" / "jocky.db"))
COMPILER_PATH = Path(
    os.environ.get("COMPILER_PATH", REPO_ROOT / "compiler" / "jocky" / "driver" / "jocky.exe")
)

DATA_DIR.mkdir(parents=True, exist_ok=True)


def load_tokens() -> dict[str, str]:
    """Parse JOCKY_TOKENS env var: 'token1:name1,token2:name2'."""
    raw = os.environ.get("JOCKY_TOKENS", "")
    tokens: dict[str, str] = {}
    for entry in raw.split(","):
        entry = entry.strip()
        if not entry:
            continue
        token, _, name = entry.partition(":")
        tokens[token] = name or "unnamed"
    return tokens
