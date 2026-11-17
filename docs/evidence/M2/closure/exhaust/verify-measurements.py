"""Recompute every retained CSV row; no resize/reload frames are exempted."""
from pathlib import Path
import csv
import hashlib
import json
import math
import re
import sys


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def analyze(root, stem, mixed):
    text = (root / (stem + ".txt")).read_text()
    assert (root / (stem + ".err")).stat().st_size == 0
    with (root / (stem + ".csv")).open(newline="") as stream:
        rows = list(csv.DictReader(stream))
    assert len(rows) == 600
    for index, row in enumerate(rows):
        assert int(row["frame"]) == index
        assert all(math.isfinite(float(value)) and float(value) >= 0 for value in row.values())
    host = [int(row["host_ns"]) for row in rows]
    assert all(a < b for a, b in zip(host, host[1:]))
    frames = sorted(int(row["frame_ns"]) for row in rows)
    timing = {f"p{p}": frames[(len(frames) * p + 99) // 100 - 1] for p in (50, 95, 99)}
    timing["max"] = frames[-1]
    for key, value in timing.items():
        assert re.search(rf"(?:^|\s){key}={value}(?:\s|$)", text)
    assert timing["p50"] <= 50000000 and timing["p95"] <= 66700000
    assert timing["p99"] <= 100000000 and timing["max"] <= 200000000
    assert "ship_triangles=238272 total_triangles=246272 scene_draws=195" in text
    assert "fixture_crc32=0ceb739b package_crc32=091e9782" in text
    assert "workload_hdr_finite=1" in text and "shadow_extent=1024 validation=0" in text
    assert "post_exposure=1.00 fxaa=1 bloom_strength=0.0400" in text
    extents = {}
    transitions = []
    generations = []
    for index, row in enumerate(rows):
        extent = f"{row['surface_width']}x{row['surface_height']}"
        extents[extent] = extents.get(extent, 0) + 1
        if index == 0 or extent != transitions[-1]["extent"]:
            transitions.append({"frame": index, "extent": extent})
        generation = int(row["generation"])
        if index == 0 or generation != generations[-1]["generation"]:
            generations.append({"frame": index, "generation": generation})
    if mixed:
        assert len(transitions) == 3 and set(extents) == {"1280x720", "1024x640"}
        assert [entry["generation"] for entry in generations] == [1, 2, 3, 4, 5]
        assert "inspector_resizes=2 inspector_reloads=4" in text
        assert "inspector_reload_phase=idle inspector_completed_reloads=4" in text
    else:
        assert extents == {"1280x720": 600} and len(generations) == 1
        assert "warm_cpp_allocations=0" in text and "warm_imgui_allocations=0" in text
    tail = rows[-300:]
    assert all(int(row["resource_retiring"]) == 0 and int(row["reload_active"]) == 0
               for row in tail)
    tracked = {}
    for column in ("vma_allocation_bytes", "vma_reserved_bytes", "resource_live", "resource_retiring"):
        values = [int(row[column]) for row in rows]
        settled = [int(row[column]) for row in tail]
        tracked[column] = {"peak": max(values), "final": values[-1],
                           "tail_min": min(settled), "tail_max": max(settled)}
    assert tracked["vma_allocation_bytes"]["peak"] <= 512 * 1024 ** 2
    return {"measured_frames": 600, "warmup_frames": 60, "frame_ns": timing,
            "extents": extents, "extent_transitions": transitions,
            "generation_transitions": generations, "tracked": tracked,
            "steady_allocation_check": "not applicable: scripted resize/reload/edit activity"
                if mixed else "zero warm C++ and ImGui allocation calls",
            "settings_scope": "walkthrough frames 1-2 exposure .75/FXAA off; restored at 3; all frames retained"
                if mixed else "default exposure 1/FXAA on throughout",
            "artifact_sha256": {extension: digest(root / (stem + extension))
                                for extension in (".bmp", ".csv", ".txt", ".err")
                                if (root / (stem + extension)).is_file()},
            "capture_hash_recomputed": (root / (stem + ".bmp")).is_file()}


def main():
    root = Path(sys.argv[1])
    print(json.dumps({"steady": analyze(root, "plume-wide-steady-clean1", False),
                      "mixed": analyze(root, "plume-wide-mixed-clean1", True),
                      "scope": "Native Iris Xe warm-cache timing and VMA records; no new whole-process memory observation."},
                     indent=2))


if __name__ == "__main__":
    main()
