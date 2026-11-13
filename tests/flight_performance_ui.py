"""Opt-in native F12 routing/reset/mode/telemetry regression, not headless CTest."""
import os
import re
import subprocess
import unittest


@unittest.skipUnless(os.getenv("NGEN_FLIGHT_DEMO") and os.getenv("NGEN_FLIGHT_PACKAGE"),
                     "Set native NGEN_FLIGHT_DEMO/PACKAGE")
class PerformanceOverlay(unittest.TestCase):
    def test_hidden_startup_does_not_draw_or_poll_os_counters(self):
        result = subprocess.run(
            [os.environ["NGEN_FLIGHT_DEMO"], os.environ["NGEN_FLIGHT_PACKAGE"],
             "--replay", "--frames=3", "--warmup=2", "--mute"],
            capture_output=True, text=True, timeout=180)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stderr, "")
        self.assertIn("performance_visible=0 performance_toggles=0 performance_drawn_frames=0", result.stdout)
        self.assertIn("os_samples=0 system_ram_available=0", result.stdout)

    def test_f12_routes_and_survives_mode_focus_and_scene_resets(self):
        result = subprocess.run(
            [os.environ["NGEN_FLIGHT_DEMO"], os.environ["NGEN_FLIGHT_PACKAGE"],
             "--walkthrough", "--frames=330", "--warmup=60", "--validation", "--mute"],
            capture_output=True, text=True, timeout=180)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stderr, "")
        self.assertIn("flight_hdr_finite=1", result.stdout)
        self.assertIn("gpu_timing_rows=330", result.stdout)
        self.assertIn("performance_visible=1 performance_toggles=3", result.stdout)
        drawn = re.search(r"performance_drawn_frames=(\d+)", result.stdout)
        self.assertIsNotNone(drawn, result.stdout)
        self.assertGreaterEqual(int(drawn.group(1)), 200)
        self.assertIn("scope=system_device", result.stdout)
        self.assertIn("system_ram_available=1", result.stdout)
        self.assertIn("performance_gpu_ready=1", result.stdout)
        ram = re.search(r"system_ram_used_bytes=(\d+) system_ram_total_bytes=(\d+)", result.stdout)
        self.assertIsNotNone(ram, result.stdout)
        used, total = map(int, ram.groups())
        self.assertGreater(total, 0)
        self.assertLessEqual(used, total)


if __name__ == "__main__":
    unittest.main()
