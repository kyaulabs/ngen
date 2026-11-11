from pathlib import Path
import concurrent.futures
import json
import os
import subprocess

root = Path.cwd()
out = root / 'build/m3-flight'
configs = ('flight-debug', 'flight-release', 'flight-asan', 'flight-gcc')

def check(config):
    env = os.environ.copy()
    env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
               UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    build = root / 'build' / config
    prefix = out / f'controller-final-{config}'
    result = {'configuration': config}
    with prefix.with_name(prefix.name + '-build.txt').open('w') as log:
        process = subprocess.run(['cmake', '--build', str(build), '-j', '2'],
                                 stdout=log, stderr=subprocess.STDOUT, env=env, timeout=300)
    result['build'] = process.returncode
    if process.returncode:
        return result
    selection = subprocess.run(['ctest', '--test-dir', str(build), '--show-only=json-v1'],
                               capture_output=True, text=True, env=env, check=True)
    tests = json.loads(selection.stdout)
    assert len(tests['tests']) == 6
    assert {t['name'] for t in tests['tests']} >= {'physics', 'flight'}
    assert not any(p['name'] == 'DISABLED' and p['value']
                   for t in tests['tests'] for p in t['properties'])
    prefix.with_name(prefix.name + '-selection.json').write_text(json.dumps(tests, indent=2) + '\n')
    result['selected'] = len(tests['tests'])
    with prefix.with_name(prefix.name + '-tests.txt').open('w') as log:
        process = subprocess.run(['ctest', '--test-dir', str(build), '--output-on-failure',
                                  '--no-tests=error'], stdout=log, stderr=subprocess.STDOUT,
                                 env=env, timeout=180)
    result['tests'] = process.returncode
    for binary in ('physics_tests', 'flight_tests'):
        dynamic = subprocess.run(['readelf', '-d', str(build / binary)],
                                 capture_output=True, text=True, check=True)
        prefix.with_name(prefix.name + f'-{binary}-dynamic.txt').write_text(dynamic.stdout)
        assert not any(s in dynamic.stdout.lower() for s in
                       ('libsdl', 'vulkan', 'libx11', 'wayland', 'libgl.', 'libegl', 'dxgi', 'd3d'))
    return result

with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    results = list(pool.map(check, configs))
(out / 'controller-final-graphs.json').write_text(json.dumps(results, indent=2) + '\n')
print(json.dumps(results, indent=2))
raise SystemExit(int(any(r['build'] or r.get('tests', 1) for r in results)))
