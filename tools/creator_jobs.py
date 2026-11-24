"""One bounded, cancellable offline job for a launcher-granted starter."""

from contextlib import suppress
from dataclasses import dataclass, field
import fcntl
import hashlib
import os
from pathlib import Path
import re
import secrets
import shutil
import stat
import subprocess
import tempfile
import threading
import time
from typing import Literal, TypedDict

import creator_scene
import project

JobKind = Literal['cook', 'build', 'test', 'package']
ROOT = Path(__file__).resolve().parent
WORKER = ROOT / 'creator_job_worker.py'
KINDS = {'cook', 'build', 'test', 'package'}
ARTIFACT_LIMIT = 32 * 1024**2
SESSION_LIMIT = 32
FAILURES = {10: 'cook_failed', 11: 'configure_failed', 12: 'build_failed', 13: 'test_failed',
            125: 'isolation_unavailable'}
REQUIRED = ('bwrap', 'systemd-run', 'systemctl', 'prlimit', 'python3', 'cmake', 'ctest', 'ninja', 'clang++')


class JobReply(TypedDict):
    job: str | None
    state: str
    revision: str | None
    artifact: str | None
    error: str | None


def denied(error: str) -> JobReply:
    return {'job': None, 'state': 'rejected', 'revision': None, 'artifact': None, 'error': error}


def snapshot(directory):
    with creator_scene.locked(directory, fcntl.LOCK_SH) as fd:
        files = project.project_files(fd)
        project.validate(files)
        files.update(creator_scene.authored_files(fd))
    identity = hashlib.sha256()
    for name, data in sorted(files.items()):
        identity.update(name.encode('ascii') + b'\x00' + hashlib.sha256(data).digest())
    return identity.hexdigest(), files


def trusted_directory(path):
    if not path.is_absolute() or path.resolve(strict=True) != path or not path.is_dir():
        raise ValueError('jobs require absolute, canonical launcher paths')
    return path


def no_nested_mounts(*roots):
    for row in Path('/proc/self/mountinfo').read_text().splitlines():
        field = row.split(' ', 5)[4]
        path = Path(re.sub(r'\\([0-7]{3})', lambda match: chr(int(match[1], 8)), field))
        if any(path != root and path.is_relative_to(root) for root in roots):
            raise ValueError('job mount contains an unexpected submount')


@dataclass
class Job:
    id: str
    kind: JobKind
    revision: str
    state: str = 'running'
    artifact: str | None = None
    error: str | None = None
    cancel: threading.Event = field(default_factory=threading.Event)

    def reply(self) -> JobReply:
        return {'job': self.id, 'state': self.state, 'revision': self.revision,
                'artifact': self.artifact, 'error': self.error}


class CleanupFailed(RuntimeError):
    pass


