"""Exercise the standalone starter through the public project CLI."""

import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tarfile
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
CLI = ROOT / "tools/project.py"


def command(*args):
    return subprocess.run([sys.executable, str(CLI), *map(str, args)], capture_output=True,
                          text=True, timeout=30, check=False)


class CreatorProject(unittest.TestCase):
    def setUp(self):
        folder = tempfile.TemporaryDirectory(prefix="ngen creator ")
        self.addCleanup(folder.cleanup)
        self.root = Path(folder.name)
        self.project = self.root / "starter project"
        result = command("new", self.project, "--name", "starter")
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_version_and_shape_are_validated(self):
        valid = command("validate", self.project)
        self.assertEqual(valid.returncode, 0, valid.stderr)
        self.assertEqual(json.loads(valid.stdout)["schema"], 1)
        manifest = self.project / "ngen-project.json"
        for content in ('{"schema": 2, "name": "starter"}', '{"schema": true, "name": "starter"}',
                        '{"schema": 1, "schema": 1, "name": "starter"}',
                        '{"schema": 1, "name": "../escape"}',
                        '{"schema": 1, "name": "starter", "extra": []}', '[' * 4000):
            with self.subTest(content=content[:80]):
                manifest.write_text(content, encoding="utf-8")
                rejected = command("validate", self.project)
                self.assertNotEqual(rejected.returncode, 0, rejected.stdout)
                self.assertFalse(json.loads(rejected.stderr)["ok"])

    def test_package_is_source_only_and_cannot_replace_good_output(self):
        archive = self.root / "source.tar.gz"
        (self.project / "scene.ngas").write_bytes(b"not a source file")
        (self.project / "private-local.txt").write_bytes(b"not an archive member")
        packaged = command("package", self.project, "--output", archive)
        self.assertEqual(packaged.returncode, 0, packaged.stderr)
        with tarfile.open(archive) as source:
            self.assertEqual({member.name for member in source},
                             {"starter/" + name for name in ("CMakeLists.txt", "main.cpp", "marker.glb",
                                                            "ngen-project.json", "LICENSE")})
        original = archive.read_bytes()
        duplicate = command("package", self.project, "--output", archive)
        self.assertNotEqual(duplicate.returncode, 0)
        self.assertFalse(json.loads(duplicate.stderr)["ok"])
        self.assertEqual(archive.read_bytes(), original)

        (self.project / "marker.glb").write_bytes(b"not GLB")
        new_archive = self.root / "missing.tar.gz"
        invalid = command("package", self.project, "--output", new_archive)
        self.assertNotEqual(invalid.returncode, 0)
        self.assertIn("invalid GLB", json.loads(invalid.stderr)["error"])
        self.assertFalse(new_archive.exists())

    def test_existing_project_and_nonregular_sources_are_not_admitted(self):
        existing = command("new", self.project, "--name", "replacement")
        self.assertNotEqual(existing.returncode, 0)
        self.assertEqual(json.loads(command("validate", self.project).stdout)["name"], "starter")
        invalid = self.root / "invalid"
        self.assertNotEqual(command("new", invalid, "--name", "../unsafe").returncode, 0)
        self.assertFalse(invalid.exists())

        marker = self.project / "marker.glb"
        marker.unlink()
        target = self.root / "external fixture.glb"
        target.write_bytes(struct.pack("<4sII", b"glTF", 2, 20) + bytes(8))
        marker.symlink_to(target)
        self.assertNotEqual(command("validate", self.project).returncode, 0)
        marker.unlink()
        os.mkfifo(marker)
        self.assertNotEqual(command("validate", self.project).returncode, 0)

    @unittest.skipUnless(os.environ.get("NGEN_STARTER_BUILD"), "requires the built asset SDK and cooker graph")
    def test_fresh_source_archive_builds_cooks_tests_and_runs(self):
        def run(*args, success=True):
            result = subprocess.run(list(map(str, args)), capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
            return result

        build = Path(os.environ["NGEN_STARTER_BUILD"])
        configuration = os.environ["NGEN_STARTER_CONFIG"]
        prefix = self.root / "engine prefix"
        run("cmake", "--install", build, "--config", configuration, "--prefix", prefix)
        archive = self.root / "starter.tar.gz"
        run(sys.executable, CLI, "package", self.project, "--output", archive)
        original = archive.read_bytes()
        extracted = self.root / "unpacked"
        with tarfile.open(archive) as source:
            source.extractall(extracted, filter="data")
        project = extracted / "starter"
        run(sys.executable, CLI, "validate", project)

        def cook(src, output, success=True):
            return run(sys.executable, ROOT / "tools/assetc/cook.py", "--assetc", os.environ["NGEN_ASSETC_EXE"],
                       "--ktx", os.environ["NGEN_KTX_EXE"], "--probe", os.environ["NGEN_PACKAGE_PROBE"],
                       "--source", src, "--output", output, success=success)

        cooked = self.root / "cooked board"
        scene = cooked / "scene.ngas"
        cook(project / "marker.glb", cooked)
        expected = scene.read_bytes()
        invalid = self.root / "invalid.glb"
        invalid.write_bytes(b"not GLB")
        failed = self.root / "failed cook"
        rejected = cook(invalid, failed, success=False)
        self.assertIn("offline tool failed", rejected.stderr)
        self.assertFalse(failed.exists())
        cook(project / "marker.glb", cooked, success=False)
        self.assertEqual(scene.read_bytes(), expected)

        obj = self.root / "consumer build"
        run("cmake", "-S", project, "-B", obj, "-G", "Ninja", f"-DCMAKE_BUILD_TYPE={configuration}",
            f"-DCMAKE_TOOLCHAIN_FILE={os.environ['NGEN_STARTER_TOOLCHAIN']}",
            f"-DCMAKE_PREFIX_PATH={prefix}", f"-DNGEN_STARTER_SCENE={scene}")
        run("cmake", "--build", obj, "--parallel", "2")
        run("ctest", "--test-dir", obj, "--output-on-failure", "--no-tests=error")
        self.assertIn("piece moved 42 mm", run(obj / "ngen_starter", scene).stdout)
        scene.write_bytes(b"bad package")
        self.assertIn("scene:", run(obj / "ngen_starter", scene, success=False).stderr)
        (project / "main.cpp").write_text('#error controlled starter build failure\n', encoding="utf-8")
        self.assertIn("controlled starter build failure", run("cmake", "--build", obj, success=False).stdout)
        self.assertEqual(archive.read_bytes(), original)


if __name__ == "__main__":
    unittest.main()
