"""Fixed creator job commands; invoked only by the trusted local job supervisor."""

import ctypes
import errno
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time


def job_filter():
    # No host-facing sockets, including IPC on read-only mounts. Do not
    # grant access to inherited kernel keyrings.
    sec = ctypes.CDLL('libseccomp.so.2')
    sec.seccomp_init.argtypes = [ctypes.c_uint32]
    sec.seccomp_init.restype = ctypes.c_void_p
    sec.seccomp_rule_add.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_int, ctypes.c_uint]
    sec.seccomp_syscall_resolve_name.argtypes = [ctypes.c_char_p]
    sec.seccomp_export_bpf.argtypes = [ctypes.c_void_p, ctypes.c_int]
    sec.seccomp_release.argtypes = [ctypes.c_void_p]
    sec.seccomp_release.restype = None
    context = sec.seccomp_init(0x7fff0000)  # SCMP_ACT_ALLOW
    if not context:
        raise ValueError('seccomp unavailable')
    fd = os.memfd_create('ngen-job-filter', flags=0)
    try:
        for name in (b'socket', b'io_uring_setup', b'keyctl', b'request_key', b'add_key'):
            syscall = sec.seccomp_syscall_resolve_name(name)
            if syscall < 0 or sec.seccomp_rule_add(context, 0x00050000 | errno.EPERM, syscall, 0):
                raise ValueError('required syscall filter unavailable')
        if sec.seccomp_export_bpf(context, fd):
            raise ValueError('could not export job filter')
        os.lseek(fd, 0, os.SEEK_SET)
        return fd
    except (OSError, ValueError):
        os.close(fd)
        raise
    finally:
        sec.seccomp_release(context)


def enter():
    # This branch runs before mounting or executing any project input. Verify
    # the scope's effective limits rather than trusting systemd's CLI success.
    rows = Path('/proc/self/cgroup').read_text().splitlines()
    unit = sys.argv[2]
    current, = (row[3:] for row in rows if row.startswith('0::'))
    if not re.fullmatch(r'ngen-job-[0-9a-f]{32}\.scope', unit) or not current.endswith('/' + unit):
        return 125
    group = Path('/sys/fs/cgroup' + current)
    for name, expected in {'pids.max': '32', 'memory.max': '805306368',
                           'memory.swap.max': '0', 'cpu.max': '100000 100000'}.items():
        if (group / name).read_text().strip() != expected:
            return 125
    pattern = Path('/proc/sys/kernel/core_pattern').read_text().strip()
    if pattern.startswith('|') and pattern != '|/bin/false':
        return 125  # RLIMIT_CORE does not disable a piped crash collector.
    control = Path(sys.argv[3])
    fd = job_filter()
    staged = control / 'registering'
    staged.write_text(current, encoding='utf-8')
    staged.rename(control / 'ready')
    until = time.monotonic() + 5
    while not (control / 'go').exists():
        if time.monotonic() >= until:
            return 125
        time.sleep(0.02)
    os.execve('/usr/bin/bwrap', ['/usr/bin/bwrap', '--seccomp', str(fd), *sys.argv[4:]], {})


def operation(kind):
    if kind == 'package':
        # Use the same source-only packaging operation as the CLI. The
        # supervisor froze and validated these named, read-only inputs.
        sys.path.insert(0, '/app')
        import project
        source = Path('/source')
        files = {entry.name: entry.read_bytes() for entry in sorted(source.iterdir())}
        manifest = json.loads(files[project.MANIFEST])
        output = Path('/work/source.tar.gz')
        project.package(files, manifest, output)
        with output.open('rb') as stream:
            shutil.copyfileobj(stream, sys.stdout.buffer)
        return 0

    def run(arguments):
        return subprocess.run(arguments, stdin=subprocess.DEVNULL, check=False).returncode == 0

    if kind not in ('cook', 'build', 'test'):
        return 125
    if not run(['/usr/bin/python3', '-I', '/app/cook.py', '--assetc', '/app/assetc',
                '--ktx', '/app/ktx', '--probe', '/app/probe', '--source', '/source/marker.glb',
                '--output', '/work/cook']):
        return 10
    if kind == 'cook':
        return 0
    if not run(['/usr/bin/cmake', '-S', '/source', '-B', '/work/build', '-G', 'Ninja',
                '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_CXX_COMPILER=/usr/bin/clang++',
                '-DCMAKE_PREFIX_PATH=/sdk', '-DCMAKE_PROGRAM_PATH=/usr/bin',
                '-DNGEN_STARTER_SCENE=/work/cook/scene.ngas', '-DFETCHCONTENT_FULLY_DISCONNECTED=ON']):
        return 11
    if not run(['/usr/bin/cmake', '--build', '/work/build', '--parallel', '2']):
        return 12
    if kind == 'test' and not run(['/usr/bin/ctest', '--test-dir', '/work/build',
                                   '--no-tests=error', '--parallel', '1', '--timeout', '30']):
        return 13
    return 0


if __name__ == '__main__':
    if sys.argv[1] == 'enter':
        try:
            sys.exit(enter())
        except (OSError, ValueError):
            sys.exit(125)
    sys.exit(operation(sys.argv[1]))
