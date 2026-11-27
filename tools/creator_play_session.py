"""Own one launcher-granted CLI board playtest for the local MCP companion."""

from argparse import Namespace
from contextlib import suppress
import json
import os
from pathlib import Path
import secrets
import subprocess
import sys
import tempfile
from threading import Lock
from typing import TypedDict

import creator_play
import creator_scene


TOOLS = ("assetc", "ktx", "probe", "player")
REQUEST_TIMEOUT = creator_play.COOK_TIMEOUT + creator_play.WORKER_TIMEOUT + 5
STOP_TIMEOUT = creator_play.WORKER_TIMEOUT + 2


class Snapshot(TypedDict):
    generation: int
    revision: int
    tick: int
    marker_mm: int
    instances: int
    asset_cooks: int


class Reply(TypedDict):
    ok: bool
    session: str | None
    snapshot: Snapshot | None
    stopped: bool
    error: str | None


def denied(error: str) -> Reply:
    return {"ok": False, "session": None, "snapshot": None, "stopped": False, "error": error}


def checked_snapshot(row, generation, revision) -> Snapshot:
    limits = {"generation": (generation, generation), "revision": (revision, revision),
              "tick": (1, 1), "marker_mm": (-8192000 + 42, 8192000 + 42),
              "instances": (1, creator_scene.MAX_REVISION), "asset_cooks": (1, creator_play.COMMAND_LIMIT + 1)}
    if not isinstance(row, dict) or row.get("ok") is not True or row.keys() != limits.keys() | {"ok"}:
        raise ValueError("invalid board reply")
    snapshot = {}
    for key, (minimum, maximum) in limits.items():
        value = row[key]
        if type(value) is not int or not minimum <= value <= maximum:
            raise ValueError("invalid board snapshot")
        snapshot[key] = value
    return snapshot


class Runner:
    def __init__(self, directory, tools):
        creator_play.executable(creator_play.BWRAP)
        root = Path(f"/proc/self/fd/{directory}/.").resolve(strict=True)
        self.tools = {name: creator_play.executable(tools[name]).resolve(strict=True) for name in TOOLS}
        if any(path.is_relative_to(root) for path in self.tools.values()):
            raise ValueError("play tools must be outside the project")
        self.directory = directory
        self.lock = Lock()
        self.process = self.stage = self.lines = self.session = self.snapshot = None

    def _retire(self):
        process = self.process
        try:
            if process is not None:
                with suppress(OSError):
                    process.stdin.close()  # EOF requests normal board stop and reaping.
                try:
                    process.wait(timeout=STOP_TIMEOUT)
                except subprocess.TimeoutExpired:
                    process.kill()  # Only the unreaped direct child owned by this runner.
                    process.wait(timeout=STOP_TIMEOUT)
                process.stdout.close()
            if self.stage is not None:
                self.stage.cleanup()
        except (OSError, subprocess.TimeoutExpired):
            # Keep ownership to retry retirement. Never admit a replacement worker.
            return "play_cleanup_failed"
        self.process = self.stage = self.lines = self.session = self.snapshot = None
        return "play_failed" if process is not None and process.returncode else None

    def _fail(self, error):
        retired = self._retire()
        return denied("play_cleanup_failed" if retired == "play_cleanup_failed" else error)

    def close(self):
        with self.lock:
            if self._retire() == "play_cleanup_failed":
                raise RuntimeError("could not clean up the owned board playtest")

    def _reply(self) -> Reply:
        return {"ok": True, "session": self.session, "snapshot": self.snapshot,
                "stopped": False, "error": None}

    def start(self, revision: int) -> Reply:
        if not self.lock.acquire(blocking=False):
            return denied("play_busy")
        try:
            if self.process is not None or self.stage is not None:
                if self.process is not None and self.process.poll() is None:
                    return denied("play_busy")
                if self._retire() == "play_cleanup_failed":
                    return denied("play_cleanup_failed")
            try:
                self.stage = tempfile.TemporaryDirectory(prefix="ngen-mcp-play-")
                args = [sys.executable, "-B", __file__, str(self.directory), str(revision),
                        *(str(self.tools[name]) for name in TOOLS)]
                self.process = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                                stderr=subprocess.DEVNULL, pass_fds=(self.directory,),
                                                cwd=self.stage.name, env={"PATH": "/usr/bin:/bin", "LANG": "C.UTF-8",
                                                                         "TMPDIR": self.stage.name})
                self.lines = creator_play.Lines(self.process.stdout)
                self.snapshot = checked_snapshot(self.lines.read(REQUEST_TIMEOUT), 1, revision)
            except (OSError, ValueError):
                return self._fail("play_start_failed")
            self.session = secrets.token_hex(16)
            return self._reply()
        finally:
            self.lock.release()

    def command(self, session, generation, op, revision=None) -> Reply:
        if not self.lock.acquire(blocking=False):
            return denied("play_busy")
        try:
            if self.session is None or session != self.session:
                return denied("unknown_session")
            if self.process.poll() is not None:
                return self._fail("unknown_session")
            if generation != self.snapshot["generation"]:
                return denied("stale_generation")
            command = {"op": op}
            expected_generation, expected_revision = generation, self.snapshot["revision"]
            if op == "reload":
                if revision <= expected_revision:
                    return denied("play_rejected")
                command.update(expect=generation, revision=revision)
                expected_generation, expected_revision = generation + 1, revision
            try:
                self.process.stdin.write(json.dumps(command).encode("ascii") + b"\n")
                self.process.stdin.flush()
                row = self.lines.read(REQUEST_TIMEOUT if op == "reload" else creator_play.WORKER_TIMEOUT + 1)
                if op == "stop":
                    if row != {"ok": True, "stopped": True}:
                        raise ValueError("invalid stop reply")
                    if error := self._retire():
                        return denied(error)
                    return {"ok": True, "session": session, "snapshot": None, "stopped": True, "error": None}
                if op == "reload" and isinstance(row, dict) and row.get("ok") is False:
                    return denied("play_rejected")
                self.snapshot = checked_snapshot(row, expected_generation, expected_revision)
            except (OSError, ValueError):
                return self._fail("play_failed")
            return self._reply()
        finally:
            self.lock.release()


def _run_child():
    import project

    inherited, revision, *paths = sys.argv[1:]
    profile = Namespace(expect=int(revision), **dict(zip(TOOLS, map(Path, paths), strict=True)))
    fd = int(inherited)
    try:
        # Lock an independent open description; killing this helper must not
        # leave the launcher's directory descriptor holding a shared flock.
        directory = os.open(".", os.O_RDONLY | os.O_DIRECTORY, dir_fd=fd)
    finally:
        os.close(fd)
    try:
        creator_play.run(directory, lambda fd: project.validate(project.project_files(fd)), profile)
    finally:
        os.close(directory)


if __name__ == "__main__":
    try:
        _run_child()
    except (OSError, RuntimeError, ValueError):
        sys.exit(1)
