"""Owned, Linux-only headless board playtest. Project files are data, not code."""

from contextlib import contextmanager, suppress
import fcntl
import hashlib
import json
import os
from pathlib import Path
import select
import stat
import subprocess
import sys
import tempfile
import time

import creator_scene


BWRAP = Path("/usr/bin/bwrap")
PYTHON = "/usr/bin/python3"
COOKER = Path(__file__).resolve().with_name("assetc") / "cook.py"
LINE_LIMIT = 1024
COOK_TIMEOUT = 60
WORKER_TIMEOUT = 10
IDLE_TIMEOUT = 300
COMMAND_LIMIT = 128
PACKAGE_LIMIT = 256 * 1024**2
EOF = object()


class Lines:
    """Read bounded, newline-terminated messages, without a buffered-stream race."""

    def __init__(self, stream):
        self.fd = stream.fileno()
        self.pending = bytearray()

    def read(self, timeout):
        limit = time.monotonic() + timeout
        while b"\n" not in self.pending:
            if len(self.pending) >= LINE_LIMIT:
                raise ValueError("play message exceeds limit")
            ready, _, _ = select.select([self.fd], [], [], max(0, limit - time.monotonic()))
            if not ready:
                raise ValueError("play message timed out")
            data = os.read(self.fd, LINE_LIMIT - len(self.pending))
            if not data:
                if self.pending:
                    raise ValueError("incomplete play message")
                return EOF
            self.pending.extend(data)
        line, _, rest = self.pending.partition(b"\n")
        self.pending = rest
        return creator_scene.decode(line)


def executable(path):
    if not path.is_absolute() or not stat.S_ISREG(path.lstat().st_mode) or not os.access(path, os.X_OK):
        raise ValueError("play requires absolute regular executable paths")
    return path


def sandbox(work, bindings, args, writable=False):
    return [BWRAP, "--unshare-all", "--unshare-user", "--unshare-net", "--disable-userns",
            "--cap-drop", "ALL", "--die-with-parent", "--new-session", "--clearenv",
            "--setenv", "PATH", "/usr/bin", "--setenv", "LANG", "C.UTF-8",
            "--ro-bind", "/usr", "/usr", "--symlink", "usr/lib", "/lib",
            "--symlink", "usr/lib64", "/lib64", "--dev", "/dev", "--proc", "/proc",
            "--tmpfs", "/tmp", "--bind" if writable else "--ro-bind", work, "/work",
            *[part for target, source in bindings.items() for part in ("--ro-bind", source, target)],
            "--chdir", "/work", "--", *args]


def snapshot(directory, validate_project, revision):
    if type(revision) is not int or not 1 <= revision <= creator_scene.MAX_REVISION:
        raise ValueError("expected a saved scene revision")
    try:
        fcntl.flock(directory, fcntl.LOCK_SH | fcntl.LOCK_NB)
    except BlockingIOError as error:
        raise ValueError("scene is busy") from error
    try:
        validate_project(directory)
        saved = creator_scene.load(directory)
        if saved["revision"] != revision:
            raise ValueError("stale scene revision")
        entities = saved["entities"]
        if len(entities) != 1:
            raise ValueError("board play requires exactly one asset entity")
        entity = next(iter(entities.values()))
        transform = entity["transform"]
        if (entity.keys() != {"transform", "asset"} or transform["position_m"][1:] != [0, 0]
                or transform["rotation_xyzw"] != [0, 0, 0, 1] or transform["scale"] != [1, 1, 1]):
            raise ValueError("board play requires an X-only translation and unit orientation/scale")
        data = creator_scene.assets(directory, entities)[entity["asset"]]
        return data, int(transform["position_m"][0] * 1000)
    finally:
        fcntl.flock(directory, fcntl.LOCK_UN)