class Runner:
    def __init__(self, *, sdk, tools, output, seconds):
        if not 1 <= seconds <= 90 or os.getuid() == 0 or os.uname().machine != 'x86_64':
            raise ValueError('jobs require a non-root user and a deadline of 1–90 seconds')
        self.sdk = trusted_directory(sdk)
        if not (self.sdk / 'lib/cmake/NGEN/NGENConfig.cmake').is_file():
            raise ValueError('expected a dedicated installed SDK prefix')
        tools = trusted_directory(tools)
        self.tools = {name: tools / name for name in ('assetc', 'ktx', 'probe')}
        no_nested_mounts(self.sdk, Path('/usr'))
        for path in self.tools.values():
            if not stat.S_ISREG(path.lstat().st_mode):
                raise ValueError('job tools must be regular, trusted files')
        for executable in (*self.tools.values(), *(Path('/usr/bin') / name for name in REQUIRED)):
            if not executable.is_file() or not os.access(executable, os.X_OK):
                raise ValueError('required local job tool unavailable')
        runtime = f'/run/user/{os.getuid()}'
        if not Path(runtime, 'bus').is_socket():
            raise ValueError('jobs require a running systemd user manager')
        self.env = {'XDG_RUNTIME_DIR': runtime, 'DBUS_SESSION_BUS_ADDRESS': 'unix:path=' + runtime + '/bus'}
        self.seconds = seconds
        self.lock = threading.Lock()
        self.current = None
        self.thread = None
        self.disabled = False
        self.accepted = 0
        self.output = os.open(trusted_directory(output), os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
        try:
            info = os.fstat(self.output)
            if info.st_uid != os.getuid() or stat.S_IMODE(info.st_mode) != 0o700:
                raise ValueError('job output must be a private owner directory (0700)')
        except (OSError, ValueError):
            os.close(self.output)
            raise

    def close(self):
        with self.lock:
            self.disabled = True
            if self.current:
                self.current.cancel.set()
        if self.thread:
            self.thread.join()
        os.close(self.output)

    def start(self, directory, kind: JobKind, expected: str) -> JobReply:
        if kind not in KINDS or not re.fullmatch(r'[0-9a-f]{64}', expected):
            return denied('invalid_job')
        with self.lock:
            if self.disabled:
                return denied('isolation_unavailable')
            if self.current and self.current.state == 'running':
                return denied('job_busy')
            if self.accepted == SESSION_LIMIT:
                return denied('job_limit')
            try:
                revision, files = snapshot(directory)
            except creator_scene.Busy:
                return denied('project_busy')
            except (OSError, ValueError):
                return denied('invalid_project')
            if revision != expected:
                return denied('stale_revision')
            job = Job(secrets.token_hex(16), kind, revision)
            self.thread = threading.Thread(target=self._run, args=(job, files))
            self.current = job
            try:
                self.thread.start()
            except RuntimeError:
                self.thread = self.current = None
                return denied('isolation_unavailable')
            self.accepted += 1
            return job.reply()

    def status(self, identity, *, cancel=False) -> JobReply:
        with self.lock:
            if not self.current or self.current.id != identity:
                return denied('unknown_job')
            if cancel and self.current.state == 'running':
                self.current.cancel.set()
            return self.current.reply()

    def _command(self, job, source, control, unit):
        bindings = {'/app/worker.py': WORKER, '/app/project.py': ROOT / 'project.py',
                    '/app/creator_scene.py': ROOT / 'creator_scene.py', '/app/cook.py': ROOT / 'assetc/cook.py',
                    **{f'/app/{name}': value for name, value in self.tools.items()}}
        return ['/usr/bin/systemd-run', '--user', '--scope', '--quiet', f'--unit={unit}',
                '-p', 'TasksMax=32', '-p', 'MemoryMax=768M', '-p', 'MemorySwapMax=0', '-p', 'CPUQuota=100%',
                '-p', 'CPUQuotaPeriodSec=100ms', '-p', 'RuntimeMaxSec=100s', '-p', 'TimeoutStopSec=4s',
                '--', '/usr/bin/prlimit', f'--fsize={ARTIFACT_LIMIT}:{ARTIFACT_LIMIT}', '--core=0:0',
                '--', '/usr/bin/python3', '-I', WORKER, 'enter', unit, control,
                '--unshare-all', '--unshare-user', '--unshare-net', '--disable-userns', '--cap-drop', 'ALL',
                '--die-with-parent', '--new-session', '--clearenv', '--setenv', 'PATH', '/usr/bin',
                '--setenv', 'HOME', '/work', '--setenv', 'LANG', 'C.UTF-8', '--setenv', 'PYTHONDONTWRITEBYTECODE', '1',
                '--ro-bind', '/usr', '/usr', '--symlink', 'usr/bin', '/bin',
                '--symlink', 'usr/lib', '/lib', '--symlink', 'usr/lib64', '/lib64',
                '--dev', '/dev', '--proc', '/proc', '--size', '268435456', '--tmpfs', '/work',
                '--size', '67108864', '--tmpfs', '/tmp', '--ro-bind', source, '/source',
                '--ro-bind', self.sdk, '/sdk',
                *[arg for dest, path in bindings.items() for arg in ('--ro-bind', path, dest)],
                '--chdir', '/work', '--', '/usr/bin/python3', '-I', '/app/worker.py', job.kind]

    def _stop(self, unit, events):
        try:
            stop = subprocess.Popen(['/usr/bin/systemctl', '--user', 'stop', unit], cwd='/', env=self.env,
                                    stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            try:
                stop.wait(timeout=6)
            except subprocess.TimeoutExpired:
                stop.kill()
                stop.wait(timeout=2)
        except (OSError, subprocess.TimeoutExpired):
            return False
        if events is None:
            return True  # No launch permit was issued; no project ran.
        try:
            return 'populated 0' in events.read_text(encoding='ascii').splitlines()
        except FileNotFoundError:
            return True  # A populated cgroup cannot have been removed.
        except OSError:
            return False

    def _stage(self, job, data):
        size = data.tell()
        if not 0 < size < ARTIFACT_LIMIT:
            raise ValueError('invalid source archive size')
        data.seek(0)
        staged = '.ngen-job-' + job.id
        fd = os.open(staged, os.O_CREAT | os.O_EXCL | os.O_WRONLY | os.O_NOFOLLOW, 0o600, dir_fd=self.output)
        try:
            with os.fdopen(fd, 'wb') as target:
                shutil.copyfileobj(data, target)
                target.flush()
                os.fsync(target.fileno())
            return staged
        except OSError:
            with suppress(OSError):
                os.unlink(staged, dir_fd=self.output)
            raise

    def _isolated(self, job, source, control, unit, artifact, deadline):
        events = None
        error = 'isolation_unavailable'
        process = subprocess.Popen(self._command(job, source, control, unit), cwd='/', env=self.env,
                                   stdin=subprocess.DEVNULL,
                                   stdout=artifact if job.kind == 'package' else subprocess.DEVNULL,
                                   stderr=subprocess.DEVNULL)
        try:
            while not job.cancel.is_set() and time.monotonic() < deadline:
                if events is None and (control / 'ready').is_file():
                    current = (control / 'ready').read_text(encoding='utf-8')
                    group = Path(current)
                    if not group.is_absolute() or '..' in group.parts or group.name != unit:
                        break
                    events = Path('/sys/fs/cgroup' + current) / 'cgroup.events'
                    if not events.is_file():
                        break
                    with self.lock:
                        if job.cancel.is_set() or time.monotonic() >= deadline:
                            break
                        (control / 'go').touch(exist_ok=False)
                try:
                    rc = process.wait(timeout=0.05)
                    if events is not None:
                        error = FAILURES.get(rc, 'job_failed') if rc else None
                    break
                except subprocess.TimeoutExpired:
                    pass
        finally:
            stopped = self._stop(unit, events)
            try:
                if process.poll() is None:
                    process.kill()
                process.wait(timeout=2)
            except (OSError, subprocess.TimeoutExpired) as failure:
                raise CleanupFailed from failure
            if not stopped:
                # Do not permit another project to start after failed cleanup.
                raise CleanupFailed
        return error

    def _run(self, job, files):
        deadline = time.monotonic() + self.seconds
        unit = f'ngen-job-{job.id}.scope'
        error = 'job_failed'
        staged = None
        try:
            with tempfile.TemporaryDirectory(prefix='ngen-job-') as work, tempfile.TemporaryFile() as artifact:
                source = Path(work) / 'source'
                source.mkdir(mode=0o700)
                for name, data in files.items():
                    (source / name).write_bytes(data)
                if not job.cancel.is_set() and time.monotonic() < deadline:
                    error = self._isolated(job, source, Path(work), unit, artifact, deadline)
                    if error is None and job.kind == 'package' and not job.cancel.is_set():
                        artifact.seek(0, os.SEEK_END)
                        staged = self._stage(job, artifact)
        except CleanupFailed:
            error = 'cleanup_failed'
        except (OSError, ValueError):
            error = 'job_failed'

        with self.lock:
            if error == 'cleanup_failed':
                self.disabled = True
            cancelled = error != 'cleanup_failed' and job.cancel.is_set()
            if error != 'cleanup_failed' and not cancelled and time.monotonic() >= deadline:
                error = 'deadline'
            if not cancelled and error is None and staged:
                name = job.id + '-source.tar.gz'
                try:
                    os.link(staged, name, src_dir_fd=self.output, dst_dir_fd=self.output, follow_symlinks=False)
                    job.artifact = name
                except OSError:
                    error = 'publish_failed'
            if staged:
                with suppress(OSError):
                    os.unlink(staged, dir_fd=self.output)
            job.error = None if cancelled else error
            job.state = 'cancelled' if cancelled else ('failed' if error else 'succeeded')
