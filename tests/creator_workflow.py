"""One non-space creator workflow over real MCP stdio on the local Linux profiles."""

import asyncio
import json
from pathlib import Path
import tarfile
import tempfile
import unittest

from mcp.client import Client

from tests.creator_jobs import PROFILE
from tests.creator_mcp import PLAY_BUILD, PLAY_KTX, ROOT, board_transport


@unittest.skipUnless(PROFILE and PLAY_BUILD and PLAY_KTX, "requires the native job and board profiles")
class CreatorWorkflow(unittest.IsolatedAsyncioTestCase):
    async def test_granted_board_creation_to_source_archive_and_cancelled_job(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            board, output, staging = (root / name for name in ('board', 'output', 'play-stage'))
            for directory in (board, output, staging):
                directory.mkdir(mode=0o700)
            profile = Path(PROFILE)
            sentinel = 'local-parent-only-sentinel'
            transport = board_transport(board, '--allow-create', '--allow-edits', '--allow-jobs',
                                        '--job-sdk', str(profile / 'sdk'), '--job-tools', str(profile / 'tools'),
                                        '--job-output', str(output), '--job-seconds', '45',
                                        env={'NGEN_CREATOR_WORKFLOW_SENTINEL': sentinel, 'TMPDIR': str(staging)})
            async with asyncio.timeout(160):
                async with Client(transport, mode='2026-07-28', read_timeout_seconds=25) as client:
                    async def call(tool, **arguments):
                        result = await client.call_tool(tool, arguments)
                        self.assertFalse(result.is_error, result.content)
                        self.assertNotIn(sentinel, repr(result))
                        self.assertNotIn(str(root), repr(result))
                        return result.structured_content

                    handle, = (await call('list_projects'))['handles']

                    async def request(tool, **arguments):
                        return await call(tool, request={'handle': handle, **arguments})

                    async def edit(operation, change):
                        revisions = (await call('edit_revisions', handle=handle))['revisions']
                        result = await request('edit_project', operation=operation, change=change,
                                               expected=revisions[operation], apply=True)
                        self.assertTrue(result['applied'], result)
                        return result['after']

                    async def job_revision():
                        return (await call('job_revision', handle=handle))['revision']

                    async def start_job(kind, expected):
                        started = await call('start_job', handle=handle, kind=kind, expected=expected)
                        self.assertEqual(started['state'], 'running', started)
                        return started['job']

                    async def finish_job(job, state):
                        while True:
                            status = await call('job_status', handle=handle, job=job)
                            if status['state'] != 'running':
                                self.assertEqual(status['state'], state, status)
                                return status
                            await asyncio.sleep(.05)

                    self.assertEqual(list(board.iterdir()), [])
                    created = await request('create_project', name='workflow-board')
                    self.assertEqual(created['project'], {'name': 'workflow-board', 'schema': 1})
                    baseline = await job_revision()
                    template = (ROOT / 'tools/project_template/main.cpp').read_text()
                    rules = '#include <cstdlib>\n' + template.replace('42', '64').replace(
                        '    if (argc != 2) {',
                        '    if (std::getenv("NGEN_CREATOR_WORKFLOW_SENTINEL"))\n'
                        '        return 1;\n    if (argc != 2) {')
                    await edit('replace_source', rules)
                    layout = json.loads((ROOT / 'samples/board/layout.json').read_bytes())
                    self.assertEqual(await edit('edit_scene', json.dumps(layout)), '1')
                    current = await job_revision()
                    self.assertNotEqual(current, baseline)
                    stale = await call('start_job', handle=handle, kind='test', expected=baseline)
                    self.assertEqual(stale['error'], 'stale_revision')
                    for kind in ('cook', 'build', 'test'):
                        status = await finish_job(await start_job(kind, current), 'succeeded')
                        self.assertIsNone(status['artifact'])

                    play = await request('start_playtest', revision=1)
                    self.assertTrue(play['ok'], play)
                    self.assertEqual(play['snapshot']['marker_mm'], 1042)
                    owned = {'session': play['session'], 'generation': 1}
                    layout['board']['transform']['position_m'][0] = 2
                    self.assertEqual(await edit('edit_scene', json.dumps(layout)), '2')
                    updated = await request('reload_playtest', revision=2, **owned)
                    self.assertEqual(updated['snapshot'], {**play['snapshot'], 'generation': 2,
                                                           'revision': 2, 'marker_mm': 2042})
                    self.assertEqual((await request('stop_playtest', **owned))['error'], 'stale_generation')
                    self.assertTrue((await request('stop_playtest', **(owned | {'generation': 2})))['stopped'])
                    self.assertEqual(list(staging.iterdir()), [])

                    packaged = await finish_job(await start_job('package', await job_revision()), 'succeeded')
                    name = packaged['artifact']
                    self.assertEqual(name, packaged['job'] + '-source.tar.gz')
                    archive = output / name
                    before = archive.read_bytes()
                    with tarfile.open(archive) as source:
                        prefix = 'workflow-board/'
                        members = source.getmembers()
                        self.assertTrue(all(member.isfile() for member in members))
                        self.assertEqual({member.name for member in members}, {
                            prefix + file for file in ('ngen-project.json', 'LICENSE', 'CMakeLists.txt',
                                                       'main.cpp', 'marker.glb', 'ngen-scene.json')})
                        self.assertEqual(source.extractfile(prefix + 'main.cpp').read(), rules.encode())
                        self.assertEqual(source.extractfile(prefix + 'LICENSE').read(), (ROOT / 'LICENSE').read_bytes())
                        scene = json.load(source.extractfile(prefix + 'ngen-scene.json'))
                        self.assertEqual((scene['revision'], scene['entities']), (2, layout))

                    await edit('replace_source', '#include <unistd.h>\nint main() { sleep(12); return 0; }\n')
                    current = await job_revision()
                    running = await start_job('test', current)
                    busy = await asyncio.gather(*(call('start_job', handle=handle, kind='package', expected=current)
                                                  for _ in range(16)))
                    self.assertEqual({reply['error'] for reply in busy}, {'job_busy'})
                    self.assertEqual((await call('cancel_job', handle=handle, job=running))['state'], 'running')
                    cancelled = await finish_job(running, 'cancelled')
                    self.assertIsNone(cancelled['artifact'])
                    self.assertEqual(list(output.iterdir()), [archive])
                    self.assertEqual(archive.read_bytes(), before)
                self.assertEqual(list(staging.iterdir()), [])
