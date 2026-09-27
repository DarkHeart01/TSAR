from pydantic import BaseModel


class ConnectRequest(BaseModel):
    token: str


class ConnectResponse(BaseModel):
    client_name: str


# ── Targets ──────────────────────────────────────────────────────────────────

class TargetCreateRequest(BaseModel):
    name: str
    host: str
    os: str | None = None
    notes: str | None = None


class TargetResponse(BaseModel):
    id: str
    name: str
    host: str
    os: str | None
    notes: str | None
    created_at: str


# ── Builds ───────────────────────────────────────────────────────────────────

class BuildCreatedResponse(BaseModel):
    build_id: str


class BuildLogsResponse(BaseModel):
    lines: list[str]
    next_offset: int
    status: str
    exit_code: int | None


class BuildSummary(BaseModel):
    id: str
    target_id: str | None
    target_name: str | None
    target_host: str | None
    filename: str
    output_name: str
    status: str
    exit_code: int | None
    created_at: str


class OpsEntry(BaseModel):
    build_id: str
    target_id: str | None
    target_name: str | None
    target_host: str | None
    filename: str
    output_name: str
    status: str
    exit_code: int | None
    last_line: str | None
    created_at: str
