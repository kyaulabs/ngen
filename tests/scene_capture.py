"""Explicit physical-device scene capture checks; never registered as headless CTest.

Set NGEN_SCENE_PROBE and NGEN_SCENE_PACKAGE to existing absolute paths.
"""
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import threading
import unittest

PROBE = Path(os.environ["NGEN_SCENE_PROBE"]).resolve(strict=True)
PACKAGE = Path(os.environ["NGEN_SCENE_PACKAGE"]).resolve(strict=True)


def run(capture):
    return subprocess.run([str(PROBE), str(PACKAGE), str(capture)],
                          capture_output=True, text=True, timeout=90, check=False)


class SceneCapture(unittest.TestCase):
    def test_native_post_processed_target_resolution_capture(self):
        with tempfile.TemporaryDirectory(prefix="ngen scene capture ") as directory:
            capture = Path(directory) / "post processed scene.bmp"
            result = run(capture)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(result.stderr, "", "unexpected native/validation diagnostics")
            self.assertIn("scene_hdr_width=1280 scene_hdr_height=720", result.stdout)
            self.assertIn("generated_shadow=1 shadow_extent=1024", result.stdout)
            self.assertIn("scene_capture=1", result.stdout)
            self.assertIn("frame_pin_retired=1", result.stdout)
            self.assertIn("resident_mip_readback=1", result.stdout)
            self.assertIn("scene_wsi_queued=1", result.stdout)
            data = capture.read_bytes()
            self.assertEqual(data[:2], b"BM")
            self.assertEqual(struct.unpack_from("<I", data, 2)[0], len(data))
            self.assertEqual(struct.unpack_from("<ii", data, 18), (1280, 720))
            self.assertEqual(struct.unpack_from("<H", data, 28)[0], 32)
            offset = struct.unpack_from("<I", data, 10)[0]
            pixels = data[offset:]
            self.assertEqual(len(pixels), 1280 * 720 * 4)
            lit = sum(any(pixels[i:i + 3]) for i in range(0, len(pixels), 4))
            self.assertGreater(lit, 0, "capture is completely black")
            self.assertLess(lit, 1280 * 720, "fixture background disappeared")

    def test_file_created_during_gpu_work_is_preserved(self):
        with tempfile.TemporaryDirectory(prefix="ngen concurrent capture ") as directory:
            capture = Path(directory) / "concurrent.bmp"
            with subprocess.Popen([str(PROBE), str(PACKAGE), str(capture)],
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True) as process:
                # Bound the streaming checkpoint as well as final completion.
                deadline = threading.Timer(90, process.kill)
                deadline.start()
                # Public readback checkpoint, before scene draw/pipeline preparation.
                try:
                    for line in process.stdout:
                        if "resident_verified_mip_bytes=" in line:
                            break
                    else:
                        self.fail("probe did not reach completed resident readback")
                    capture.write_bytes(b"concurrent owner bytes")
                    output, errors = process.communicate(timeout=90)
                    self.assertNotEqual(process.returncode, 0, output + errors)
                    self.assertIn("scene capture path exists", errors)
                    self.assertEqual(capture.read_bytes(), b"concurrent owner bytes")
                finally:
                    deadline.cancel()
                    if process.poll() is None:
                        process.kill()
                        process.communicate()

    def test_existing_outputs_directories_and_links_are_preserved(self):
        with tempfile.TemporaryDirectory(prefix="ngen preserved capture ") as directory:
            root = Path(directory)
            owned = root / "owned.bmp"
            owned.write_bytes(b"owner bytes")
            paths = [owned, root, root / "wrong-extension.png"]
            if os.name != "nt":
                link = root / "dangling.bmp"
                link.symlink_to(root / "missing.bmp")
                paths.append(link)
            for capture in paths:
                with self.subTest(path=capture.name):
                    result = run(capture)
                    self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                    self.assertIn("new .bmp path", result.stderr)
            self.assertEqual(owned.read_bytes(), b"owner bytes")
            self.assertFalse((root / "missing.bmp").exists())
            self.assertFalse((root / "wrong-extension.png").exists())


if __name__ == "__main__":
    unittest.main()
