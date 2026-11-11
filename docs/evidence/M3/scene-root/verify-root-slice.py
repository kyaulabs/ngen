from pathlib import Path
import concurrent.futures
import json
import os
import subprocess

root = Path.cwd()
out = root / 'build/m3-flight'
configs = {'inspector-debug': 12, 'inspector-release': 12, 'inspector-asan': 12,
           'inspector-gcc': 12, 'scene-shipping': 11, 'scene-no-shaders': 10}
env = os.environ.copy()
env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
           UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1',
           VK_LAYER_PATH=str(root / 'build/validation-layer/usr/share/vulkan/explicit_layer.d'))
env['LD_LIBRARY_PATH'] = str(root / 'build/validation-layer/usr/lib') + (
    ':' + env['LD_LIBRARY_PATH'] if env.get('LD_LIBRARY_PATH') else '')

def check(item):
    config, count = item
    build = root / 'build' / config
    prefix = out / f'root-final-{config}'
    row = {'configuration': config}
    with prefix.with_name(prefix.name + '-build.txt').open('w') as log:
        result = subprocess.run(['cmake', '--build', str(build), '-j', '2'], stdout=log,
                                stderr=subprocess.STDOUT, env=env, timeout=600)
    row['build'] = result.returncode
    if result.returncode:
        return row
    listing = subprocess.run(['ctest', '--test-dir', str(build), '--show-only=json-v1'],
                             capture_output=True, text=True, env=env, check=True)
    tests = json.loads(listing.stdout)
    assert len(tests['tests']) == count
    assert not any(p['name'] == 'DISABLED' and p['value']
                   for t in tests['tests'] for p in t['properties'])
    prefix.with_name(prefix.name + '-selection.json').write_text(json.dumps(tests, indent=2) + '\n')
    row['selected'] = count
    with prefix.with_name(prefix.name + '-tests.txt').open('w') as log:
        result = subprocess.run(['ctest', '--test-dir', str(build), '--output-on-failure',
                                 '--no-tests=error'], stdout=log, stderr=subprocess.STDOUT,
                                env=env, timeout=240)
    row['tests'] = result.returncode
    return row

with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    graphs = list(pool.map(check, configs.items()))
(out / 'root-final-graphs.json').write_text(json.dumps(graphs, indent=2) + '\n')
assert all(r['build'] == 0 and r.get('tests') == 0 for r in graphs), graphs
native = []
for config in ('inspector-debug', 'inspector-release', 'inspector-asan', 'inspector-gcc'):
    prefix = out / f'root-final-{config}-native'
    with prefix.with_suffix('.txt').open('w') as stdout, prefix.with_suffix('.err').open('w') as stderr:
        result = subprocess.run(['timeout', '--kill-after=5s', '120s',
                                 str(root / 'build' / config / 'inspector_probe')],
                                stdout=stdout, stderr=stderr, env=env, timeout=130)
    text = prefix.with_suffix('.txt').read_text()
    markers = ('scene_root_rigid=1', 'scene_root_copied=1', 'scene_root_lights=1',
               'scene_root_extras=1', 'scene_root_transparent_order=1')
    row = {'configuration': config, 'status': result.returncode,
           'stderr_bytes': prefix.with_suffix('.err').stat().st_size,
           'all_root_markers': all(m in text for m in markers)}
    native.append(row)
(out / 'root-final-native.json').write_text(json.dumps(native, indent=2) + '\n')
print(json.dumps({'graphs': graphs, 'native': native}, indent=2))
raise SystemExit(int(any(r['status'] or r['stderr_bytes'] or not r['all_root_markers'] for r in native)))
