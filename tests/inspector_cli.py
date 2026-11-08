"""Explicit native inspector UI/process verification; not a headless CTest."""
import csv
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

PROBE = Path(os.environ["NGEN_WORKLOAD_EXE"]).resolve(strict=True)
PACKAGE = Path(os.environ["NGEN_WORKLOAD_PACKAGE"]).resolve(strict=True)


class Inspector(unittest.TestCase):
    def test_repeated_reload_reaches_a_settled_tracked_memory_plateau(self):
        with tempfile.TemporaryDirectory(prefix="ngen inspector reload plateau ") as directory:
            report = Path(directory) / "plateau.csv"
            result = subprocess.run([str(PROBE), str(PACKAGE), "--inspector", "--walkthrough", "--no-resize", "--reload-cycles=2",
                                     "--frames=1200", "--warmup=2", "--validation", f"--report={report}"],
                                    capture_output=True, text=True, timeout=120, check=False)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(result.stderr, "")
            self.assertIn("inspector_reloads=2", result.stdout)
            self.assertIn("inspector_snapshot_generation=3", result.stdout)
            self.assertIn("inspector_resizes=0", result.stdout)
            with report.open() as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual(len(rows), 1200)
            self.assertEqual({(row["surface_width"], row["surface_height"]) for row in rows}, {("1280", "720")})
            settled = rows[-30:]
            self.assertEqual({row["generation"] for row in settled}, {"3"})
            self.assertEqual({row["reload_active"] for row in settled}, {"0"})
            self.assertEqual({row["resource_retiring"] for row in settled}, {"0"})
            self.assertEqual(len({row["resource_live"] for row in settled}), 1)
            self.assertEqual(len({row["vma_allocation_bytes"] for row in settled}), 1)
            self.assertGreater(int(settled[0]["vma_allocation_bytes"]), 0)

    def test_warmed_ui_has_no_observed_cpp_or_imgui_allocations(self):
        result = subprocess.run([str(PROBE), str(PACKAGE), "--inspector", "--frames=12", "--warmup=60"],
                                capture_output=True, text=True, timeout=90, check=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stderr, "")
        self.assertIn("inspector_ui=1", result.stdout)
        self.assertIn("warm_imgui_allocations=0", result.stdout)
        if "allocation_probe=1" in result.stdout:
            self.assertIn("warm_cpp_allocations=0", result.stdout)
        else:
            self.assertIn("allocation_probe=0", result.stdout)

    def test_scripted_walkthrough_applies_edits_then_keyboard_reload_and_resize(self):
        result = subprocess.run([str(PROBE), str(PACKAGE), "--inspector", "--walkthrough",
                                 "--frames=600", "--warmup=2", "--validation"],
                                capture_output=True, text=True, timeout=120, check=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stderr, "")
        for marker in ("inspector_ui=1", "walkthrough_edits=1", "walkthrough_keyboard_reload=1",
                       "inspector_reloads=1", "inspector_snapshot_generation=2", "inspector_resizes=2",
                       "workload_wsi_queued=600", "post_exposure=1.00 fxaa=1 bloom_strength=0.0400"):
            self.assertIn(marker, result.stdout)

    def test_native_inspector_is_composited_into_the_workload_and_capture(self):
        with tempfile.TemporaryDirectory(prefix="ngen inspector ui ") as directory:
            capture = Path(directory) / "inspector.bmp"
            report = Path(directory) / "inspector.csv"
            result = subprocess.run([str(PROBE), str(PACKAGE), "--inspector", "--frames=3", "--warmup=2",
                                     "--validation", f"--capture={capture}", f"--report={report}"],
                                    capture_output=True, text=True, timeout=90, check=False)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(result.stderr, "")
            self.assertIn("inspector_ui=1", result.stdout)
            self.assertIn("inspector_snapshot_generation=1", result.stdout)
            self.assertIn("workload_wsi_queued=3", result.stdout)
            image = capture.read_bytes()
            self.assertEqual(image[:2], b"BM")
            width, height = struct.unpack_from("<ii", image, 18)
            self.assertEqual((width, height), (1280, 720))
            offset = struct.unpack_from("<I", image, 10)[0]
            self.assertEqual(struct.unpack_from("<H", image, 28)[0], 32)
            # Unoccluded right-panel padding, not the underlying black flight scene.
            at = offset + ((height - 1 - 120) * width + 1256) * 4
            blue, green, red, _ = image[at:at + 4]
            self.assertTrue(10 <= red <= 30 and 18 <= green <= 36 and 25 <= blue <= 50,
                            f"native panel was missing from capture: {(red, green, blue)}")
            rows = report.read_text().splitlines()
            self.assertIn("gpu_ui_ns", rows[0].split(","))
            self.assertEqual(len(rows), 4)


if __name__ == "__main__":
    unittest.main()
