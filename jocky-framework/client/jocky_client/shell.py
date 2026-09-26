import argparse
import cmd
import shlex
import time
from pathlib import Path

from . import config as client_config
from .api import ApiClient
from .banner import BANNER

POLL_INTERVAL_SECONDS = 0.4


class ArgParseExit(Exception):
    pass


class _Parser(argparse.ArgumentParser):
    """argparse exits the process on bad args by default; raise instead so the shell survives."""

    def error(self, message: str) -> None:
        print(f"error: {message}")
        raise ArgParseExit(message)


class JockyShell(cmd.Cmd):
    intro = BANNER
    prompt = "jocky > "

    def __init__(self) -> None:
        super().__init__()
        self.addr: str | None = None
        self.token: str | None = None

    @property
    def connected(self) -> bool:
        return self.addr is not None and self.token is not None

    def _require_connected(self) -> bool:
        if not self.connected:
            print("not connected - run `connect --addr <ip:port> --token <token>` first")
            return False
        return True

    def do_connect(self, arg: str) -> None:
        "connect --addr <ip:port> --token <token> : authenticate and link this shell to a jocky server"
        parser = _Parser(prog="connect", add_help=False)
        parser.add_argument("--addr", required=True)
        parser.add_argument("--token", required=True)
        try:
            args = parser.parse_args(shlex.split(arg))
        except ArgParseExit:
            return

        client = ApiClient(args.addr, args.token)
        try:
            result = client.connect()
        except Exception as exc:
            print(f"connect failed: {exc}")
            return

        self.addr = args.addr
        self.token = args.token
        client_config.save(args.addr, args.token)
        print(f"connected to {args.addr} as {result['client_name']}")

    def do_build(self, arg: str) -> None:
        "build --template <path> --output <name> --passes <passes> : upload a .cpp file and stream compile logs"
        if not self._require_connected():
            return

        parser = _Parser(prog="build", add_help=False)
        parser.add_argument("--template", required=True)
        parser.add_argument("--output", required=True)
        parser.add_argument("--passes", default="")
        try:
            args = parser.parse_args(shlex.split(arg))
        except ArgParseExit:
            return

        source_path = Path(args.template)
        if not source_path.is_file():
            print(f"template not found: {source_path}")
            return

        client = ApiClient(self.addr, self.token)
        try:
            build_id = client.submit_build(source_path, args.output, args.passes)
        except Exception as exc:
            print(f"build submission failed: {exc}")
            return

        print(f"build {build_id} submitted, polling for logs...")
        self._poll_build(client, build_id)

    def _poll_build(self, client: ApiClient, build_id: str) -> None:
        offset = 0
        while True:
            try:
                data = client.poll_build_logs(build_id, offset)
            except Exception as exc:
                print(f"log poll failed: {exc}")
                return

            for line in data["lines"]:
                print(f"[compiler] {line}")
            offset = data["next_offset"]

            status = data["status"]
            if status in ("success", "failed"):
                exit_code = data["exit_code"]
                if status == "success":
                    print(f"BUILD SUCCEEDED (exit {exit_code})")
                else:
                    print(f"BUILD FAILED (exit {exit_code})")
                return

            time.sleep(POLL_INTERVAL_SECONDS)

    def do_sessions(self, arg: str) -> None:
        "sessions : list past builds"
        self._print_sessions()

    def do_history(self, arg: str) -> None:
        "history : alias for sessions"
        self._print_sessions()

    def _print_sessions(self) -> None:
        if not self._require_connected():
            return
        client = ApiClient(self.addr, self.token)
        try:
            rows = client.sessions()
        except Exception as exc:
            print(f"failed to fetch sessions: {exc}")
            return

        if not rows:
            print("no builds yet")
            return

        print(f"{'ID':<34} {'FILE':<20} {'OUTPUT':<16} {'STATUS':<8} {'EXIT':<5} CREATED")
        for row in rows:
            exit_code = row["exit_code"]
            print(
                f"{row['id']:<34} {row['filename']:<20} {row['output_name']:<16} "
                f"{row['status']:<8} {'' if exit_code is None else exit_code:<5} {row['created_at']}"
            )

    def do_exit(self, arg: str) -> bool:
        "exit : quit the shell"
        print("bye")
        return True

    def do_quit(self, arg: str) -> bool:
        "quit : quit the shell"
        return self.do_exit(arg)

    def emptyline(self) -> None:
        pass
