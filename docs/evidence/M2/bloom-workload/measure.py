"""Linux child-process measurement; no GPU ownership or runtime file polling added."""
import csv
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

root = Path.cwd()
mode = sys.argv[1]
assert mode in ('steady', 'stream')
cycles = int(sys.argv[3]) if len(sys.argv) > 3 else 3
assert 1 <= cycles <= 4
label = sys.argv[2]
assert re.fullmatch(r'[a-z0-9-]{1,40}', label)
prefix = root / 'build' / ('m2-' + label + '-' + mode)
package = root / 'build/cooked-candidate/frigate-canonical-first/scene.ngas'
exe = root / 'build/inspector-release/scene_workload'
paths = {key: Path(str(prefix) + suffix) for key, suffix in (
    ('report', '.csv'), ('capture', '.bmp'), ('stdout', '.txt'), ('stderr', '.err'),
    ('memory', '-memory.csv'), ('metadata', '.json'))}
assert not any(path.exists() or path.is_symlink() for path in paths.values()), 'outputs must be fresh'

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def bounded(path, limit):
    try:
        with path.open() as stream:
            text = stream.read(limit + 1)
    except (FileNotFoundError, ProcessLookupError):
        return ''
    assert len(text) <= limit, f'bounded metadata exceeded: {path.name}'
    return text

units = {'': 1, 'B': 1, 'KiB': 1024, 'kB': 1024, 'MiB': 1024 ** 2, 'GiB': 1024 ** 3}
def memory_value(text):
    match = re.fullmatch(r'(\d+)\s*(B|KiB|kB|MiB|GiB)?', text)
    assert match, 'unrecognized DRM memory unit'
    return int(match[1]) * units[match[2] or '']

def sample(pid, detail=False):
    status = bounded(Path(f'/proc/{pid}/status'), 16384)
    rss = re.search(r'^VmRSS:\s+(\d+)\s+kB$', status, re.MULTILINE)
    clients = {}
    try:
        files = list(Path(f'/proc/{pid}/fdinfo').iterdir())
    except (FileNotFoundError, ProcessLookupError):
        files = []
    assert len(files) <= 64, 'fdinfo admission exceeded'
    for path in files:
        text = bounded(path, 8192)
        fields = dict(line.split(':', 1) for line in text.splitlines() if ':' in line)
        if 'drm-driver' not in fields or 'drm-client-id' not in fields:
            continue
        fields = {key: value.strip() for key, value in fields.items() if key.startswith('drm-')}
        key = (fields['drm-driver'], fields.get('drm-pdev', ''), fields['drm-client-id'])
        clients[key] = fields # Duplicate descriptors for one DRM client are not summed twice.
    values = {}
    for category in ('total', 'resident', 'shared', 'active'):
        prefix = f'drm-{category}-'
        regions = [memory_value(value) for fields in clients.values() for key, value in fields.items() if key.startswith(prefix)]
        values[category] = sum(regions) if regions else None
    rollup = bounded(Path(f'/proc/{pid}/smaps_rollup'), 8192) if detail else ''
    parts = {}
    for name in ('Pss_Anon', 'Pss_Shmem', 'Private_Dirty'):
        match = re.search(r'^' + name + r':\s+(\d+)\s+kB$', rollup, re.MULTILINE)
        parts[name.lower() + '_bytes'] = int(match[1]) * 1024 if match else None
    return {'host_ns': time.monotonic_ns(), 'rss_bytes': int(rss[1]) * 1024 if rss else 0,
            'drm_clients': len(clients), **{f'drm_{key}_bytes': value for key, value in values.items()}, **parts}, list(clients.values())

sources = sorted(set(path for directory in ('engine/render', 'engine/tools', 'apps/scene_workload')
                     for path in (root / directory).glob('*pp')) | {root / 'shaders/post.slang', root / 'shaders/pbr.slang',
                     root / 'CMakeLists.txt', root / 'conan-inspector.lock'})
