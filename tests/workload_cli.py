"""Explicit physical-device ship/asteroid workload, not headless CTest."""
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

PROBE = Path(os.environ["NGEN_WORKLOAD_EXE"]).resolve(strict=True)
PACKAGE = Path(os.environ["NGEN_WORKLOAD_PACKAGE"]).resolve(strict=True)


class Workload(unittest.TestCase):
    def test_checked_ship_and_hundred_instanced_asteroids_are_presented(self):
        with tempfile.TemporaryDirectory(prefix="ngen flight workload ") as directory:
            capture = Path(directory) / "flight.bmp"
            report = Path(directory) / "flight.csv"
            result = subprocess.run([str(PROBE), str(PACKAGE), "--frames=2", "--warmup=1",
                                     "--validation", f"--capture={capture}", f"--report={report}"],
                                    capture_output=True, text=True, timeout=90, check=False)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(result.stderr, "")
            for marker in ("asteroid_instances=100", "asteroid_triangles=80", "ship_triangles=238080",
                           "total_triangles=246080", "workload_frames=2", "workload_wsi_queued=2",
                           "workload_hdr_finite=1", "workload_capture=1", "bloom_strength=0.0400"):
                self.assertIn(marker, result.stdout)
            # Validation layers allocate internally; their work remains included
            # in this process-wide C++ counter, not hidden or treated as performance.
            self.assertIn("allocation_probe=", result.stdout)
            self.assertIn("validation=1", result.stdout)
            self.assertIn("gpu_timestamps=available", result.stdout)
            image = capture.read_bytes()
            self.assertEqual(image[:2], b"BM")
            self.assertEqual(struct.unpack_from("<ii", image, 18), (1280, 720))
            rows = report.read_text().splitlines()
            self.assertEqual(rows[0].split(",")[:2], ["frame", "frame_ns"])
            self.assertEqual(len(rows), 3)

    def test_warmed_performance_path_has_no_observed_cpp_new(self):
        result = subprocess.run([str(PROBE), str(PACKAGE), "--frames=8", "--warmup=60"],
                                capture_output=True, text=True, timeout=90, check=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stderr, "")
        self.assertIn("validation=0", result.stdout)
        if "allocation_probe=1" in result.stdout:
            self.assertIn("warm_cpp_allocations=0", result.stdout)
        else:
            self.assertIn("allocation_probe=0", result.stdout)  # Sanitizer allocator untouched.

    def test_bad_controls_and_existing_outputs_reject_before_native_execution(self):
        for argument in ("--frames=0", "--frames=-1", "--frames=3601", "--warmup=601", "--unknown", "--walkthrough", "--reload-cycles=0", "--reload-cycles=5", "--reload-cycles=2", "--no-resize"):
            with self.subTest(argument=argument):
                result = subprocess.run([str(PROBE), str(PACKAGE), argument],
                                        capture_output=True, text=True, timeout=10, check=False)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertIn("Workload arguments:", result.stderr)
                self.assertEqual(result.stdout, "")
        with tempfile.TemporaryDirectory(prefix="ngen-flight-owner-") as temporary:
            for suffix, option in ((".bmp", "--capture="), (".csv", "--report=")):
                path = Path(temporary) / ("owner" + suffix)
                path.write_bytes(b"owner data must survive")
                result = subprocess.run([str(PROBE), str(PACKAGE), option + str(path)],
                                        capture_output=True, text=True, timeout=10, check=False)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertEqual(result.stdout, "")
                self.assertEqual(path.read_bytes(), b"owner data must survive")

    def test_bloom_can_be_disabled_without_disabling_the_workload(self):
        result = subprocess.run([str(PROBE), str(PACKAGE), "--frames=2", "--warmup=1", "--no-bloom"],
                                capture_output=True, text=True, timeout=90, check=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stderr, "")
        self.assertIn("bloom_strength=0.0000", result.stdout)
        self.assertIn("workload_wsi_queued=2", result.stdout)
        self.assertIn("workload_hdr_finite=1", result.stdout)

    def test_gpu_diagnostics_can_be_disabled_without_disabling_the_workload(self):
        result = subprocess.run([str(PROBE), str(PACKAGE), "--frames=2", "--warmup=1",
                                 "--no-gpu-timings"], capture_output=True, text=True,
                                timeout=90, check=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stderr, "")
        self.assertIn("gpu_timestamps=unavailable", result.stdout)
        self.assertIn("workload_wsi_queued=2", result.stdout)
        self.assertIn("workload_hdr_finite=1", result.stdout)


if __name__ == "__main__":
    unittest.main()
