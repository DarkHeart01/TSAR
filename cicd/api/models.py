import re
from typing import Dict, List, Optional

from pydantic import BaseModel, Field, field_validator

# --- Input validation / sanitization ---------------------------------------
# These patterns are the first line of defense against command injection on
# the build server. The worker re-validates independently (defense in depth)
# since it must never trust the API layer alone.

FILENAME_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_\-./]{0,127}\.(c|cc|cpp|cxx|h|hpp|hxx|txt)$")
FLAG_RE = re.compile(r"^-[A-Za-z][A-Za-z0-9=_.\-]{0,64}$")
CMAKE_DEFINE_RE = re.compile(r"^-D[A-Z_][A-Z0-9_]*=[A-Za-z0-9_./\- ]{0,128}$")

BLOCKED_FLAG_PREFIXES = (
    "-o", "-I/", "-L/", "-B/", "-isystem/", "-isysroot", "--sysroot",
    "-Xclang", "-Xlinker", "-Wl,", "-fplugin", "-include", "-fuse-ld", "-dumpdir",
)
DANGEROUS_SUBSTRINGS = (";", "|", "&", "$", "`", "\n", "\r", ">", "<", "..")


def validate_filename(name: str) -> str:
    if not FILENAME_RE.match(name) or ".." in name or name.startswith("/"):
        raise ValueError(f"Invalid or unsafe source filename: {name!r}")
    return name


def validate_flag(flag: str) -> str:
    if any(bad in flag for bad in DANGEROUS_SUBSTRINGS):
        raise ValueError(f"Flag contains disallowed characters: {flag!r}")
    if not FLAG_RE.match(flag) or flag.startswith(BLOCKED_FLAG_PREFIXES):
        raise ValueError(f"Flag is not permitted: {flag!r}")
    return flag


def validate_cmake_define(flag: str) -> str:
    if any(bad in flag for bad in DANGEROUS_SUBSTRINGS):
        raise ValueError(f"CMake flag contains disallowed characters: {flag!r}")
    if not CMAKE_DEFINE_RE.match(flag):
        raise ValueError(f"Only -D<NAME>=<value> CMake defines are permitted: {flag!r}")
    return flag


ALLOWED_OPT_LEVELS = {"O0", "O1", "O2", "O3", "Os", "Oz"}


class BuildRequest(BaseModel):
    source_files: Dict[str, str] = Field(..., description="filename -> file content")
    build_flags: List[str] = Field(default_factory=list)
    cmake_flags: List[str] = Field(default_factory=list, description="-D defines only")
    optimization: str = Field(default="O2")
    timeout_seconds: int = Field(default=120, ge=10, le=600)

    @field_validator("source_files")
    @classmethod
    def check_source_files(cls, v: Dict[str, str]) -> Dict[str, str]:
        if not v:
            raise ValueError("At least one source file is required")
        if len(v) > 50:
            raise ValueError("Too many source files (max 50)")
        cleaned = {}
        for name, content in v.items():
            validate_filename(name)
            if len(content) > 1_000_000:
                raise ValueError(f"Source file too large: {name!r}")
            cleaned[name] = content
        if not any(name.endswith((".c", ".cc", ".cpp", ".cxx")) for name in cleaned):
            raise ValueError("At least one compilable source file (.c/.cpp/.cc/.cxx) is required")
        return cleaned

    @field_validator("build_flags")
    @classmethod
    def check_build_flags(cls, v: List[str]) -> List[str]:
        if len(v) > 40:
            raise ValueError("Too many build flags (max 40)")
        return [validate_flag(f) for f in v]

    @field_validator("cmake_flags")
    @classmethod
    def check_cmake_flags(cls, v: List[str]) -> List[str]:
        if len(v) > 40:
            raise ValueError("Too many CMake flags (max 40)")
        return [validate_cmake_define(f) for f in v]

    @field_validator("optimization")
    @classmethod
    def check_optimization(cls, v: str) -> str:
        if v not in ALLOWED_OPT_LEVELS:
            raise ValueError(f"optimization must be one of {sorted(ALLOWED_OPT_LEVELS)}")
        return v


class BuildStatusResponse(BaseModel):
    job_id: str
    status: str
    created_at: Optional[str] = None
    started_at: Optional[str] = None
    finished_at: Optional[str] = None
    error: Optional[str] = None
    log_tail: Optional[str] = None
