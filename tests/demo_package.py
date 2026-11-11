"""Public bundle creation with real files and mocked external build-tool boundaries."""
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True  # The public boundary tests must not dirty the source tree.
SPEC = importlib.util.spec_from_file_location("ngen_package_demo", ROOT / "tools/package_demo.py")
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)


class PackageDemo(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="ngen offline bundle ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.build = self.root / "build"
        self.sdk = self.root / "sdl"
        self.build.mkdir()
        (self.sdk / "lib").mkdir(parents=True)
        (self.sdk / "licenses").mkdir()
        (self.sdk / "lib/libSDL3.so.0").write_bytes(b"shared native SDL")
        (self.sdk / "licenses/LICENSE.txt").write_text("SDL zlib license\n")
        (self.build / "flight_demo").write_bytes(b"native flight executable")
        self.asset = self.root / "ship.ngas"
        self.asset.write_bytes(b"local cooked asset fixture")
        metadata = {"schema": 1, "os": "Linux", "configuration": "Release", "packages": [
            {"name": "sdl", "reference": "sdl/3.4.16@ngen/wayland", "root": str(self.sdk),
             "libdirs": ["lib"], "bindirs": ["bin"]}]}
        (self.build / "demo-runtime.json").write_text(json.dumps(metadata))
        self.output = self.root / "delivery"

    @staticmethod
    def external_tool(arguments, **_kwargs):
        result = ""
        if arguments[0] == "git":
            if "rev-parse" in arguments:
                result = "a" * 40 + "\n"
            elif "ls-tree" in arguments:
                result = "100644 blob " + "b" * 40 + "\tCMakeLists.txt\0"
            elif "archive" in arguments:
                destination = next(x.split("=", 1)[1] for x in arguments if x.startswith("--output="))
                Path(destination).write_bytes(b"corresponding source archive fixture")
        elif arguments[0] == "patchelf" and "--print-needed" in arguments:
            result = "libc.so.6\n"
        return subprocess.CompletedProcess(arguments, 0, stdout=result, stderr="")

    def test_bad_runtime_shapes_and_instrumented_delivery_reject_before_output(self):
        manifest = self.build / "demo-runtime.json"
        original = json.loads(manifest.read_text())
        malformed = dict(original, packages=[{"name": {}}])
        for value in [[], malformed, dict(original, sanitizer="address,undefined"), dict(original, os=[])]:
            with self.subTest(value=value):
                manifest.write_text(json.dumps(value))
                with self.assertRaises(ValueError):
                    PACKAGE.create_bundle(self.build, self.asset, self.output)
                self.assertFalse(self.output.exists())

    def test_bundled_sdl_without_its_notice_is_not_delivered(self):
        (self.sdk / "licenses/LICENSE.txt").unlink()
        with patch.object(PACKAGE.subprocess, "run", side_effect=self.external_tool):
            with self.assertRaisesRegex(ValueError, "SDL license"):
                PACKAGE.create_bundle(self.build, self.asset, self.output)
        self.assertFalse(self.output.exists())

    def test_nearby_but_unpinned_sdl_version_is_not_accepted(self):
        metadata = json.loads((self.build / "demo-runtime.json").read_text())
        metadata["packages"][0]["reference"] = "sdl/3.4.160@ngen/wayland"
        (self.build / "demo-runtime.json").write_text(json.dumps(metadata))
        with patch.object(PACKAGE.subprocess, "run", side_effect=self.external_tool):
            with self.assertRaisesRegex(ValueError, "pinned SDL"):
                PACKAGE.create_bundle(self.build, self.asset, self.output)
        self.assertFalse(self.output.exists())

    def test_dirty_source_cannot_be_claimed_as_the_archived_head(self):
        def dirty(arguments, **kwargs):
            if arguments[0] == "git" and "status" in arguments:
                return subprocess.CompletedProcess(arguments, 0, stdout=" M apps/flight_demo/app.cpp\n", stderr="")
            return self.external_tool(arguments, **kwargs)
        with patch.object(PACKAGE.subprocess, "run", side_effect=dirty):
            with self.assertRaisesRegex(ValueError, "committed source"):
                PACKAGE.create_bundle(self.build, self.asset, self.output)
        self.assertFalse(self.output.exists())

    def test_existing_owner_file_or_directory_is_never_replaced(self):
        self.output.write_bytes(b"owner bytes")
        with self.assertRaises(FileExistsError):
            PACKAGE.create_bundle(self.build, self.asset, self.output)
        self.assertEqual(self.output.read_bytes(), b"owner bytes")
        self.output.unlink()
        self.output.mkdir()
        with self.assertRaises(FileExistsError):
            PACKAGE.create_bundle(self.build, self.asset, self.output)
        self.assertTrue(self.output.is_dir())
        self.assertEqual(list(self.output.iterdir()), [])

    def test_runtime_escape_and_failed_external_tool_leave_no_partial_delivery(self):
        metadata = json.loads((self.build / "demo-runtime.json").read_text())
        metadata["packages"][0]["libdirs"] = [str(self.root)]
        (self.build / "demo-runtime.json").write_text(json.dumps(metadata))
        with self.assertRaises(ValueError):
            PACKAGE.create_bundle(self.build, self.asset, self.output)
        self.assertFalse(self.output.exists())
        metadata["packages"][0]["libdirs"] = ["lib"]
        (self.build / "demo-runtime.json").write_text(json.dumps(metadata))
        with patch.object(PACKAGE.subprocess, "run", side_effect=subprocess.CalledProcessError(17, ["patchelf"])):
            with self.assertRaises(subprocess.CalledProcessError):
                PACKAGE.create_bundle(self.build, self.asset, self.output)
        self.assertFalse(self.output.exists())
        self.assertEqual(self.asset.read_bytes(), b"local cooked asset fixture")
        self.assertEqual((self.sdk / "lib/libSDL3.so.0").read_bytes(), b"shared native SDL")

    def test_windows_layout_copies_native_dll_and_app_local_compiler_runtimes(self):
        metadata = json.loads((self.build / "demo-runtime.json").read_text())
        metadata["os"] = "Windows"
        metadata["packages"][0]["reference"] = "sdl/3.4.16"
        (self.build / "demo-runtime.json").write_text(json.dumps(metadata))
        (self.build / "flight_demo.exe").write_bytes(b"native Windows fixture")
        (self.sdk / "bin").mkdir()
        (self.sdk / "bin/SDL3.dll").write_bytes(b"SDL DLL fixture")
        runtime = self.root / "msvcp140.dll"
        runtime.write_bytes(b"compiler runtime fixture")
        (self.build / "demo-msvc-runtime.txt").write_text(str(runtime) + "\n")
        with patch.object(PACKAGE.subprocess, "run", side_effect=self.external_tool) as system:
            bundle = PACKAGE.create_bundle(self.build, self.asset, self.output)
        self.assertEqual((bundle / "SDL3.dll").read_bytes(), b"SDL DLL fixture")
        self.assertEqual((bundle / runtime.name).read_bytes(), runtime.read_bytes())
        self.assertIn(b"flight_demo.exe", (bundle / "run.cmd").read_bytes())
        self.assertFalse(any(call.args[0][0] == "patchelf" for call in system.call_args_list))

    def test_bundle_contains_local_asset_runtime_notices_and_integrity_without_sdk_paths(self):
        with patch.object(PACKAGE.subprocess, "run", side_effect=self.external_tool):
            bundle = PACKAGE.create_bundle(self.build, self.asset, self.output)
        self.assertEqual(bundle, self.output / "ngen-flight")
        self.assertEqual((bundle / "assets/ship.ngas").read_bytes(), self.asset.read_bytes())
        self.assertEqual((bundle / "lib/libSDL3.so.0").read_bytes(), b"shared native SDL")
        self.assertTrue((bundle / "run.sh").is_file())
        self.assertTrue((bundle / "source.tar.gz").is_file())
        self.assertTrue((bundle / "licenses/sdl/LICENSE.txt").is_file())
        manifest = json.loads((bundle / "manifest.json").read_text())
        self.assertEqual(manifest["files"]["assets/ship.ngas"], hashlib.sha256(self.asset.read_bytes()).hexdigest())
        self.assertNotIn(str(self.root), json.dumps(manifest))
        self.assertEqual((self.build / "flight_demo").read_bytes(), b"native flight executable")


if __name__ == "__main__":
    unittest.main()
