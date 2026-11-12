"""Opt-in offline authoring failure checks with the private accepted base.

Requires the already-used NumPy authoring environment, not Vulkan or a running game.
Set NGEN_RCS_BASE. Probe failures/collisions mock only the external process boundary.
"""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class RcsAuthor(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = os.environ.get("NGEN_RCS_BASE")
        if not source:
            raise unittest.SkipTest("set private accepted NGEN_RCS_BASE")
        cls.base = Path(source)

    def invoke(self, base, output, probe):
        env = os.environ.copy()
        env["PYTHONDONTWRITEBYTECODE"] = "1"
        return subprocess.run([sys.executable, str(ROOT / "apps/flight_demo/author_rcs.py"),
                               "--base", str(base), "--output", str(output), "--probe", str(probe)],
                              capture_output=True, text=True, timeout=180, env=env, check=False)

    def test_existing_owner_output_is_preserved(self):
        with tempfile.TemporaryDirectory(prefix="ngen-rcs-owner-") as directory:
            output = Path(directory) / "owner"
            output.mkdir()
            sentinel = output / "keep.txt"
            sentinel.write_text("owner data\n")
            result = self.invoke(self.base, output, "not-invoked")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("output already exists", result.stderr)
            self.assertEqual(list(output.iterdir()), [sentinel])
            self.assertEqual(sentinel.read_text(), "owner data\n")

    def test_unrecognized_base_is_rejected_without_output(self):
        with tempfile.TemporaryDirectory(prefix="ngen-rcs-base-") as directory:
            base = Path(directory) / "wrong.ngas"
            base.write_bytes(b"not the accepted immutable source")
            output = Path(directory) / "new"
            result = self.invoke(base, output, "not-invoked")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("unexpected accepted base", result.stderr)
            self.assertFalse(output.exists())
            self.assertEqual(base.read_bytes(), b"not the accepted immutable source")

    def test_failed_probe_rolls_back_only_owned_output(self):
        false = shutil.which("false")
        if not false:
            self.skipTest("native false process unavailable")
        with tempfile.TemporaryDirectory(prefix="ngen-rcs-probe-") as directory:
            output = Path(directory) / "new"
            keep = Path(directory) / "keep.txt"
            keep.write_text("parent owner data\n")
            result = self.invoke(self.base, output, false)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(output.exists())
            self.assertEqual(keep.read_text(), "parent owner data\n")

    @unittest.skipUnless(os.name == "posix", "owned executable probe fixture uses POSIX shebang")
    def test_late_owner_manifest_collision_preserves_owner_file(self):
        with tempfile.TemporaryDirectory(prefix="ngen-rcs-publish-") as directory:
            output = Path(directory) / "new"
            probe = Path(directory) / "probe"
            probe.write_text(
                "#!" + sys.executable + "\n"
                "from pathlib import Path\nimport sys\n"
                "owner = Path(sys.argv[1]).parents[1] / 'manifest.json'\n"
                "with owner.open('x') as stream: stream.write('owner manifest\\n')\n"
                "print('{\"instances\":240,\"materials\":29}')\n")
            probe.chmod(0o700)
            result = self.invoke(self.base, output, probe)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse((output / "scene.ngas").exists())
            self.assertEqual((output / "manifest.json").read_text(), "owner manifest\n")
            self.assertEqual([p.name for p in output.iterdir()], ["manifest.json"])


if __name__ == "__main__":
    unittest.main()
