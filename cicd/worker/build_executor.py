import os
import shutil
import signal
import subprocess
import resource
from pathlib import Path
from typing import Dict, List, Tuple

from sanitize import safe_filename, safe_flag, safe_cmake_define, UnsafeInputError

WORKSPACE_DIR = Path(os.environ.get("WORKSPACE_DIR", "/build_workspace"))
ARTIFACT_DIR = Path(os.environ.get("ARTIFACT_DIR", "/build_artifacts"))
MAX_BUILD_TIMEOUT = int(os.environ.get("MAX_BUILD_TIMEOUT", 600))

# Minimal, hardcoded environment for every compiler/cmake invocation - the
# request payload never contributes environment variables to a subprocess.
SAFE_ENV = {"PATH": "/usr/bin:/bin", "HOME": "/tmp", "LANG": "C"}


class BuildFailure(Exception):
    def __init__(self, message: str, log: str = ""):
        super().__init__(message)
        self.log = log


def _make_preexec(timeout: int):
    def _limit_resources():
        # Runs in the forked child before exec(). Backstops the wall-clock
        # timeout below in case a compiler process detaches/backgrounds.
        cpu_limit = timeout + 5
        resource.setrlimit(resource.RLIMIT_CPU, (cpu_limit, cpu_limit))
        resource.setrlimit(resource.RLIMIT_AS, (2 * 1024 ** 3, 2 * 1024 ** 3))
        resource.setrlimit(resource.RLIMIT_NOFILE, (256, 256))

    return _limit_resources


def _run(cmd: List[str], cwd: Path, timeout: int) -> Tuple[int, str]:
    proc = subprocess.Popen(
        cmd,
        cwd=str(cwd),
        env=SAFE_ENV,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        preexec_fn=_make_preexec(timeout),
        start_new_session=True,  # own process group, so we can kill children too
        shell=False,
    )
    try:
        stdout, stderr = proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except ProcessLookupError:
            pass
        stdout, stderr = proc.communicate()
        raise BuildFailure(
            f"Command timed out after {timeout}s: {' '.join(cmd)}",
            log=f"$ {' '.join(cmd)}\n{stdout}\n{stderr}\n",
        )

    log = f"$ {' '.join(cmd)}\n{stdout}\n{stderr}\n"
    if proc.returncode != 0:
        raise BuildFailure(f"Command failed with exit code {proc.returncode}: {' '.join(cmd)}", log=log)
    return proc.returncode, log


def _write_sources(job_dir: Path, source_files: Dict[str, str]) -> List[str]:
    written = []
    job_root = job_dir.resolve()
    for name, content in source_files.items():
        safe_name = safe_filename(name)
        dest = (job_dir / safe_name).resolve()
        if not str(dest).startswith(str(job_root)):
            raise UnsafeInputError(f"Resolved path escapes job workspace: {name!r}")
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_text(content)
        written.append(safe_name)
    return written


def _find_executable(build_dir: Path):
    for path in build_dir.rglob("*"):
        if path.is_file() and os.access(path, os.X_OK) and not path.suffix:
            return path
    return None


def run_build(job_id: str, payload: dict) -> Tuple[Path, str]:
    """Compiles submitted sources for one job. Returns (artifact_path, full_log)."""
    job_dir = WORKSPACE_DIR / job_id
    job_dir.mkdir(parents=True, exist_ok=True)
    full_log = ""

    try:
        source_files = payload["source_files"]
        build_flags = [safe_flag(f) for f in payload.get("build_flags", [])]
        cmake_flags = [safe_cmake_define(f) for f in payload.get("cmake_flags", [])]
        optimization = payload.get("optimization", "O2")
        timeout = min(int(payload.get("timeout_seconds", 120)), MAX_BUILD_TIMEOUT)

        written_files = _write_sources(job_dir, source_files)
        output_path = job_dir / "output_binary"

        if "CMakeLists.txt" in written_files:
            build_dir = job_dir / "build"
            build_dir.mkdir(exist_ok=True)

            _, log = _run(
                [
                    "cmake", "-S", str(job_dir), "-B", str(build_dir),
                    "-G", "Ninja",
                    "-DCMAKE_BUILD_TYPE=Release",
                    "-DCMAKE_C_COMPILER=clang",
                    "-DCMAKE_CXX_COMPILER=clang++",
                    f"-DCMAKE_C_FLAGS=-{optimization}",
                    f"-DCMAKE_CXX_FLAGS=-{optimization}",
                    *cmake_flags,
                ],
                cwd=job_dir,
                timeout=timeout,
            )
            full_log += log

            _, log = _run(["cmake", "--build", str(build_dir), "--parallel"], cwd=job_dir, timeout=timeout)
            full_log += log

            compiled = _find_executable(build_dir)
            if compiled is None:
                raise BuildFailure("CMake build finished but no executable artifact was found", log=full_log)
            shutil.copy2(compiled, output_path)
        else:
            c_sources = [f for f in written_files if f.endswith(".c")]
            cxx_sources = [f for f in written_files if f.endswith((".cpp", ".cc", ".cxx"))]
            if not c_sources and not cxx_sources:
                raise BuildFailure("No compilable .c/.cpp source files were provided")

            # Mixed C/C++ sources are linked together via the clang++ driver.
            if cxx_sources:
                compiler = "clang++"
                sources = c_sources + cxx_sources
            else:
                compiler = "clang"
                sources = c_sources

            cmd = [compiler, f"-{optimization}", *build_flags, *sources, "-o", "output_binary"]
            _, log = _run(cmd, cwd=job_dir, timeout=timeout)
            full_log += log

        if not output_path.is_file():
            raise BuildFailure("Build reported success but no output binary was produced", log=full_log)

        artifact_dest_dir = ARTIFACT_DIR / job_id
        artifact_dest_dir.mkdir(parents=True, exist_ok=True)
        artifact_dest = artifact_dest_dir / "output_binary"
        shutil.copy2(output_path, artifact_dest)
        os.chmod(artifact_dest, 0o644)
        (artifact_dest_dir / "build.log").write_text(full_log)

        return artifact_dest, full_log
    finally:
        shutil.rmtree(job_dir, ignore_errors=True)
