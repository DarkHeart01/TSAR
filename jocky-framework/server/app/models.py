from pydantic import BaseModel


class ConnectRequest(BaseModel):
    token: str


class ConnectResponse(BaseModel):
    client_name: str


class BuildCreatedResponse(BaseModel):
    build_id: str


class BuildLogsResponse(BaseModel):
    lines: list[str]
    next_offset: int
    status: str
    exit_code: int | None


class BuildSummary(BaseModel):
    id: str
    filename: str
    output_name: str
    status: str
    exit_code: int | None
    created_at: str
