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

    def test_motion_vectors_select_runtime_without_changing_headless_verify(self):
        runtime = invoke("missing-flight-asset.ngas", "--motion-vectors")
        self.assertEqual(runtime.returncode, 1, runtime.stdout + runtime.stderr)
        self.assertIn("Flight demo:", runtime.stderr)
        conflict = invoke("missing-flight-asset.ngas", "--verify", "--motion-vectors")
        self.assertEqual(conflict.returncode, 2, conflict.stdout + conflict.stderr)
        self.assertIn("runtime rendering", conflict.stderr)

    def test_saved_survey_requires_opt_in_build_and_destination(self):
        with tempfile.TemporaryDirectory(prefix="ngen live survey admission ") as directory:
            survey = "--survey=" + directory
            result = invoke("missing-flight-asset.ngas", survey)
            self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
            if os.environ.get("NGEN_SURVEY_TEST") == "1":
                self.assertIn("survey requires a destination landmark", result.stderr)
                runtime = invoke("missing-flight-asset.ngas", survey,
                                 "--destination=missing-marker.ngas")
                self.assertEqual(runtime.returncode, 1, runtime.stdout + runtime.stderr)
                self.assertEqual(list(Path(directory).iterdir()), [])
                conflict = invoke("missing-flight-asset.ngas", survey, "--verify",
                                  "--destination=missing-marker.ngas")
                self.assertEqual(conflict.returncode, 2, conflict.stdout + conflict.stderr)
                self.assertIn("survey requires runtime flight", conflict.stderr)
                short = invoke("missing-flight-asset.ngas", survey, "--frames=10", "--replay",
                               "--destination=missing-marker.ngas")
                self.assertEqual(short.returncode, 2, short.stdout + short.stderr)
                self.assertIn("scratch save directory", short.stderr)
                self.assertEqual(list(Path(directory).iterdir()), [])
            else:
                self.assertIn("flight demo was built without Surveyors", result.stderr)

    @unittest.skipUnless(os.environ.get("NGEN_SURVEY_TEST") == "1", "opt-in Surveyors build")
    def test_soak_has_a_wall_clock_budget_and_never_reuses_an_owner_directory(self):
        with tempfile.TemporaryDirectory(prefix="ngen soak admission ") as directory:
            root = Path(directory)
            original = root / "owner-save"
            original.write_bytes(b"preserve owner bytes")
            dest = "--destination=missing-marker.ngas"
            conflict = invoke("missing-flight-asset.ngas", f"--survey={root}", dest,
                              "--soak-seconds=3600")
            self.assertEqual(conflict.returncode, 2, conflict.stdout + conflict.stderr)
            self.assertIn("new scratch directory", conflict.stderr)
            self.assertEqual(original.read_bytes(), b"preserve owner bytes")
            survey = f"--survey={root / 'fresh'}"
            for flags in [("--soak-seconds=0",), ("--soak-seconds=3601",),
                          ("--soak-seconds=3600", "--frames=10")]:
                with self.subTest(flags=flags):
                    result = invoke("missing-flight-asset.ngas", survey, dest, *flags)
                    self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
            result = invoke("missing-flight-asset.ngas", survey, dest, "--soak-seconds=3600")
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertFalse((root / "fresh").exists())

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
