"""Opt-in physical-device flight capture regression; not a headless CTest.

Set NGEN_FLIGHT_DEMO and NGEN_FLIGHT_PACKAGE to existing absolute paths.
Uses bounded automated clients, not owner-controlled recording sessions.
"""
import csv
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

EXE = Path(os.environ["NGEN_FLIGHT_DEMO"]).resolve(strict=True)
PACKAGE = Path(os.environ["NGEN_FLIGHT_PACKAGE"]).resolve(strict=True)


def run(capture, report, width=1920):
    return subprocess.run(
        [str(EXE), str(PACKAGE), "--replay", "--frames=3", "--warmup=2",
         "--validation", "--mute", f"--width={width}", "--height=1080",
         f"--capture={capture}", f"--report={report}"],
        capture_output=True, text=True, timeout=180, check=False)


class FlightCapture(unittest.TestCase):
    def test_full_hd_capture_and_report_fit_the_bounded_evidence_budget(self):
        with tempfile.TemporaryDirectory(prefix="ngen flight full hd ") as directory:
            capture = Path(directory) / "full hd.bmp"
            report = Path(directory) / "full hd.csv"
            result = run(capture, report)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(result.stderr, "", "unexpected native/validation diagnostics")
            self.assertIn("flight_hdr_finite=1", result.stdout)
            self.assertIn("gpu_timing_rows=3", result.stdout)
            data = capture.read_bytes()
            self.assertLessEqual(len(data), 8 * 1024 * 1024)
            self.assertEqual(data[:2], b"BM")
            self.assertEqual(struct.unpack_from("<I", data, 2)[0], len(data))
            self.assertEqual(struct.unpack_from("<ii", data, 18), (1920, 1080))
            self.assertEqual(struct.unpack_from("<H", data, 28)[0], 32)
            offset = struct.unpack_from("<I", data, 10)[0]
            self.assertEqual(len(data) - offset, 1920 * 1080 * 4)
            with report.open(newline="") as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual(len(rows), 3)
            self.assertTrue(all(row["gpu_ready"] == "1" for row in rows))

    def test_full_hd_handoff_keeps_warm_hud_storage(self):
        result = subprocess.run(
            [str(EXE), str(PACKAGE), "--replay", "--frames=330", "--warmup=60",
             "--width=1920", "--height=1080", "--mute"],
            capture_output=True, text=True, timeout=180, check=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stderr, "", "unexpected native diagnostics")
        self.assertIn("validation=0", result.stdout)
        self.assertIn("gpu_timing_rows=330", result.stdout)
        self.assertIn("imgui_calls=0", result.stdout)

    def test_capture_above_the_budget_rejects_without_partial_evidence_files(self):
        with tempfile.TemporaryDirectory(prefix="ngen flight oversized capture ") as directory:
            capture = Path(directory) / "oversized.bmp"
            report = Path(directory) / "oversized.csv"
            result = run(capture, report, width=2048)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertEqual(
                result.stderr,
                "Flight demo: scene capture extent exceeds bounded encoding storage\n")
            self.assertFalse(capture.exists())
            self.assertFalse(report.exists())


if __name__ == "__main__":
    unittest.main()
