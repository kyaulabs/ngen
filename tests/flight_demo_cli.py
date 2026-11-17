"""Flight application admission; no display, GPU, audio device or private asset."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

EXE = Path(os.environ["NGEN_FLIGHT_DEMO"]).resolve(strict=True)
ENV = os.environ.copy()
ENV.pop("DISPLAY", None)
ENV.pop("WAYLAND_DISPLAY", None)


def invoke(*arguments):
    return subprocess.run([str(EXE), *map(str, arguments)], env=ENV,
                          capture_output=True, text=True, timeout=15, check=False)


class FlightAdmission(unittest.TestCase):
    def test_help_and_invalid_controls_do_not_start_native_presentation(self):
        help_result = invoke("--help")
        self.assertEqual(help_result.returncode, 0, help_result.stderr)
        self.assertIn("Usage: flight_demo", help_result.stdout)
        self.assertEqual(invoke().returncode, 2)
        for arguments in [("--replay",), ("--walkthrough",), ("--frames=0",),
                          ("--frames=-1",), ("--frames=3601",), ("--unknown",),
                          ("--frames=2", "--replay", "--walkthrough")]:
            with self.subTest(arguments=arguments):
                result = invoke("missing-flight-asset.ngas", *arguments)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertEqual(result.stdout, "")
                self.assertIn("Flight arguments:", result.stderr)

    def test_existing_owner_outputs_are_preserved_before_asset_or_native_startup(self):
        with tempfile.TemporaryDirectory(prefix="ngen flight admission ") as directory:
            for extension, option in [(".csv", "--report="), (".bmp", "--capture=")]:
                path = Path(directory) / ("owner" + extension)
                original = b"owner bytes must survive"
                path.write_bytes(original)
                result = invoke("missing-flight-asset.ngas", "--frames=2", option + str(path))
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertEqual(result.stdout, "")
                self.assertEqual(path.read_bytes(), original)

    def test_bad_package_rejects_without_a_display_or_device(self):
        with tempfile.TemporaryDirectory(prefix="ngen flight invalid ") as directory:
            package = Path(directory) / "invalid.ngas"
            package.write_bytes(b"not a cooked scene")
            result = invoke(package, "--verify")
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertEqual(result.stdout, "")
            self.assertIn("Flight demo:", result.stderr)
            self.assertNotIn("SDL", result.stderr)


if __name__ == "__main__":
    unittest.main()
