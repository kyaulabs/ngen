"""Public build-time brand input/output contracts using only the standard library."""
import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True  # Tests must not dirty corresponding source inputs.
spec = importlib.util.spec_from_file_location("brand_compile", ROOT / "tools/brandc/compile.py")
brand = importlib.util.module_from_spec(spec)
spec.loader.exec_module(brand)


def png(raw, width=2, height=2, colour=6):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, colour, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


class Brand(unittest.TestCase):
    def test_rgba_artwork_preserves_transparent_and_coloured_pixels(self):
        rgba = bytes([99, 221, 248, 255, 182, 165, 255, 128,
                      221, 243, 248, 255, 0, 0, 0, 0])
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "mark.png"
            path.write_bytes(png(b"\0" + rgba[:8] + b"\0" + rgba[8:]))
            width, height, pixels = brand.read_png(path)
        self.assertEqual((width, height), (2, 2))
        self.assertEqual(pixels, rgba)

    def test_opaque_app_tile_gains_alpha_without_changing_its_rgb(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "tile.png"
            path.write_bytes(png(b"\0\x0e\x22\x2c\x63\xdd\xf8", height=1, colour=2))
            width, height, pixels = brand.read_png(path)
        self.assertEqual((width, height), (2, 1))
        self.assertEqual(pixels, b"\x0e\x22\x2c\xff\x63\xdd\xf8\xff")

    def test_build_emits_the_supplied_dark_identity_without_runtime_files(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "brand_data.hpp"
            metadata = brand.generate(ROOT / "brand", output)
            self.assertTrue(output.is_file(), "The build must emit its self-contained artwork")
            self.assertEqual(metadata["wordmark"], (640, 177))
            self.assertEqual(metadata["icon"], (64, 64))
            self.assertEqual(metadata["text"], "#DDF3F8")
            self.assertGreater(output.stat().st_size, 640 * 177 * 4)

    def test_corrupt_truncated_and_expanding_inputs_fail_closed(self):
        valid = png(b"\0" + bytes(8) + b"\0" + bytes(8))
        corrupt = bytearray(valid)
        corrupt[29] ^= 1
        for data in (valid[:-1], bytes(corrupt), valid + b"trailing",
                     png(b"\0" + bytes(8), width=2048), png(bytes(100000))):
            with self.subTest(size=len(data)), tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / "bad.png"
                path.write_bytes(data)
                with self.assertRaises((ValueError, zlib.error)):
                    brand.read_png(path)

    def test_all_png_row_filters_preserve_the_same_artwork(self):
        row = bytes([10, 20, 30, 255, 40, 50, 60, 128])
        filtered = (b"\0" + row + b"\1" + bytes([10, 20, 30, 255, 30, 30, 30, 129])
                    + b"\2" + bytes(8) + b"\3" + bytes([5, 10, 15, 128, 15, 15, 15, 193])
                    + b"\4" + bytes(8))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "filtered.png"
            path.write_bytes(png(filtered, height=5))
            width, height, pixels = brand.read_png(path)
        self.assertEqual((width, height), (2, 5))
        self.assertEqual(pixels, row * 5)


if __name__ == "__main__":
    unittest.main()
