from fastapi import FastAPI

from . import config, db
from .auth import router as auth_router
from .builds import router as builds_router

app = FastAPI(title="jocky server")


@app.on_event("startup")
def startup() -> None:
    db.init_db()
    db.seed_tokens(config.load_tokens())


app.include_router(auth_router)
app.include_router(builds_router)
