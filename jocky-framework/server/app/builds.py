import uuid
from datetime import datetime, timezone

from fastapi import (
    APIRouter,
    BackgroundTasks,
    Depends,
    File,
    Form,
    HTTPException,
    UploadFile,
)

from . import compiler, config, crypto, db
from .auth import require_token
from .models import BuildCreatedResponse, BuildLogsResponse, BuildSummary, OpsEntry

router = APIRouter()

# build_id -> {"token": str, "lines": list[str], "status": str, "exit_code": int | None}
# In-memory only — no persistent connection. Clients poll over plain HTTP.
build_states: dict[str, dict] = {}


@router.post("/api/build", response_model=BuildCreatedResponse)
async def create_build(
    background_tasks: BackgroundTasks,
    file: UploadFile = File(...),
    output: str = Form(...),
    passes: str = Form("fla,sub,api-hash"),
    target_id: str | None = Form(None),
    token: str = Depends(require_token),
) -> BuildCreatedResponse:
    if not (file.filename or "").lower().endswith(".jky"):
        raise HTTPException(
            status_code=400,
            detail="only .jky source files are accepted",
        )

    if target_id is not None:
        if db.get_target(target_id, token) is None:
            raise HTTPException(status_code=404, detail="target not found or not yours")

    build_id = uuid.uuid4().hex
    build_dir = config.DATA_DIR / build_id
    build_dir.mkdir(parents=True, exist_ok=True)

    encrypted_bytes = await file.read()
    key = crypto.derive_key(token)
    try:
        source_bytes = crypto.decrypt(
            key, encrypted_bytes, associated_data=file.filename.encode()
        )
    except Exception:
        raise HTTPException(
            status_code=400,
            detail="failed to decrypt uploaded file (wrong token or corrupted upload)",
        )

    source_path = build_dir / file.filename
    source_path.write_bytes(source_bytes)
    output_path = build_dir / output

    created_at = datetime.now(timezone.utc).isoformat()
    db.create_build(build_id, token, target_id, file.filename, output, passes, created_at)
    build_states[build_id] = {
        "token": token,
        "lines": [],
        "status": "pending",
        "exit_code": None,
    }

    async def _run() -> None:
        state = build_states[build_id]
        state["status"] = "running"
        try:
            status, exit_code, log = await compiler.run_compile(
                source_path, output_path, passes, lambda line: state["lines"].append(line)
            )
        except Exception as exc:
            status, exit_code, log = "failed", -1, str(exc)
            state["lines"].append(f"server error: {exc}")
        state["status"] = status
        state["exit_code"] = exit_code
        db.update_build(build_id, status, exit_code, log)

    background_tasks.add_task(_run)
    return BuildCreatedResponse(build_id=build_id)


@router.get("/api/build/{build_id}/logs", response_model=BuildLogsResponse)
def poll_build_logs(
    build_id: str,
    offset: int = 0,
    token: str = Depends(require_token),
) -> BuildLogsResponse:
    state = build_states.get(build_id)
    if state is None:
        raise HTTPException(status_code=404, detail="unknown build_id (may have been lost on server restart)")
    if state["token"] != token:
        raise HTTPException(status_code=403, detail="not your build")

    new_lines = state["lines"][offset:]
    return BuildLogsResponse(
        lines=new_lines,
        next_offset=offset + len(new_lines),
        status=state["status"],
        exit_code=state["exit_code"],
    )


@router.get("/api/sessions", response_model=list[BuildSummary])
def list_sessions(
    target_id: str | None = None,
    token: str = Depends(require_token),
) -> list[dict]:
    return db.list_builds(token, target_id=target_id)


@router.get("/api/ops", response_model=list[OpsEntry])
def get_ops(
    status: str | None = None,
    token: str = Depends(require_token),
) -> list[OpsEntry]:
    """All builds for this operator, enriched with live in-memory state for running builds."""
    rows = db.list_builds(token)
    result = []
    for row in rows:
        build_id = row["id"]
        mem = build_states.get(build_id)

        effective_status = mem["status"] if mem else row["status"]
        effective_exit_code = mem["exit_code"] if mem else row["exit_code"]
        last_line = mem["lines"][-1] if (mem and mem["lines"]) else None

        if status and effective_status != status:
            continue

        result.append(
            OpsEntry(
                build_id=build_id,
                target_id=row.get("target_id"),
                target_name=row.get("target_name"),
                target_host=row.get("target_host"),
                filename=row["filename"],
                output_name=row["output_name"],
                status=effective_status,
                exit_code=effective_exit_code,
                last_line=last_line,
                created_at=row["created_at"],
            )
        )
    return result
