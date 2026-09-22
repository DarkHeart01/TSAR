import json
import time
import traceback
from datetime import datetime, timezone

from redis_client import get_client
from build_executor import run_build, BuildFailure
from sanitize import UnsafeInputError

QUEUE_KEY = "build_queue:pending"
POLL_BLOCK_SECONDS = 5


def job_key(job_id: str) -> str:
    return f"job:{job_id}"


def now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


def update_job(r, job_id: str, **fields) -> None:
    r.hset(job_key(job_id), mapping=fields)


def process_job(r, payload: dict) -> None:
    job_id = payload["job_id"]
    update_job(r, job_id, status="COMPILING", started_at=now_iso())
    try:
        _, log = run_build(job_id, payload)
        update_job(r, job_id, status="SUCCESS", finished_at=now_iso(), log_tail=log[-4000:], error="")
    except (BuildFailure, UnsafeInputError) as exc:
        log = getattr(exc, "log", "")
        update_job(r, job_id, status="FAILED", finished_at=now_iso(), error=str(exc), log_tail=log[-4000:])
    except Exception as exc:  # noqa: BLE001 - last-resort guard so the loop never dies
        update_job(
            r, job_id,
            status="FAILED",
            finished_at=now_iso(),
            error=f"Internal worker error: {exc}",
            log_tail=traceback.format_exc()[-4000:],
        )


def main() -> None:
    r = get_client()
    print(f"[build-worker] listening on {QUEUE_KEY}", flush=True)
    while True:
        try:
            item = r.brpop(QUEUE_KEY, timeout=POLL_BLOCK_SECONDS)
            if item is None:
                continue
            _, raw_payload = item
            payload = json.loads(raw_payload)
            process_job(r, payload)
        except Exception:
            traceback.print_exc()
            time.sleep(1)


if __name__ == "__main__":
    main()
