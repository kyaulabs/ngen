"""Public process checks for the offline scene-cooking boundary."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import struct
import zlib
import sys
import tempfile
import unittest

from asset_cli import triangle

EXE = os.environ["NGEN_ASSETC_EXE"]
KTX = os.environ["NGEN_KTX_EXE"]
PROBE = os.environ["NGEN_PACKAGE_PROBE"]
FAILURE_PROBE = Path(os.environ["NGEN_PACKAGE_FAILURE_PROBE"])
COOK = Path(__file__).resolve().parents[1] / "tools/assetc/cook.py"


def textured_triangle():
    original = triangle()
    size = struct.unpack_from("<I", original, 12)[0]
    document = json.loads(original[20:20 + size])
    binary = original[28 + size:]
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)
    pixels = bytes([0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255])
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">2I5B", 2, 2, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(b"\0" + pixels[:8] + b"\0" + pixels[8:])) + chunk(b"IEND", b"")
    document["bufferViews"].append({"buffer": 0, "byteOffset": len(binary), "byteLength": len(png)})
    document["images"] = [{"bufferView": 2, "mimeType": "image/png"}]
    document["textures"] = [{"source": 0}]
    document["materials"] = [{"pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}}]
    document["meshes"][0]["primitives"][0]["material"] = 0
    binary += png
    document["buffers"][0]["byteLength"] = len(binary)
    binary += b"\0" * (-len(binary) % 4)
    text = json.dumps(document, separators=(",", ":")).encode()
    text += b" " * (-len(text) % 4)
    return struct.pack("<5I", 0x46546c67, 2, 28 + len(text) + len(binary), len(text), 0x4e4f534a) + text + struct.pack("<2I", len(binary), 0x004e4942) + binary


def cook(source, output, probe=PROBE):
    return subprocess.run([sys.executable, str(COOK), "--assetc", EXE, "--ktx", KTX,
                           "--probe", str(probe), "--source", str(source), "--output", str(output)],
                          capture_output=True, text=True, timeout=60, check=False)


class SceneCooking(unittest.TestCase):
    def test_embedded_png_becomes_checked_native_ktx_mips(self):
        with tempfile.TemporaryDirectory(prefix="ngen textured scene ") as directory:
            root = Path(directory)
            source = root / "textured.glb"
            source.write_bytes(textured_triangle())
            result = cook(source, root / "package")
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(result.stderr, "")
            report = json.loads((root / "package/manifest.json").read_text())
            self.assertEqual(report["runtime"]["images"], 1)
            self.assertEqual(report["runtime"]["materials"], 1)
            self.assertEqual(report["runtime"]["texture_bytes"], 20)

    def test_late_runtime_rejection_does_not_publish_staged_bytes(self):
        with tempfile.TemporaryDirectory(prefix="ngen rejected package ") as directory:
            root = Path(directory)
            source = root / "triangle.glb"
            source.write_bytes(triangle())
            probe = root / ("rejecting probe" + FAILURE_PROBE.suffix)
            shutil.copy2(FAILURE_PROBE, probe)
            output = root / "package"
            result = cook(source, output, probe)
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertFalse(output.exists())
            self.assertFalse(list(root.glob(".ngen-cook-*")), "private staging was not cleaned")

    def test_failed_native_tool_stdout_diagnostic_is_retained(self):
        with tempfile.TemporaryDirectory(prefix="ngen tool diagnostic ") as directory:
            root = Path(directory)
            source = root / "triangle.glb"
            source.write_bytes(triangle())
            probe = root / ("diagnostic-probe" + FAILURE_PROBE.suffix)
            shutil.copy2(FAILURE_PROBE, probe)
            result = cook(source, root / "package", probe)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("EXPECTED_SCENE_VERIFY_FAILURE", result.stderr)

    def test_failed_cook_preserves_existing_package_and_publishes_nothing_new(self):
        with tempfile.TemporaryDirectory(prefix="ngen failed scene ") as directory:
            root = Path(directory)
            source = root / "triangle.glb"
            source.write_bytes(triangle())
            output = root / "existing"
            output.mkdir()
            marker = output / "owner-file"
            marker.write_bytes(b"preserve me")
            result = cook(source, output)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(marker.read_bytes(), b"preserve me")
            self.assertEqual(list(output.iterdir()), [marker])
            source.write_bytes(b"invalid GLB")
            fresh = root / "fresh"
            result = cook(source, fresh)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(fresh.exists())

    def test_canonical_package_loads_without_source_import_or_network(self):
        with tempfile.TemporaryDirectory(prefix="ngen canonical package ") as directory:
            root = Path(directory)
            source = root / "triangle.glb"
            source.write_bytes(triangle())
            source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
            packages = []
            for name in ("first", "repeat"):
                output = root / name
                result = subprocess.run([sys.executable, str(COOK), "--assetc", EXE,
                                         "--ktx", KTX, "--probe", PROBE, "--source", str(source),
                                         "--output", str(output)],
                                        capture_output=True, text=True, timeout=60, check=False)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                manifest = json.loads((output / "manifest.json").read_text())
                package = (output / "scene.ngas").read_bytes()
                self.assertEqual(manifest["source_sha256"], source_hash)
                self.assertEqual(manifest["package_sha256"], hashlib.sha256(package).hexdigest())
                self.assertEqual(manifest["runtime"]["lod_triangles"], [1, 1, 1])
                packages.append(package)
            self.assertEqual(packages[0], packages[1], "canonical package bytes changed")
            source.unlink()
            result = subprocess.run([PROBE, str(root / "first/scene.ngas")],
                                    capture_output=True, text=True, timeout=20, check=False)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(json.loads(result.stdout)["package_valid"], 1)

    def test_stage_retains_source_and_emits_typed_geometry_sections(self):
        with tempfile.TemporaryDirectory(prefix="ngen scene cooking ") as directory:
            root = Path(directory)
            source = root / "source with spaces.glb"
            source.write_bytes(triangle())
            expected = hashlib.sha256(source.read_bytes()).hexdigest()
            stage = root / "native stage"
            result = subprocess.run([EXE, "stage", "--preset", "laptop", str(source), str(stage)],
                                    capture_output=True, text=True, timeout=30, check=False)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            report = json.loads((stage / "stage.json").read_text())
            self.assertEqual(report["schema"], 1)
            self.assertEqual(report["preset"], "laptop")
            self.assertEqual(report["lod_triangles"], [1, 1, 1])
            self.assertEqual(report["images"], [])
            self.assertEqual((stage / "section-1.bin").stat().st_size, 3 * 56)
            self.assertEqual((stage / "section-3.bin").stat().st_size, 80)
            self.assertEqual(hashlib.sha256(source.read_bytes()).hexdigest(), expected)


if __name__ == "__main__":
    unittest.main()
