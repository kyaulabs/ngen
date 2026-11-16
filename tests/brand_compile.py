"""Public build-time brand input/output contracts using only the standard library."""
import importlib.util
import json
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
            self.assertEqual(metadata["text"], "#EAF2FF")
            self.assertGreater(output.stat().st_size, 640 * 177 * 4)

    def test_component_materials_preserve_individual_alphas_and_text(self):
        with tempfile.TemporaryDirectory() as directory:
            metadata = brand.generate(ROOT / "brand", Path(directory) / "brand_data.hpp")
        self.assertEqual(metadata["materials"], {"panel": (10, 16, 29, 168),
            "panel-2": (16, 24, 42, 140), "line": (125, 155, 210, 36),
            "line-strong": (125, 155, 210, 77)})
        self.assertEqual(metadata["text"], "#EAF2FF")
        self.assertEqual(metadata["fonts"], {"sans_font": (ROOT / "brand/web/fonts/NeoSansPro-Regular.ttf").stat().st_size,
            "mono_font": (ROOT / "brand/web/fonts/OperatorMonoLigNerdFont-Regular.otf").stat().st_size})

    def test_unsupported_material_syntax_fails_before_artwork_is_read(self):
        document = json.loads((ROOT / "brand/web/ngen-tokens.json").read_text())
        for value in (None, "var(--panel)", "color-mix(in srgb, #0A101D 101%, transparent)", 66):
            with self.subTest(value=value), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / "web").mkdir()
                document["components"]["dark"]["panel"] = value
                (root / "web/ngen-tokens.json").write_text(json.dumps(document))
                with self.assertRaisesRegex(ValueError, "Invalid brand material"):
                    brand.generate(root, root / "out.hpp")

    def test_unbounded_linked_and_out_of_range_font_sources_fail_closed(self):
        malformed = [(b"bad", "Invalid brand font file"),
            (b"wOFF" + bytes(20), "Unsupported brand font container"),
            (bytes([0, 1, 0, 0]) + struct.pack(">HHHH", 1, 0, 0, 0) + b"head" + struct.pack(">III", 0, 500, 100),
             "Brand font table exceeds its input"),
            (bytes(4 * 1024**2 + 1), "Invalid brand font file")]
        for data, message in malformed:
            with self.subTest(size=len(data)), tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / "font.ttf"
                path.write_bytes(data)
                with self.assertRaisesRegex(ValueError, message):
                    brand.read_font(path)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "font.ttf"
            try:
                path.symlink_to(ROOT / "brand/web/fonts/NeoSansPro-Regular.ttf")
            except OSError:
                self.skipTest("Host cannot create a font-input link")
            with self.assertRaises(ValueError):
                brand.read_font(path)

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
