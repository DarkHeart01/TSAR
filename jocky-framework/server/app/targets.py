import uuid
from datetime import datetime, timezone

from fastapi import APIRouter, Depends, HTTPException

from . import db
from .auth import require_token
from .models import TargetCreateRequest, TargetResponse

router = APIRouter()


@router.post("/api/target", response_model=TargetResponse, status_code=201)
def create_target(
    req: TargetCreateRequest,
    token: str = Depends(require_token),
) -> TargetResponse:
    target_id = uuid.uuid4().hex
    created_at = datetime.now(timezone.utc).isoformat()
    db.create_target(target_id, token, req.name, req.host, req.os, req.notes, created_at)
    return TargetResponse(
        id=target_id,
        name=req.name,
        host=req.host,
        os=req.os,
        notes=req.notes,
        created_at=created_at,
    )


@router.get("/api/targets", response_model=list[TargetResponse])
def list_targets(token: str = Depends(require_token)) -> list[dict]:
    return db.list_targets(token)


@router.get("/api/target/{target_id}", response_model=TargetResponse)
def get_target(target_id: str, token: str = Depends(require_token)) -> dict:
    target = db.get_target(target_id, token)
    if target is None:
        raise HTTPException(status_code=404, detail="target not found")
    return target