source_hashes = {str(path.relative_to(root)): sha(path) for path in sources}
identity = sha(package)
exe_identity = sha(exe)
arguments = [str(exe), str(package), '--inspector', '--frames=600', '--warmup=60',
             f'--capture={paths["capture"]}', f'--report={paths["report"]}']
if mode == 'stream':
    arguments += ['--walkthrough', '--no-resize', f'--reload-cycles={cycles}']
settings = {'mode': mode, 'frames': 600, 'warmup': 60, 'width': 1280, 'height': 720,
            'ship_lod': 2, 'asteroid_instances': 100, 'asteroid_triangles': 80,
            'shadow_extent': 1024, 'validation': False, 'gpu_timestamps': True,
            'bloom_strength': .04, 'bloom_taps': 25, 'bloom_footprint': '9x9',
            'gpu_post_includes_bloom': True, 'ui': True, 'reload_cycles': cycles if mode == 'stream' else 0,
            'resize': False, 'cycle_settle_frames': 30,
            'filesystem_cache': 'package identity hashing before launch warms the same-file reload cache; not cold storage latency',
            'script': 'stream: queued edit at frame 0, verify at 1, restore exposure/FXAA at 3; keyboard reloads; fixed 1280x720 throughout; 30 settling frames between reloads',
            'source_sha256': source_hashes}
