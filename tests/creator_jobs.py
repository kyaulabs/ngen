"""Local stdio integration against a deliberately provisioned Linux job profile."""

import asyncio
from contextlib import asynccontextmanager
import json
import os
from pathlib import Path
import secrets
import struct
import subprocess
import sys
import tarfile
import tempfile
import unittest

from mcp import StdioServerParameters
from mcp.client import Client


ROOT = Path(__file__).resolve().parents[1]
PROFILE = os.environ.get("NGEN_JOBS_FIXTURE")


@unittest.skipUnless(PROFILE, "requires the isolated creator job SDK/tool profile")
class CreatorJobs(unittest.IsolatedAsyncioTestCase):
    async def finish(self, client, handle, job):
        while True:
            result = (await client.call_tool('job_status', {'handle': handle, 'job': job})).structured_content
            if result['state'] != 'running':
                return result
            await asyncio.sleep(.05)

    @asynccontextmanager
    async def sandbox(self, seconds=30):
        profile = Path(PROFILE)
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            board, output = root / 'board', root / 'output'
            output.mkdir(mode=0o700)
            subprocess.run([sys.executable, ROOT / 'tools/project.py', 'new', board,
                            '--name', 'jobs-board'], check=True, capture_output=True)
            args = [str(ROOT / 'tools/creator_mcp.py'), '--project', str(board), '--allow-jobs',
                    '--job-sdk', str(profile / 'sdk'), '--job-tools', str(profile / 'tools'),
                    '--job-output', str(output), '--job-seconds', str(seconds)]
            async with Client(StdioServerParameters(command=sys.executable, args=args,
                                                     env={'NGEN_HOST_TEST_SENTINEL': 'parent-only-fixture'}),
                              mode='2026-07-28', read_timeout_seconds=12) as client:
                handle, = (await client.call_tool('list_projects', {})).structured_content['handles']
                yield client, handle, board, output

    async def start(self, client, handle, kind):
        info = (await client.call_tool('job_revision', {'handle': handle})).structured_content
        result = (await client.call_tool('start_job', {'handle': handle, 'kind': kind,
                                                       'expected': info['revision']})).structured_content
        self.assertEqual(result['state'], 'running', result)
        return result['job']

    async def test_source_package_requires_the_grant_and_publishes_only_on_success(self):
        profile = Path(PROFILE)
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            board = root / "board"
            output = root / "output"
            output.mkdir(mode=0o700)
            subprocess.run([sys.executable, ROOT / "tools/project.py", "new", board,
                            "--name", "jobs-board"], check=True, capture_output=True)
            args = [str(ROOT / "tools/creator_mcp.py"), "--project", str(board)]
            async with asyncio.timeout(100):
                async with Client(StdioServerParameters(command=sys.executable, args=args),
                                  mode="2026-07-28", read_timeout_seconds=12) as client:
                    tools = {tool.name for tool in (await client.list_tools()).tools}
                    self.assertNotIn("start_job", tools)
                args += ["--allow-jobs", "--job-sdk", str(profile / "sdk"),
                         "--job-tools", str(profile / "tools"), "--job-output", str(output)]
                async with Client(StdioServerParameters(command=sys.executable, args=args),
                                  mode="2026-07-28", read_timeout_seconds=12) as client:
                    handle, = (await client.call_tool("list_projects", {})).structured_content["handles"]
                    response = await client.call_tool("job_revision", {"handle": handle})
                    self.assertFalse(response.is_error, response.content)
                    revision = response.structured_content["revision"]
                    denied = (await client.call_tool("start_job", {
                        "handle": handle, "kind": "package", "expected": "0" * 64})).structured_content
                    self.assertEqual(denied["error"], "stale_revision")
                    self.assertEqual((await client.call_tool('job_status', {
                        'handle': handle, 'job': '0' * 32})).structured_content['error'], 'unknown_job')
                    self.assertEqual((await client.call_tool('job_revision', {
                        'handle': '0' * 32})).structured_content['error'], 'unknown_project')
                    self.assertEqual(list(output.iterdir()), [])
                    accepted = (await client.call_tool("start_job", {
                        "handle": handle, "kind": "package", "expected": revision})).structured_content
                    self.assertEqual(accepted["state"], "running", accepted)
                    job = accepted["job"]
                    while True:
                        result = (await client.call_tool("job_status", {
                            "handle": handle, "job": job})).structured_content
                        if result["state"] != "running":
                            break
                        await asyncio.sleep(0.05)
                    self.assertEqual(result["state"], "succeeded", result)
                    name = result["artifact"]
                    self.assertEqual(name, job + "-source.tar.gz")
                    with tarfile.open(output / name, "r:gz") as archive:
                        self.assertEqual(set(archive.getnames()), {
                            "jobs-board/" + member for member in (
                                "ngen-project.json", "LICENSE", "CMakeLists.txt", "main.cpp", "marker.glb")})
                    self.assertEqual([item.name for item in output.iterdir()], [name])

    async def test_native_cook_build_and_test_are_isolated_and_report_failures(self):
        async with asyncio.timeout(130):
            async with self.sandbox() as (client, handle, board, output):
                for kind in ('cook', 'build', 'test'):
                    job = await self.start(client, handle, kind)
                    result = await self.finish(client, handle, job)
                    self.assertEqual(result['state'], 'succeeded', result)
                    self.assertIsNone(result['artifact'])

                outside = board.parent / 'host-only.txt'
                outside.write_text('keep this outside the sandbox')
                net = os.readlink('/proc/self/ns/net')
                source = board / 'main.cpp'
                source.write_text('#include <cstdlib>\n#include <fstream>\n#include <filesystem>\n'
                                  '#include <sys/socket.h>\n#include <sys/syscall.h>\n#include <unistd.h>\n#include <cerrno>\n'
                                  'int main() {\n'
                                  '  if (std::getenv("NGEN_HOST_TEST_SENTINEL")) return 1;\n'
                                  f'  if (std::ifstream({json.dumps(str(outside))})) return 2;\n'
                                  f'  if (std::filesystem::read_symlink("/proc/self/ns/net") == {json.dumps(net)}) return 3;\n'
                                  '  std::ofstream forbidden("/source/main.cpp", std::ios::app);\n'
                                  '  if (forbidden) return 4;\n'
                                  '  if (socket(AF_UNIX, SOCK_STREAM, 0) != -1 || errno != EPERM) return 5;\n'
                                  '  if (syscall(SYS_keyctl, -1, 0, 0, 0, 0) != -1 || errno != EPERM) return 6;\n'
                                  '  if (syscall(SYS_io_uring_setup, 0, nullptr) != -1 || errno != EPERM) return 7;\n'
                                  '  std::ofstream discard("/dev/stdout");\n'
                                  '  for (int i = 0; i < 65536; ++i) discard << "fixture output discarded\\n";\n'
                                  '  return 0;\n}\n')
                original = source.read_bytes()
                job = await self.start(client, handle, 'test')
                result = await self.finish(client, handle, job)
                self.assertEqual(result['state'], 'succeeded', result)
                self.assertEqual(source.read_bytes(), original)
                self.assertEqual(outside.read_text(), 'keep this outside the sandbox')
                source.write_text('#error fixture compiler failure\n')
                job = await self.start(client, handle, 'build')
                self.assertEqual((await self.finish(client, handle, job))['error'], 'build_failed')
                source.write_text('int main() { return 1; }\n')
                job = await self.start(client, handle, 'test')
                self.assertEqual((await self.finish(client, handle, job))['error'], 'test_failed')
                (board / 'marker.glb').write_bytes(struct.pack('<4sII8x', b'glTF', 2, 20))
                job = await self.start(client, handle, 'cook')
                self.assertEqual((await self.finish(client, handle, job))['error'], 'cook_failed')
                self.assertEqual(list(output.iterdir()), [])

    def test_unverified_scope_cannot_request_a_launch_permit(self):
        unit = f'ngen-job-{secrets.token_hex(16)}.scope'
        with tempfile.TemporaryDirectory() as control:
            result = subprocess.run(['/usr/bin/systemd-run', '--user', '--scope', '--quiet', f'--unit={unit}',
                                     '-p', 'TasksMax=16', '/usr/bin/python3', '-I',
                                     ROOT / 'tools/creator_job_worker.py', 'enter', unit, control],
                                    capture_output=True, timeout=8, check=False)
            self.assertEqual(result.returncode, 125, result.stderr)
            self.assertEqual(list(Path(control).iterdir()), [])

    async def test_cancel_and_deadline_close_the_owned_scope(self):
        async with asyncio.timeout(90):
            for seconds in (1, 30):
                async with self.sandbox(seconds) as (client, handle, board, output):
                    (board / 'main.cpp').write_text('#include <unistd.h>\nint main() {\n'
                                                   '  if (fork() == -1) return 1;\n'
                                                   '  sleep(12);\n  return 0;\n}\n')
                    job = await self.start(client, handle, 'test')
                    cgroup = None
                    if seconds == 30:
                        info = (await client.call_tool('job_revision', {'handle': handle})).structured_content
                        denied = (await client.call_tool('start_job', {
                            'handle': handle, 'kind': 'package', 'expected': info['revision']})).structured_content
                        self.assertEqual(denied['error'], 'job_busy')
                        for _ in range(20):
                            running = subprocess.run(['/usr/bin/systemctl', '--user', 'show', '--value',
                                                      '--property=ActiveState', f'ngen-job-{job}.scope'],
                                                     capture_output=True, timeout=4, check=True)
                            if running.stdout.strip() == b'active':
                                break
                            await asyncio.sleep(0.1)
                        self.assertEqual(running.stdout.strip(), b'active')
                        group = subprocess.run(['/usr/bin/systemctl', '--user', 'show', '--value',
                                                '--property=ControlGroup', f'ngen-job-{job}.scope'],
                                               capture_output=True, timeout=4, check=True).stdout.decode().strip()
                        self.assertTrue(group.endswith(f'/ngen-job-{job}.scope'))
                        cgroup = Path('/sys/fs/cgroup' + group) / 'cgroup.events'
                        for _ in range(80):
                            starters = 0
                            for pid in cgroup.with_name('cgroup.procs').read_text().split():
                                try:
                                    starters += (Path('/proc', pid, 'comm').read_text().strip() == 'ngen_starter')
                                except (FileNotFoundError, ProcessLookupError):
                                    # The process may exit before open or between open and read.
                                    continue
                            if starters >= 2:
                                break
                            await asyncio.sleep(.1)
                        self.assertGreaterEqual(starters, 2, 'expected an owned test program and descendant')
                        result = (await client.call_tool('cancel_job', {'handle': handle, 'job': job})).structured_content
                        self.assertEqual(result['state'], 'running')
                    result = await self.finish(client, handle, job)
                    if seconds == 30:
                        self.assertEqual(result['state'], 'cancelled', result)
                    else:
                        self.assertEqual(result['error'], 'deadline', result)
                    state = subprocess.run(['/usr/bin/systemctl', '--user', 'show', '--value',
                                            '--property=ActiveState', f'ngen-job-{job}.scope'],
                                           capture_output=True, timeout=4, check=True)
                    self.assertIn(state.stdout.strip(), (b'inactive', b'failed'))
                    if cgroup is not None and cgroup.exists():
                        self.assertIn('populated 0', cgroup.read_text().splitlines())
                    self.assertEqual(list(output.iterdir()), [])
