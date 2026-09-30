import sqlite3
from contextlib import contextmanager

from . import config


@contextmanager
def get_conn():
    conn = sqlite3.connect(config.DB_PATH)
    conn.row_factory = sqlite3.Row
    try:
        yield conn
    finally:
        conn.close()


def init_db() -> None:
    with get_conn() as conn:
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS tokens (
                token TEXT PRIMARY KEY,
                client_name TEXT NOT NULL
            )
            """
        )
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS builds (
                id TEXT PRIMARY KEY,
                token TEXT NOT NULL,
                filename TEXT NOT NULL,
                output_name TEXT NOT NULL,
                passes TEXT,
                status TEXT NOT NULL,
                exit_code INTEGER,
                log TEXT,
                created_at TEXT NOT NULL
            )
            """
        )
        conn.commit()


def seed_tokens(tokens: dict[str, str]) -> None:
    with get_conn() as conn:
        for token, name in tokens.items():
            conn.execute(
                "INSERT OR REPLACE INTO tokens (token, client_name) VALUES (?, ?)",
                (token, name),
            )
        conn.commit()


def get_client_name(token: str) -> str | None:
    with get_conn() as conn:
        row = conn.execute(
            "SELECT client_name FROM tokens WHERE token = ?", (token,)
        ).fetchone()
        return row["client_name"] if row else None


def create_build(
    build_id: str,
    token: str,
    filename: str,
    output_name: str,
    passes: str,
    created_at: str,
) -> None:
    with get_conn() as conn:
        conn.execute(
            """
            INSERT INTO builds (id, token, filename, output_name, passes, status, exit_code, log, created_at)
            VALUES (?, ?, ?, ?, ?, 'pending', NULL, '', ?)
            """,
            (build_id, token, filename, output_name, passes, created_at),
        )
        conn.commit()


def update_build(build_id: str, status: str, exit_code: int, log: str) -> None:
    with get_conn() as conn:
        conn.execute(
            "UPDATE builds SET status = ?, exit_code = ?, log = ? WHERE id = ?",
            (status, exit_code, log, build_id),
        )
        conn.commit()


def get_build(build_id: str) -> dict | None:
    with get_conn() as conn:
        row = conn.execute(
            "SELECT id, token, filename, output_name, passes, status, exit_code, created_at FROM builds WHERE id = ?",
            (build_id,),
        ).fetchone()
        return dict(row) if row else None


def list_builds(token: str) -> list[dict]:
    with get_conn() as conn:
        rows = conn.execute(
            """
            SELECT id, filename, output_name, status, exit_code, created_at
            FROM builds WHERE token = ? ORDER BY created_at DESC
            """,
            (token,),
        ).fetchall()
        return [dict(r) for r in rows]