settings_hash = hashlib.sha256(json.dumps(settings, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
records = []
peak_clients = []
peak_drm = -1
started = time.monotonic_ns()
with paths['stdout'].open('x') as output, paths['stderr'].open('x') as errors:
    child = subprocess.Popen(arguments, stdout=output, stderr=errors)
    try:
        while True:
            pid, status, usage = os.wait4(child.pid, os.WNOHANG)
            if pid:
                child.returncode = os.waitstatus_to_exitcode(status)
                break
            if time.monotonic_ns() - started > 160_000_000_000:
                raise RuntimeError('native measurement deadline exceeded')
            row, clients = sample(child.pid, len(records) % 10 == 0)
            records.append(row)
            if row['drm_total_bytes'] is not None and row['drm_total_bytes'] > peak_drm:
                peak_drm, peak_clients = row['drm_total_bytes'], clients
            time.sleep(.025)
    except BaseException:
        if child.returncode is None:
            child.kill()
            _, status, _ = os.wait4(child.pid, 0)
            child.returncode = os.waitstatus_to_exitcode(status)
        raise
ended = time.monotonic_ns()
assert child.returncode == 0, f'native exit {child.returncode}; inspect selected logs'
assert paths['stderr'].stat().st_size == 0, 'unexpected native diagnostics'
assert len(records) <= 6400
assert identity == sha(package) and exe_identity == sha(exe), 'input identity changed'
assert all(sha(root / path) == value for path, value in source_hashes.items()), 'source changed'
with paths['memory'].open('x', newline='') as stream:
    writer = csv.DictWriter(stream, fieldnames=records[0].keys())
    writer.writeheader()
    writer.writerows(records)
with paths['report'].open() as stream:
    frames = list(csv.DictReader(stream))
assert len(frames) == 600 and all(started <= int(row['host_ns']) <= ended for row in frames), 'clock/row correlation unavailable'
assert all(row['surface_width'] == '1280' and row['surface_height'] == '720' for row in frames), 'measured presentation extent changed'
text = bounded(paths['stdout'], 65536)
assert 'post_exposure=1.00 fxaa=1 bloom_strength=0.0400' in text
assert 'total_triangles=246080' in text and 'allocation_probe=1' in text
assert peak_drm > 0, 'process DRM memory unavailable'
if mode == 'steady':
    assert 'warm_cpp_allocations=0' in text and 'warm_imgui_allocations=0' in text
else:
    assert f'inspector_reloads={cycles}' in text and f'inspector_snapshot_generation={cycles + 1}' in text
    assert 'inspector_resizes=0' in text

def distribution(values):
    values = sorted(values)
    return {f'p{percent}': values[(len(values) * percent + 99) // 100 - 1] for percent in (50, 95, 99)} | {'max': values[-1]}
metrics = {field: distribution([float(row[field]) for row in frames]) for field in (
    'frame_ns', 'prepare_ns', 'present_ns', 'ui_prepare_ns', 'gpu_shadow_ns', 'gpu_pbr_ns', 'gpu_post_ns', 'gpu_ui_ns')}
plateaus = []
for generation in sorted({int(row['generation']) for row in frames}):
    rows = [row for row in frames if int(row['generation']) == generation and row['reload_active'] == '0' and row['resource_retiring'] == '0']
    if len(rows) < 15:
        continue
    rows = rows[-15:]
    low, high = int(rows[0]['host_ns']), int(rows[-1]['host_ns'])
    observed = [row for row in records if low <= row['host_ns'] <= high and row['drm_clients'] > 0]
    plateaus.append({'generation': generation, 'first_frame': int(rows[0]['frame']), 'last_frame': int(rows[-1]['frame']),
        'vma_allocation_bytes': sorted({int(row['vma_allocation_bytes']) for row in rows}),
        'vma_reserved_bytes': sorted({int(row['vma_reserved_bytes']) for row in rows}),
        'resource_live': sorted({int(row['resource_live']) for row in rows}),
        'rss_bytes': [min(row['rss_bytes'] for row in observed), max(row['rss_bytes'] for row in observed)] if observed else None,
        'drm_total_bytes': sorted({row['drm_total_bytes'] for row in observed if row['drm_total_bytes'] is not None}),
        'rss_components': {field: sorted({row[field] for row in observed if row[field] is not None}) for field in ('pss_anon_bytes', 'pss_shmem_bytes', 'private_dirty_bytes')}})
verdicts = {'median_le_50ms': metrics['frame_ns']['p50'] <= 50e6,
            'p95_le_66_7ms': metrics['frame_ns']['p95'] <= 66.7e6,
            'p99_le_100ms': metrics['frame_ns']['p99'] <= 100e6,
            'no_frame_over_200ms': metrics['frame_ns']['max'] <= 200e6,
            'sampled_process_drm_total_le_1gib': peak_drm <= 1024 ** 3,
            'cold_inclusive_linux_peak_rss_le_6gib': usage.ru_maxrss * 1024 <= 6 * 1024 ** 3}
metadata = {'settings': settings, 'settings_sha256': settings_hash, 'package_sha256': identity,
    'package_bytes': package.stat().st_size, 'exe_sha256': exe_identity,
    'compile_database_sha256': sha(root / 'build/inspector-release/compile_commands.json'),
    'distributions_ns': metrics, 'process_exit': child.returncode,
    'wall_seconds_including_cold_work': (ended - started) / 1e9,
    'linux_wait4_maxrss_kib_including_cold_work': usage.ru_maxrss,
    'user_cpu_seconds_including_cold_work': usage.ru_utime, 'system_cpu_seconds_including_cold_work': usage.ru_stime,
    'memory_sample_period_seconds': .025, 'memory_samples': len(records), 'sampled_peak_drm_total_bytes': peak_drm,
    'sampled_peak_drm_resident_bytes': max(row['drm_resident_bytes'] or 0 for row in records),
    'peak_drm_client_fields': peak_clients, 'clock_correlation': 'Linux steady_clock and parent CLOCK_MONOTONIC validated in run interval',
    'plateaus': plateaus, 'scoped_budget_observations': verdicts,
    'artifacts': {key: {'sha256': sha(path), 'bytes': path.stat().st_size} for key, path in paths.items() if key != 'metadata'},
    'notes': 'DRM totals are deduplicated per-process i915 client/region counters including shared objects, not whole-device/global usage. Peaks are sampled, not hard bounds. VMA totals, driver residency and CPU RSS are distinct; Linux RSS is not Windows working set. No owner/independent acceptance.'}
with paths['metadata'].open('x') as stream:
    json.dump(metadata, stream, indent=2)
    stream.write('\n')
print(json.dumps({'metadata': str(paths['metadata']), 'budgets': verdicts, 'plateaus': plateaus}, indent=2))
assert all(verdicts.values()), 'scoped workload budget failure; retained artifacts'
