import argparse
import cmd
import shlex
import time
from pathlib import Path

from . import config as client_config
from .api import ApiClient
from .banner import BANNER

POLL_INTERVAL_SECONDS = 0.4
OPS_WATCH_INTERVAL = 2.0

_STATUS_COLOR = {
    "pending": "\x1b[33m",   # yellow
    "running": "\x1b[94m",   # blue
    "success": "\x1b[32m",   # green
    "failed":  "\x1b[31m",   # red
}
_RESET = "\x1b[0m"


def _colorize_status(status: str) -> str:
    color = _STATUS_COLOR.get(status, "")
    return f"{color}{status}{_RESET}" if color else status


class ArgParseExit(Exception):
    pass


class _Parser(argparse.ArgumentParser):
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
        self.active_target_id: str | None = None
        self.active_target_name: str | None = None

        saved = client_config.load()
        if saved:
            self.addr = saved.get("addr")
            self.token = saved.get("token")
            self.active_target_id = saved.get("target_id")

    @property
    def connected(self) -> bool:
        return self.addr is not None and self.token is not None

    def _require_connected(self) -> bool:
        if not self.connected:
            print("not connected — run `connect --addr <ip:port> --token <token>` first")
            return False
        return True

    def _client(self) -> ApiClient:
        return ApiClient(self.addr, self.token)

    def _update_prompt(self) -> None:
        if self.active_target_name:
            self.prompt = f"jocky [{self.active_target_name}] > "
        else:
            self.prompt = "jocky > "

    # ── connect ──────────────────────────────────────────────────────────────

    def do_connect(self, arg: str) -> None:
        "connect --addr <ip:port> --token <token>  — authenticate and link this shell to a jocky server"
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

    # ── target ───────────────────────────────────────────────────────────────

    def do_target(self, arg: str) -> None:
        """target <subcommand> [options]

Subcommands:
  add  --name <name> --host <host> [--os <os>] [--notes <text>]
  list
  select <id>
  info"""
        parts = shlex.split(arg)
        if not parts:
            print(self.do_target.__doc__)
            return
        sub = parts[0]
        rest = parts[1:]

        if sub == "add":
            self._target_add(rest)
        elif sub == "list":
            self._target_list()
        elif sub == "select":
            self._target_select(rest)
        elif sub == "info":
            self._target_info()
        else:
            print(f"unknown subcommand: {sub}")
            print(self.do_target.__doc__)

    def _target_add(self, argv: list[str]) -> None:
        if not self._require_connected():
            return
        parser = _Parser(prog="target add", add_help=False)
        parser.add_argument("--name", required=True)
        parser.add_argument("--host", required=True)
        parser.add_argument("--os", default=None)
        parser.add_argument("--notes", default=None)
        try:
            args = parser.parse_args(argv)
        except ArgParseExit:
            return

        try:
            t = self._client().add_target(args.name, args.host, args.os, args.notes)
        except Exception as exc:
            print(f"failed: {exc}")
            return

        print(f"target registered: [{t['id']}] {t['name']}  {t['host']}")
        print(f"  to select: target select {t['id']}")

    def _target_list(self) -> None:
        if not self._require_connected():
            return
        try:
            targets = self._client().list_targets()
        except Exception as exc:
            print(f"failed: {exc}")
            return

        if not targets:
            print("no targets registered")
            return

        print(f"\n{'ID':<34} {'NAME':<20} {'HOST':<20} {'OS':<12} NOTES")
        print("-" * 100)
        for t in targets:
            marker = " *" if t["id"] == self.active_target_id else ""
            print(
                f"{t['id']:<34} {t['name']:<20} {t['host']:<20} "
                f"{(t['os'] or '-'):<12} {t['notes'] or ''}{marker}"
            )
        print()

    def _target_select(self, argv: list[str]) -> None:
        if not self._require_connected():
            return
        if not argv:
            print("usage: target select <id>")
            return

        target_id = argv[0]
        try:
            t = self._client().get_target(target_id)
        except Exception as exc:
            print(f"failed: {exc}")
            return

        self.active_target_id = t["id"]
        self.active_target_name = t["name"]
        client_config.save_target(t["id"])
        self._update_prompt()
        print(f"active target: [{t['id']}] {t['name']}  {t['host']}")

    def _target_info(self) -> None:
        if self.active_target_id is None:
            print("no active target — use `target select <id>`")
            return
        if not self._require_connected():
            return
        try:
            t = self._client().get_target(self.active_target_id)
        except Exception as exc:
            print(f"failed: {exc}")
            return

        print(f"\nActive target")
        print(f"  ID    : {t['id']}")
        print(f"  Name  : {t['name']}")
        print(f"  Host  : {t['host']}")
        print(f"  OS    : {t['os'] or '-'}")
        print(f"  Notes : {t['notes'] or '-'}")
        print(f"  Since : {t['created_at']}\n")

    # ── build ────────────────────────────────────────────────────────────────

    def do_build(self, arg: str) -> None:
        "build --source <file.jky> --output <name.exe> [--passes <passes>] [--target-id <id>]  — compile a .jky payload"
        if not self._require_connected():
            return

        parser = _Parser(prog="build", add_help=False)
        parser.add_argument("--source", required=True)
        parser.add_argument("--output", required=True)
        parser.add_argument("--passes", default="fla,sub,api-hash")
        parser.add_argument("--target-id", default=None)
        try:
            args = parser.parse_args(shlex.split(arg))
        except ArgParseExit:
            return

        source_path = Path(args.source)
        if not source_path.is_file():
            print(f"file not found: {source_path}")
            return
        if not source_path.suffix.lower() == ".jky":
            print(f"error: only .jky source files are accepted (got {source_path.suffix})")
            return

        target_id = args.target_id or self.active_target_id
        if target_id:
            print(f"target: {self.active_target_name or target_id}")

        client = self._client()
        try:
            build_id = client.submit_build(source_path, args.output, args.passes, target_id)
        except Exception as exc:
            print(f"build submission failed: {exc}")
            return

        print(f"build {build_id} submitted, streaming logs...")
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
                print(f"  {line}")
            offset = data["next_offset"]

            status = data["status"]
            if status in ("success", "failed"):
                exit_code = data["exit_code"]
                color = _STATUS_COLOR.get(status, "")
                print(f"\n{color}{'BUILD SUCCEEDED' if status == 'success' else 'BUILD FAILED'} (exit {exit_code}){_RESET}\n")
                return

            time.sleep(POLL_INTERVAL_SECONDS)

    # ── sessions ─────────────────────────────────────────────────────────────

    def do_sessions(self, arg: str) -> None:
        "sessions [--target-id <id>]  — list past builds"
        self._print_sessions(arg)

    def do_history(self, arg: str) -> None:
        "history [--target-id <id>]  — alias for sessions"
        self._print_sessions(arg)

    def _print_sessions(self, arg: str) -> None:
        if not self._require_connected():
            return

        parser = _Parser(prog="sessions", add_help=False)
        parser.add_argument("--target-id", default=None)
        try:
            args = parser.parse_args(shlex.split(arg))
        except ArgParseExit:
            return

        target_id = args.target_id or self.active_target_id
        client = self._client()
        try:
            rows = client.sessions(target_id=target_id)
        except Exception as exc:
            print(f"failed to fetch sessions: {exc}")
            return

        if not rows:
            print("no builds yet")
            return

        print(f"\n{'ID':<34} {'TARGET':<16} {'FILE':<20} {'OUTPUT':<16} {'STATUS':<8} {'EXIT':<5} CREATED")
        print("-" * 120)
        for row in rows:
            exit_code = row["exit_code"]
            target = row.get("target_name") or "-"
            status = _colorize_status(row["status"])
            print(
                f"{row['id']:<34} {target:<16} {row['filename']:<20} {row['output_name']:<16} "
                f"{status:<8} {'' if exit_code is None else exit_code:<5} {row['created_at']}"
            )
        print()

    # ── ops ──────────────────────────────────────────────────────────────────

    def do_ops(self, arg: str) -> None:
        "ops [--status running|pending|success|failed] [--watch] [--interval N]  — live multi-target attack monitor"
        if not self._require_connected():
            return

        parser = _Parser(prog="ops", add_help=False)
        parser.add_argument("--status", default=None)
        parser.add_argument("--watch", action="store_true")
        parser.add_argument("--interval", type=float, default=OPS_WATCH_INTERVAL)
        try:
            args = parser.parse_args(shlex.split(arg))
        except ArgParseExit:
            return

        client = self._client()

        def fetch_and_print() -> bool:
            try:
                entries = client.get_ops(status=args.status)
            except Exception as exc:
                print(f"ops failed: {exc}")
                return False

            if not entries:
                print("no builds found")
                return True

            running = sum(1 for e in entries if e["status"] in ("running", "pending"))
            success = sum(1 for e in entries if e["status"] == "success")
            failed  = sum(1 for e in entries if e["status"] == "failed")
            print(f"  {len(entries)} builds  |  {_colorize_status('running')} {running}  "
                  f"{_colorize_status('success')} {success}  {_colorize_status('failed')} {failed}\n")

            print(f"  {'BUILD ID':<18} {'TARGET':<16} {'HOST':<18} {'FILE':<20} {'STATUS':<8} {'EXIT':<5} LAST LOG")
            print("  " + "-" * 110)
            for e in entries:
                short_id = e["build_id"][:16]
                target   = (e.get("target_name") or "-")[:15]
                host     = (e.get("target_host") or "-")[:17]
                fname    = e["filename"][:19]
                status   = _colorize_status(e["status"])
                exit_c   = "" if e["exit_code"] is None else e["exit_code"]
                last     = (e.get("last_line") or "")[:45]
                print(f"  {short_id:<18} {target:<16} {host:<18} {fname:<20} {status:<8} {exit_c:<5} {last}")
            print()
            return True

        if args.watch:
            print("watching ops  (Ctrl+C to stop)...")
            try:
                while True:
                    print("\033[2J\033[H", end="")
                    print(f"jocky ops  [every {args.interval}s]  Ctrl+C to stop\n")
                    fetch_and_print()
                    time.sleep(args.interval)
            except KeyboardInterrupt:
                print("\nstopped")
        else:
            fetch_and_print()

    # ── exit ─────────────────────────────────────────────────────────────────

    def do_exit(self, arg: str) -> bool:
        "exit  — quit the shell"
        print("bye")
        return True

    def do_quit(self, arg: str) -> bool:
        "quit  — quit the shell"
        return self.do_exit(arg)

    def emptyline(self) -> None:
        pass
