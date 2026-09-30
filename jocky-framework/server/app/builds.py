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
from fastapi.responses import FileResponse

from . import compiler, config, crypto, db
from .auth import require_token
from .models import BuildCreatedResponse, BuildLogsResponse, BuildSummary

router = APIRouter()

# build_id -> {"token": str, "lines": list[str], "status": str, "exit_code": int | None}
# In-memory only: no persistent connection to clients, they poll this state over plain HTTP.
build_states: dict[str, dict] = {}


@router.post("/api/build", response_model=BuildCreatedResponse)
async def create_build(
    background_tasks: BackgroundTasks,
    file: UploadFile = File(...),
    output: str = Form(...),
    passes: str = Form(""),
    token: str = Depends(require_token),
) -> BuildCreatedResponse:
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
    db.create_build(build_id, token, file.filename, output, passes, created_at)
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
        except Exception as exc:  # compiler binary missing, bad args, etc.
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
        raise HTTPException(status_code=404, detail="unknown build_id")
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
def list_sessions(token: str = Depends(require_token)) -> list[dict]:
    return db.list_builds(token)


@router.get("/api/build/{build_id}/artifact")
def download_artifact(build_id: str, token: str = Depends(require_token)) -> FileResponse:
    "Download the compiled artifact for a completed build."
    state = build_states.get(build_id)
    if state is None:
        raise HTTPException(status_code=404, detail="unknown build_id")
    if state["token"] != token:
        raise HTTPException(status_code=403, detail="not your build")
    if state["status"] != "success":
        raise HTTPException(status_code=409, detail="build not complete or failed")

    row = db.get_build(build_id)
    if row is None:
        raise HTTPException(status_code=404, detail="build record missing")

    artifact_path = config.DATA_DIR / build_id / row["output_name"]
    if not artifact_path.exists():
        raise HTTPException(status_code=404, detail="artifact file missing from disk")

    return FileResponse(
        path=str(artifact_path),
        media_type="application/octet-stream",
        filename=row["output_name"],
    )
