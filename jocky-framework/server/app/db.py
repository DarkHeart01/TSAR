import sqlite3
from contextlib import contextmanager

from . import config


@contextmanager
def get_conn():
    conn = sqlite3.connect(config.DB_PATH)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA foreign_keys = ON")
    try:
        yield conn
    finally:
        conn.close()


def init_db() -> None:
    with get_conn() as conn:
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS tokens (
                token      TEXT PRIMARY KEY,
                client_name TEXT NOT NULL
            )
            """
        )
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS targets (
                id         TEXT PRIMARY KEY,
                token      TEXT NOT NULL,
                name       TEXT NOT NULL,
                host       TEXT NOT NULL,
                os         TEXT,
                notes      TEXT,
                created_at TEXT NOT NULL
            )
            """
        )
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS builds (
                id          TEXT PRIMARY KEY,
                token       TEXT NOT NULL,
                target_id   TEXT,
                filename    TEXT NOT NULL,
                output_name TEXT NOT NULL,
                passes      TEXT,
                status      TEXT NOT NULL,
                exit_code   INTEGER,
                log         TEXT,
                created_at  TEXT NOT NULL,
                FOREIGN KEY (target_id) REFERENCES targets(id)
            )
            """
        )
        # migration: add target_id column when upgrading from an older schema
        try:
            conn.execute("ALTER TABLE builds ADD COLUMN target_id TEXT REFERENCES targets(id)")
        except Exception:
            pass
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


# ── Targets ──────────────────────────────────────────────────────────────────

def create_target(
    target_id: str,
    token: str,
    name: str,
    host: str,
    os: str | None,
    notes: str | None,
    created_at: str,
) -> None:
    with get_conn() as conn:
        conn.execute(
            "INSERT INTO targets (id, token, name, host, os, notes, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)",
            (target_id, token, name, host, os, notes, created_at),
        )
        conn.commit()


def list_targets(token: str) -> list[dict]:
    with get_conn() as conn:
        rows = conn.execute(
            "SELECT id, name, host, os, notes, created_at FROM targets WHERE token = ? ORDER BY created_at DESC",
            (token,),
        ).fetchall()
        return [dict(r) for r in rows]


def get_target(target_id: str, token: str) -> dict | None:
    with get_conn() as conn:
        row = conn.execute(
            "SELECT id, name, host, os, notes, created_at FROM targets WHERE id = ? AND token = ?",
            (target_id, token),
        ).fetchone()
        return dict(row) if row else None


# ── Builds ───────────────────────────────────────────────────────────────────

def create_build(
    build_id: str,
    token: str,
    target_id: str | None,
    filename: str,
    output_name: str,
    passes: str,
    created_at: str,
) -> None:
    with get_conn() as conn:
        conn.execute(
            """
            INSERT INTO builds (id, token, target_id, filename, output_name, passes,
                                status, exit_code, log, created_at)
            VALUES (?, ?, ?, ?, ?, ?, 'pending', NULL, '', ?)
            """,
            (build_id, token, target_id, filename, output_name, passes, created_at),
        )
        conn.commit()


def update_build(build_id: str, status: str, exit_code: int, log: str) -> None:
    with get_conn() as conn:
        conn.execute(
            "UPDATE builds SET status = ?, exit_code = ?, log = ? WHERE id = ?",
            (status, exit_code, log, build_id),
        )
        conn.commit()


def list_builds(token: str, target_id: str | None = None) -> list[dict]:
    with get_conn() as conn:
        if target_id:
            rows = conn.execute(
                """
                SELECT b.id, b.target_id, b.filename, b.output_name, b.status,
                       b.exit_code, b.created_at,
                       t.name AS target_name, t.host AS target_host
                FROM builds b
                LEFT JOIN targets t ON b.target_id = t.id
                WHERE b.token = ? AND b.target_id = ?
                ORDER BY b.created_at DESC
                """,
                (token, target_id),
            ).fetchall()
        else:
            rows = conn.execute(
                """
                SELECT b.id, b.target_id, b.filename, b.output_name, b.status,
                       b.exit_code, b.created_at,
                       t.name AS target_name, t.host AS target_host
                FROM builds b
                LEFT JOIN targets t ON b.target_id = t.id
                WHERE b.token = ?
                ORDER BY b.created_at DESC
                """,
                (token,),
            ).fetchall()
        return [dict(r) for r in rows]
