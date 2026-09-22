import re

# Mirrors api/models.py. The worker must never trust the API layer alone -
# a malicious or buggy caller that reaches Redis directly (or a future
# second API implementation) must not be able to smuggle shell metacharacters
# or path traversal into a subprocess argument list.

FILENAME_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_\-./]{0,127}\.(c|cc|cpp|cxx|h|hpp|hxx|txt)$")
FLAG_RE = re.compile(r"^-[A-Za-z][A-Za-z0-9=_.\-]{0,64}$")
CMAKE_DEFINE_RE = re.compile(r"^-D[A-Z_][A-Z0-9_]*=[A-Za-z0-9_./\- ]{0,128}$")

BLOCKED_FLAG_PREFIXES = (
    "-o", "-I/", "-L/", "-B/", "-isystem/", "-isysroot", "--sysroot",
    "-Xclang", "-Xlinker", "-Wl,", "-fplugin", "-include", "-fuse-ld", "-dumpdir",
)
DANGEROUS_SUBSTRINGS = (";", "|", "&", "$", "`", "\n", "\r", ">", "<", "..")


class UnsafeInputError(ValueError):
    pass


def safe_filename(name: str) -> str:
    if not isinstance(name, str) or not FILENAME_RE.match(name) or ".." in name or name.startswith("/"):
        raise UnsafeInputError(f"Unsafe source filename rejected: {name!r}")
    return name


def safe_flag(flag: str) -> str:
    if not isinstance(flag, str) or any(bad in flag for bad in DANGEROUS_SUBSTRINGS):
        raise UnsafeInputError(f"Unsafe build flag rejected: {flag!r}")
    if not FLAG_RE.match(flag) or flag.startswith(BLOCKED_FLAG_PREFIXES):
        raise UnsafeInputError(f"Build flag not permitted: {flag!r}")
    return flag


def safe_cmake_define(flag: str) -> str:
    if not isinstance(flag, str) or any(bad in flag for bad in DANGEROUS_SUBSTRINGS):
        raise UnsafeInputError(f"Unsafe CMake flag rejected: {flag!r}")
    if not CMAKE_DEFINE_RE.match(flag):
        raise UnsafeInputError(f"Only -D<NAME>=<value> CMake defines are permitted: {flag!r}")
    return flag
