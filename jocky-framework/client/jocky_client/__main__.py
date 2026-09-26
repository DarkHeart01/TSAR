import sys

# Absolute import (not relative): PyInstaller's frozen bootloader executes this file
# as a standalone "__main__" module with no parent package, so `from .shell import ...`
# fails at runtime even though it works fine under `python -m jocky_client`.
from jocky_client.shell import JockyShell


def _enable_windows_ansi() -> None:
    """Legacy cmd.exe doesn't render ANSI color codes unless this mode is set;
    without it the banner's color escapes show up as literal garbage characters."""
    if sys.platform != "win32":
        return
    import ctypes

    STD_OUTPUT_HANDLE = -11
    ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x0004
    kernel32 = ctypes.windll.kernel32
    handle = kernel32.GetStdHandle(STD_OUTPUT_HANDLE)
    mode = ctypes.c_uint32()
    if kernel32.GetConsoleMode(handle, ctypes.byref(mode)):
        kernel32.SetConsoleMode(handle, mode.value | ENABLE_VIRTUAL_TERMINAL_PROCESSING)


def main() -> None:
    # Windows consoles often default to a legacy codepage (e.g. cp1252) that can't
    # render the banner's block characters; force UTF-8 with a safe fallback instead
    # of letting cmd.Cmd crash on startup.
    if sys.stdout.encoding and sys.stdout.encoding.lower() != "utf-8":
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")

    _enable_windows_ansi()

    JockyShell().cmdloop()


if __name__ == "__main__":
    main()
