"""Real-process checks for the offline asset inspector's filesystem boundary."""
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

EXE = os.environ["NGEN_ASSETC_EXE"]


def triangle():
    binary = b"".join(struct.pack("<12f", *v) for v in (
        (0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1),
        (1, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 1),
        (0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1),
    )) + struct.pack("<4H", 0, 1, 2, 0)
    document = {"asset": {"version": "2.0"}, "buffers": [{"byteLength": 152}],
                "bufferViews": [{"buffer": 0, "byteLength": 144, "byteStride": 48},
                                {"buffer": 0, "byteOffset": 144, "byteLength": 6}],
                "accessors": [dict(bufferView=0, byteOffset=offset, componentType=5126,
                                   count=3, type=kind, **({"min": [0, 0, 0], "max": [1, 1, 0]} if offset == 0 else {}))
                              for offset, kind in ((0, "VEC3"), (12, "VEC3"), (24, "VEC2"), (32, "VEC4"))] +
                             [{"bufferView": 1, "componentType": 5123, "count": 3, "type": "SCALAR"}],
                "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2, "TANGENT": 3}, "indices": 4}]}],
                "nodes": [{"mesh": 0}], "scenes": [{"nodes": [0]}], "scene": 0}
    text = json.dumps(document, separators=(",", ":")).encode("utf-8")
    text += b" " * (-len(text) % 4)
    return struct.pack("<5I", 0x46546C67, 2, 28 + len(text) + len(binary), len(text), 0x4E4F534A) + text + struct.pack("<2I", len(binary), 0x004E4942) + binary


def invoke(*arguments):
    return subprocess.run([EXE, *map(str, arguments)], capture_output=True, text=True,
                          timeout=20, check=False)


class AssetCli(unittest.TestCase):
    def test_real_file_paths_and_explicit_cooking_transform(self):
        with tempfile.TemporaryDirectory(prefix="ngen asset CLI ") as directory:
            source = Path(directory) / "scene with spaces.glb"
            source.write_bytes(triangle())
            original = hashlib.sha256(source.read_bytes()).hexdigest()
            for options, rebuilt in (((), 0), (("--rebuild-tangents",), 1)):
                with self.subTest(rebuilt=rebuilt):
                    result = invoke("inspect", *options, source)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertEqual(result.stderr, "")
                    summary = json.loads(result.stdout)
                    self.assertEqual((summary["meshes"], summary["primitives"], summary["vertices"], summary["triangles"]), (1, 1, 3, 1))
                    self.assertEqual(summary["bounds"], {"minimum": [0, 0, 0], "maximum": [1, 1, 0]})
                    self.assertEqual(summary["rebuilt_primitives"], rebuilt)
                    self.assertEqual(summary["source_vertices"], 3)
            self.assertEqual(hashlib.sha256(source.read_bytes()).hexdigest(), original)
            self.assertEqual(list(Path(directory).iterdir()), [source], "inspection wrote unrequested output")

    def test_missing_directory_and_nonregular_input_fail_before_reading(self):
        with tempfile.TemporaryDirectory(prefix="ngen asset files ") as directory:
            paths = [Path(directory), Path(directory) / "missing.glb"]
            if hasattr(os, "mkfifo"):
                fifo = Path(directory) / "fifo.glb"
                os.mkfifo(fifo)
                paths.append(fifo)
            for path in paths:
                with self.subTest(path=path.name):
                    result = invoke("inspect", path)
                    self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                    self.assertEqual(result.stdout, "")
                    self.assertIn("regular file", result.stderr)

    def test_declared_source_size_is_rejected_without_reading_payload(self):
        with tempfile.TemporaryDirectory(prefix="ngen asset limit ") as directory:
            source = Path(directory) / "oversized.glb"
            with source.open("wb") as stream:
                stream.truncate(256 * 1024 * 1024 + 1)
            result = invoke("inspect", source)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertEqual(result.stdout, "")
            self.assertIn("source byte limit", result.stderr)

    def test_invalid_data_and_options_are_not_success_shaped(self):
        with tempfile.TemporaryDirectory(prefix="ngen asset invalid ") as directory:
            source = Path(directory) / "invalid.glb"
            source.write_bytes(b"not a GLB")
            result = invoke("inspect", source)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertEqual(result.stdout, "")
            for arguments in ((), ("cook", str(source)), ("inspect", "--unknown", str(source))):
                result = invoke(*arguments)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertIn("usage:", result.stderr)


if __name__ == "__main__":
    unittest.main()
