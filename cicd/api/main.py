import json
import os
import re
import uuid
from datetime import datetime, timezone
from pathlib import Path

import redis
from fastapi import FastAPI, HTTPException
from fastapi.responses import FileResponse

from models import BuildRequest, BuildStatusResponse

REDIS_HOST = os.environ.get("REDIS_HOST", "redis")
REDIS_PORT = int(os.environ.get("REDIS_PORT", 6379))
ARTIFACT_DIR = Path(os.environ.get("ARTIFACT_DIR", "/build_artifacts"))
QUEUE_KEY = "build_queue:pending"
JOB_ID_RE = re.compile(r"^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$")

app = FastAPI(title="CI/CD Build Gateway", version="1.0.0")
r = redis.Redis(host=REDIS_HOST, port=REDIS_PORT, decode_responses=True)


def job_key(job_id: str) -> str:
    return f"job:{job_id}"


def now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


def validate_job_id(job_id: str) -> str:
    # job_id is server-generated (uuid4) but we still validate on the way
    # back in since it flows into a Redis key and a filesystem path.
    if not JOB_ID_RE.match(job_id):
        raise HTTPException(status_code=400, detail="Invalid job_id format")
    return job_id


@app.post("/v1/build/trigger", status_code=202)
def trigger_build(request: BuildRequest):
    job_id = str(uuid.uuid4())
    payload = {
        "job_id": job_id,
        "source_files": request.source_files,
        "build_flags": request.build_flags,
        "cmake_flags": request.cmake_flags,
        "optimization": request.optimization,
        "timeout_seconds": request.timeout_seconds,
    }

    pipe = r.pipeline()
    pipe.hset(
        job_key(job_id),
        mapping={
            "status": "QUEUED",
            "created_at": now_iso(),
            "started_at": "",
            "finished_at": "",
            "error": "",
            "log_tail": "",
        },
    )
    pipe.lpush(QUEUE_KEY, json.dumps(payload))
    pipe.execute()

    return {"job_id": job_id, "status": "QUEUED"}


@app.get("/v1/build/status/{job_id}", response_model=BuildStatusResponse)
def get_status(job_id: str):
    validate_job_id(job_id)
    data = r.hgetall(job_key(job_id))
    if not data:
        raise HTTPException(status_code=404, detail="Job not found")
    return BuildStatusResponse(
        job_id=job_id,
        status=data.get("status", "UNKNOWN"),
        created_at=data.get("created_at") or None,
        started_at=data.get("started_at") or None,
        finished_at=data.get("finished_at") or None,
        error=data.get("error") or None,
        log_tail=data.get("log_tail") or None,
    )


@app.get("/v1/build/artifact/{job_id}")
def get_artifact(job_id: str):
    validate_job_id(job_id)
    data = r.hgetall(job_key(job_id))
    if not data:
        raise HTTPException(status_code=404, detail="Job not found")
    if data.get("status") != "SUCCESS":
        raise HTTPException(
            status_code=409,
            detail=f"Artifact not available, job status is {data.get('status')}",
        )

    resolved_root = ARTIFACT_DIR.resolve()
    artifact_path = (ARTIFACT_DIR / job_id / "output_binary").resolve()
    if not str(artifact_path).startswith(str(resolved_root)):
        raise HTTPException(status_code=400, detail="Invalid artifact path")
    if not artifact_path.is_file():
        raise HTTPException(status_code=404, detail="Artifact file missing")

    return FileResponse(
        path=artifact_path,
        filename=f"{job_id}_build_output",
        media_type="application/octet-stream",
    )


@app.get("/healthz")
def healthz():
    try:
        r.ping()
        return {"status": "ok"}
    except redis.RedisError:
        raise HTTPException(status_code=503, detail="Redis unavailable")