@contextmanager
def worker(runtime, player):
    arguments = sandbox(runtime, {"/app/player": player}, ["/app/player"])
    process = subprocess.Popen(arguments, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                               stderr=subprocess.DEVNULL, env={})
    reply = Lines(process.stdout)

    def request(command):
        try:
            process.stdin.write(command.encode("ascii") + b"\n")
            process.stdin.flush()
            row = reply.read(WORKER_TIMEOUT)
        except (OSError, ValueError) as error:
            raise RuntimeError("owned player stopped responding") from error
        if not isinstance(row, dict) or type(row.get("ok")) is not bool:
            raise RuntimeError("owned player returned no valid reply")
        return row

    try:
        yield request
    finally:
        with suppress(OSError):
            process.stdin.close()
        try:
            process.wait(timeout=WORKER_TIMEOUT)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        process.stdout.close()


def cook(tools, folder, source, destination):
    with tempfile.TemporaryDirectory(prefix="cook-", dir=folder) as job:
        job = Path(job)
        (job / "source.glb").write_bytes(source)
        arguments = sandbox(job, tools | {"/app/cook.py": COOKER},
                            [PYTHON, "/app/cook.py", "--assetc", "/app/assetc", "--ktx", "/app/ktx",
                             "--probe", "/app/probe", "--source", "/work/source.glb",
                             "--output", "/work/output"], writable=True)
        try:
            result = subprocess.run(arguments, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                    stderr=subprocess.DEVNULL, env={}, timeout=COOK_TIMEOUT, check=False)
        except subprocess.TimeoutExpired as error:
            raise ValueError("isolated cook timed out") from error
        if result.returncode:
            raise ValueError("isolated cook failed")
        package = job / "output/scene.ngas"
        if (package.resolve() != package or not package.is_file()
                or not 0 < package.stat().st_size <= PACKAGE_LIMIT):
            raise ValueError("isolated cook returned an invalid output")
        os.link(package, destination)


def emit(row):
    print(json.dumps(row), flush=True)


def run(directory, validate_project, args):
    executable(BWRAP)
    player = executable(args.player)
    tools = {f"/app/{name}": executable(getattr(args, name)) for name in ("assetc", "ktx", "probe")}
    commands = Lines(sys.stdin.buffer)
    with tempfile.TemporaryDirectory(prefix="ngen-board-play-") as path:
        folder = Path(path).resolve()
        runtime = folder / "runtime"
        runtime.mkdir()
        active, identity, payload, cooks = None, None, None, 0
        with worker(runtime, player) as request:
            def reload(revision, generation):
                nonlocal active, identity, payload, cooks
                data, anchor = snapshot(directory, validate_project, revision)
                digest = hashlib.sha256(data).digest()
                candidate = runtime / f"r{revision}.ngas"
                accepted = False
                try:
                    if digest == identity:
                        os.link(payload, candidate)
                    else:
                        cooks += 1
                        cook(tools, folder, data, candidate)
                    row = request(f"load {generation} {revision} {anchor}")
                    if not row["ok"]:
                        raise ValueError("player rejected the candidate")
                    if row.get("generation") != generation + 1 or row.get("revision") != revision:
                        raise RuntimeError("owned player returned inconsistent generation")
                    accepted = True
                    retired = payload
                    active, identity, payload = row, digest, candidate
                    if retired is not None:
                        try:
                            retired.unlink()
                        except OSError as error:
                            raise RuntimeError("could not retire the old play package") from error
                    return {**row, "asset_cooks": cooks}
                finally:
                    if not accepted:
                        candidate.unlink(missing_ok=True)

            emit(reload(args.expect, 0))
            for _ in range(COMMAND_LIMIT):
                command = commands.read(IDLE_TIMEOUT)
                if command is EOF or command == {"op": "stop"}:
                    emit(request("stop"))
                    return
                try:
                    if command == {"op": "inspect"}:
                        emit({**request("inspect"), "asset_cooks": cooks})
                    elif (isinstance(command, dict) and command.keys() == {"op", "expect", "revision"}
                          and command["op"] == "reload"):
                        if type(command["expect"]) is not int or command["expect"] != active["generation"]:
                            raise ValueError("stale play generation")
                        if type(command["revision"]) is not int or command["revision"] <= active["revision"]:
                            raise ValueError("reload requires a newer saved revision")
                        emit(reload(command["revision"], command["expect"]))
                    else:
                        raise ValueError("invalid play command")
                except (OSError, ValueError) as error:
                    emit({"ok": False, "error": str(error), "asset_cooks": cooks})
            raise ValueError("play command limit reached")
