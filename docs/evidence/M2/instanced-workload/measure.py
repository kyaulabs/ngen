import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

root = Path.cwd()
package = root / 'build/cooked-candidate/frigate-canonical-first/scene.ngas'
report = root / 'build/ship-asteroids-verified.csv'
capture = root / 'build/ship-asteroids-verified.bmp'
settings = {'width': 1280, 'height': 720, 'shadow_extent': 1024, 'ship_lod': 2,
            'asteroid_instances': 100, 'asteroid_triangles': 80, 'fixture_seed': 0x4e47454e,
            'replay_period': 600, 'frames': 600, 'warmup': 60, 'validation': False,
            'exposure': 1, 'fxaa': True, 'gpu_timestamps': True}
def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()
settings['source_sha256'] = {path: sha(root / path) for path in (
    'apps/scene_workload/fixture.cpp', 'apps/scene_workload/fixture.hpp',
    'apps/scene_workload/main.cpp', 'apps/scene_workload/allocations.cpp',
    'engine/render/environment.cpp', 'engine/render/scene_draw.cpp',
    'engine/render/resources.cpp', 'engine/render/mesh.cpp', 'engine/render/post.cpp',
    'engine/render/presentation.cpp', 'shaders/pbr.slang', 'shaders/post.slang')}
identity = sha(package)
settings_bytes = json.dumps(settings, sort_keys=True, separators=(',', ':')).encode()
start = time.monotonic()
with Path('/tmp/ngen-workload-release-verified.txt').open('w') as output:
    child = subprocess.Popen([str(root / 'build/scene-release/scene_workload'), str(package),
                              '--frames=600', '--warmup=60', f'--capture={capture}', f'--report={report}'],
                             stdout=output, stderr=subprocess.STDOUT)
    while True:
        pid, status, usage = os.wait4(child.pid, os.WNOHANG)
        if pid:
            child.returncode = os.waitstatus_to_exitcode(status)
            break
        if time.monotonic() - start > 160:
            child.kill()
            _, status, usage = os.wait4(child.pid, 0)
            child.returncode = os.waitstatus_to_exitcode(status)
            raise RuntimeError('native measurement deadline exceeded')
        time.sleep(.05)
assert child.returncode == 0, child.returncode
assert identity == sha(package), 'package changed during measurement'
assert all(sha(root / path) == value for path, value in settings['source_sha256'].items())
metadata = {'package_sha256': identity, 'package_bytes': package.stat().st_size,
            'settings': settings, 'settings_sha256': hashlib.sha256(settings_bytes).hexdigest(),
            'capture_sha256': sha(capture), 'capture_bytes': capture.stat().st_size,
            'report_sha256': sha(report), 'report_bytes': report.stat().st_size,
            'process_exit': child.returncode, 'wall_seconds_including_cold_work': time.monotonic() - start,
            'linux_wait4_maxrss_kib_including_cold_work': usage.ru_maxrss,
            'user_cpu_seconds_including_cold_work': usage.ru_utime,
            'system_cpu_seconds_including_cold_work': usage.ru_stime,
            'notes': 'No UI/bloom yet. wait4 peak RSS is Linux process residency, not whole GPU memory. '}
(root / 'build/workload-check/measurement.json').write_text(json.dumps(metadata, indent=2) + '\n')
print(json.dumps(metadata, indent=2))
