from fastapi import APIRouter, Header, HTTPException

from . import db
from .models import ConnectRequest, ConnectResponse

router = APIRouter()


@router.post("/api/connect", response_model=ConnectResponse)
def connect(req: ConnectRequest) -> ConnectResponse:
    client_name = db.get_client_name(req.token)
    if client_name is None:
        raise HTTPException(status_code=401, detail="invalid token")
    return ConnectResponse(client_name=client_name)


def require_token(authorization: str = Header(...)) -> str:
    if not authorization.startswith("Bearer "):
        raise HTTPException(status_code=401, detail="missing bearer token")
    token = authorization.removeprefix("Bearer ").strip()
    if db.get_client_name(token) is None:
        raise HTTPException(status_code=401, detail="invalid token")
    return token
