import concurrent.futures
import hashlib
import json
from pathlib import Path
import subprocess
import sys

root = Path.cwd()
headers = {'engine/render/context.hpp', 'engine/render/resources.hpp', 'engine/render/resources_internal.hpp',
           'engine/render/shader_contract.hpp', 'engine/render/overlay.hpp', 'engine/tools/ui.hpp',
           'apps/probe/capture.hpp', 'engine/render/post.hpp', 'engine/tools/inspector.hpp'}
sources = {'engine/render/presentation.cpp', 'engine/render/resources.cpp', 'engine/render/overlay.cpp',
           'engine/tools/ui.cpp', 'apps/scene_workload/main.cpp', 'apps/inspector_probe/main.cpp',
           'apps/probe/capture.cpp', 'tests/shader_layout.cpp', 'apps/resource_probe/main.cpp',
           'engine/tools/inspector.cpp', 'tests/inspector.cpp'}
absolute = {str(root / path) for path in headers}
configurations = ('inspector-debug', 'inspector-release', 'inspector-asan', 'scene-shipping',
                  'scene-no-shaders', 'inspector-tsan')
tasks = []
excluded = []
for config in configurations:
    build = root / 'build' / config
    metadata = build / 'compile_commands.json'
    entries = json.loads(metadata.read_text())
    result = subprocess.run(['ninja', '-C', str(build), '-t', 'deps'], capture_output=True, text=True, check=True)
    dependencies = {}
    current = None
    for line in result.stdout.splitlines():
        if ': #deps ' in line:
            current = line.split(': #deps ', 1)[0]
            dependencies[current] = set()
        elif current is not None and line.startswith('    '):
            dependencies[current].add(line.strip())
    for index, entry in enumerate(entries):
        try:
            source = Path(entry['file']).relative_to(root).as_posix()
        except ValueError:
            excluded.append({'configuration': config, 'source': entry['file'], 'reason': 'third-party source; native build/sanitizers, not owned-policy AST'})
            continue
        obj = Path(entry['output']).relative_to(build).as_posix()
        if source not in sources and not absolute.intersection(dependencies.get(obj, set())):
            continue
        if sys.argv[1:] and source not in sys.argv[1:]:
            continue
        target = root / 'build/bloom-check/ast' / config / str(index)
        target.mkdir(parents=True, exist_ok=True)
        (target / 'compile_commands.json').write_text(json.dumps([entry], indent=2) + '\n')
        tasks.append((config, source, obj, target, hashlib.sha256(metadata.read_bytes()).hexdigest()))

def check(task):
    config, source, obj, target, identity = task
    with (target / 'analysis.txt').open('w') as output:
        result = subprocess.run(['clang-tidy', '-p', str(target), str(root / source)], stdout=output,
                                stderr=subprocess.STDOUT, timeout=150, check=False)
    return {'configuration': config, 'source': source, 'object': obj, 'status': result.returncode,
            'compile_database_sha256': identity, 'analysis': str((target / 'analysis.txt').relative_to(root))}

with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
    results = list(pool.map(check, tasks))
name = 'ast-rerun-results.json' if sys.argv[1:] else 'ast-results.json'
(root / 'build/bloom-check' / name).write_text(json.dumps({'results': results, 'excluded': excluded}, indent=2) + '\n')
for result in results:
    print(result['configuration'], result['source'], result['object'], result['status'])
raise SystemExit(1 if not results or any(result['status'] for result in results) else 0)
