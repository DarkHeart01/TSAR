import asyncio
from pathlib import Path
from typing import Callable

from . import config


async def run_compile(
    source_path: Path,
    output_path: Path,
    passes: str,
    on_line: Callable[[str], None],
) -> tuple[str, int, str]:
    """Invoke the custom compiler, calling `on_line` for each stdout line as it runs.

    CLI contract: <COMPILER_PATH> <source> -o <output> "-passes=<passes>" -v
    No persistent connection back to the client here - callers poll for progress instead.
    """
    cmd = [
        str(config.COMPILER_PATH),
        str(source_path),
        "-o",
        str(output_path),
        f"-passes={passes}",
        "-v",
    ]

    process = await asyncio.create_subprocess_exec(
        *cmd,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.STDOUT,
    )

    log_lines: list[str] = []
    async for raw_line in process.stdout:
        line = raw_line.decode(errors="replace").rstrip("\r\n")
        log_lines.append(line)
        on_line(line)

    exit_code = await process.wait()
    status = "success" if exit_code == 0 else "failed"
    return status, exit_code, "\n".join(log_lines)
