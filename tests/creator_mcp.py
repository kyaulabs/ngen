"""Real stdio contract for the optional creator MCP companion."""

import asyncio
from contextlib import contextmanager
import fcntl
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from mcp import StdioServerParameters
from mcp.client import Client


ROOT = Path(__file__).resolve().parents[1]


class CreatorMcp(unittest.IsolatedAsyncioTestCase):
    async def test_discovery_and_granted_project_summary(self):
        with tempfile.TemporaryDirectory() as temp:
            board = Path(temp) / 'board'
            subprocess.run([sys.executable, ROOT / 'tools/project.py', 'new', board,
                            '--name', 'test-board'], check=True, capture_output=True)
            transport = StdioServerParameters(command=sys.executable, args=[
                str(ROOT / 'tools/creator_mcp.py'), '--project', str(board)])
            async with asyncio.timeout(10):
                async with Client(transport, mode='2026-07-28', read_timeout_seconds=3) as client:
                    resources = (await client.list_resources()).resources
                    self.assertEqual({str(r.uri) for r in resources}, {
                        'ngen://creator/project-schema', 'ngen://creator/sdk-guide'})
                    guide = await client.read_resource('ngen://creator/sdk-guide')
                    self.assertIn('find_package(NGEN', guide.contents[0].text)
                    schema = json.loads((await client.read_resource('ngen://creator/project-schema')).contents[0].text)
                    self.assertEqual(schema['properties']['schema']['const'], 1)
                    tools = {tool.name: tool for tool in (await client.list_tools()).tools}
                    self.assertEqual(set(tools), {'list_projects', 'inspect_project'})
                    self.assertEqual(tools['inspect_project'].input_schema['required'], ['handle'])
                    self.assertTrue(tools['inspect_project'].annotations.read_only_hint)
                    listed = (await client.call_tool('list_projects', {})).structured_content
                    handle, = listed['handles']
                    response = await client.call_tool('inspect_project', {'handle': handle})
                    self.assertEqual(response.structured_content, {
                        'ok': True, 'project': {'name': 'test-board', 'schema': 1},
                        'scene': {'revision': 0, 'entities': 0, 'assets': []}, 'error': None})

    async def test_scope_and_saved_scene(self):
        with tempfile.TemporaryDirectory() as temp:
            board = Path(temp) / 'board'
            cli = [sys.executable, ROOT / 'tools/project.py']
            subprocess.run([*cli, 'new', board, '--name', 'test-board'], check=True, capture_output=True)
            scene = board / 'ngen-scene.json'
            scene.write_text(json.dumps({'schema': 1, 'revision': 1, 'undo': [], 'redo': [], 'entities': {
                'marker': {'asset': 'marker.glb', 'transform': {
                    'position_m': [0, 0, 0], 'rotation_xyzw': [0, 0, 0, 1], 'scale': [1, 1, 1]}}}}))
            marker = board / 'marker.glb'
            transaction = StdioServerParameters(command=sys.executable, args=[
                str(ROOT / 'tools/creator_mcp.py'), '--project', str(board)])
            async with asyncio.timeout(10):
                async with Client(transaction, mode='2026-07-28', read_timeout_seconds=3) as client:
                    handle, = (await client.call_tool('list_projects', {})).structured_content['handles']

                    async def inspect(value):
                        return (await client.call_tool('inspect_project', {'handle': value})).structured_content

                    self.assertEqual((await inspect(handle))['scene'], {'revision': 1, 'entities': 1,
                        'assets': [{'name': 'marker.glb', 'bytes': marker.stat().st_size}]})
                    for denied in ('unknown', str(board), 'file:///outside', '../another-project'):
                        self.assertEqual(await inspect(denied), {
                            'ok': False, 'project': None, 'scene': None, 'error': 'unknown_project'})
                    with directory_lock(board):
                        self.assertEqual((await inspect(handle))['error'], 'project_busy')
                    outside = Path(temp) / 'outside.glb'
                    marker.rename(outside)
                    marker.symlink_to(outside)
                    self.assertEqual(await inspect(handle), {
                        'ok': False, 'project': None, 'scene': None, 'error': 'invalid_project'})


@contextmanager
def directory_lock(path):
    fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY)
    try:
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        yield
    finally:
        os.close(fd)


if __name__ == '__main__':
    unittest.main()
