from pathlib import Path
import concurrent.futures
import json
import subprocess

root = Path.cwd()
out = root / 'build/m3-flight'
expected = {root / p for p in ('engine/physics/world.cpp', 'tests/physics.cpp',
                              'game/flight/flight.cpp', 'tests/flight.cpp')}
tasks = []
for config in ('flight-debug', 'flight-release', 'flight-asan'):
    rows = json.loads((root / 'build' / config / 'compile_commands.json').read_text())
    selected = [r for r in rows if Path(r['file']) in expected]
    assert len(selected) == 4 and {Path(r['file']) for r in selected} == expected
    for i, row in enumerate(selected):
        directory = out / 'controller-ast' / config / str(i)
        directory.mkdir(parents=True, exist_ok=True)
        (directory / 'compile_commands.json').write_text(json.dumps([row], indent=2) + '\n')
        tasks.append((config, row['file'], directory))

def check(task):
    config, source, directory = task
    with (directory / 'analysis.txt').open('w') as log:
        process = subprocess.run(['clang-tidy', '-p', str(directory), source], stdout=log,
                                 stderr=subprocess.STDOUT, timeout=300)
    return {'configuration': config, 'source': str(Path(source).relative_to(root)),
            'status': process.returncode, 'flags': 'Complete actual command; no flags removed'}

with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    results = list(pool.map(check, tasks))
(out / 'controller-ast-results.json').write_text(json.dumps(results, indent=2) + '\n')
print(json.dumps(results, indent=2))
raise SystemExit(int(any(r['status'] for r in results)))
