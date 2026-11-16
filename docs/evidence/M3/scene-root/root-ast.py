from pathlib import Path
import concurrent.futures
import json
import subprocess

root = Path.cwd()
out = root / 'build/m3-flight'
header = root / 'engine/render/scene_draw.hpp'
configs = ('inspector-debug', 'inspector-release', 'inspector-asan',
           'scene-shipping', 'scene-no-shaders')
tasks, scope = [], []
for config in configs:
    build = root / 'build' / config
    rows = json.loads((build / 'compile_commands.json').read_text())
    deps = subprocess.run(['ninja', '-C', str(build), '-t', 'deps'],
                          capture_output=True, text=True, check=True)
    objects = set()
    for block in deps.stdout.split('\n\n'):
        lines = block.splitlines()
        if lines and str(header) in {line.strip() for line in lines[1:]}:
            assert '(VALID)' in lines[0], lines[0]
            objects.add(lines[0].split(': #deps', 1)[0])
    selected = [row for row in rows
                if Path(row['output']).relative_to(build).as_posix() in objects]
    assert len(selected) == len(objects), (config, objects)
    if config != 'scene-no-shaders':
        assert any(Path(row['file']) == root / 'engine/render/scene_draw.cpp' for row in selected)
    scope.append({'configuration': config, 'header': str(header.relative_to(root)),
                  'consumers': [str(Path(row['file']).relative_to(root)) for row in selected]})
    for index, row in enumerate(selected):
        directory = out / 'root-ast' / config / str(index)
        directory.mkdir(parents=True, exist_ok=True)
        (directory / 'compile_commands.json').write_text(json.dumps([row], indent=2) + '\n')
        tasks.append((config, row['file'], directory))
(out / 'root-ast-scope.json').write_text(json.dumps(scope, indent=2) + '\n')
assert tasks

def check(task):
    config, source, directory = task
    with (directory / 'analysis.txt').open('w') as log:
        result = subprocess.run(['clang-tidy', '-p', str(directory), source], stdout=log,
                                stderr=subprocess.STDOUT, timeout=300)
    return {'configuration': config, 'source': str(Path(source).relative_to(root)),
            'status': result.returncode, 'flags': 'Complete actual command; no flags removed'}

with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    results = list(pool.map(check, tasks))
(out / 'root-ast-results.json').write_text(json.dumps(results, indent=2) + '\n')
print(json.dumps({'scope': scope, 'results': results}, indent=2))
raise SystemExit(int(any(row['status'] for row in results)))
