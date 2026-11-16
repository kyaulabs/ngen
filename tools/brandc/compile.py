"""Build-time NGEN brand artwork; no runtime decoder or new dependency."""
from pathlib import Path
import argparse
import json
import re
import struct
import zlib


def read_png(path):
    path = Path(path)
    if path.is_symlink() or not path.is_file() or path.stat().st_size > 2 * 1024**2:
        raise ValueError("Invalid brand image file")
    data = path.read_bytes()
    if not data.startswith(b"\x89PNG\r\n\x1a\n"):
        raise ValueError("Invalid brand PNG signature")
    offset, width, height, channels, packed = 8, 0, 0, 0, bytearray()
    ended = False
    while offset + 12 <= len(data):
        length = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4:offset + 8]
        if length > len(data) - offset - 12:
            raise ValueError("Truncated brand PNG")
        payload = data[offset + 8:offset + 8 + length]
        crc = struct.unpack_from(">I", data, offset + 8 + length)[0]
        if zlib.crc32(kind + payload) != crc:
            raise ValueError("Brand PNG CRC mismatch")
        if kind == b"IHDR":
            if offset != 8 or length != 13:
                raise ValueError("Invalid brand PNG header")
            width, height, depth, colour, comp, filt, interlace = struct.unpack(">IIBBBBB", payload)
            if not (0 < width <= 1024 and 0 < height <= 1024 and colour in (2, 6) and
                    (depth, comp, filt, interlace) == (8, 0, 0, 0)):
                raise ValueError("Brand PNG must be bounded noninterlaced RGB/RGBA8")
            channels = 3 if colour == 2 else 4
        elif kind == b"IDAT":
            packed.extend(payload)
        elif kind == b"IEND":
            if length:
                raise ValueError("Invalid brand PNG end")
            ended = True
        elif kind[:1].isupper():
            raise ValueError("Unsupported critical brand PNG chunk")
        offset += length + 12
        if ended:
            break
    if not ended or offset != len(data) or not width or not packed:
        raise ValueError("Incomplete brand PNG")
    stride = width * channels
    expected = (stride + 1) * height
    decoder = zlib.decompressobj()
    raw = decoder.decompress(packed, expected + 1)
    if len(raw) != expected or not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
        raise ValueError("Invalid bounded brand PNG data")
    pixels = bytearray()
    previous = bytes(stride)
    for row in range(height):
        start = row * (stride + 1)
        kind = raw[start]
        if kind > 4:
            raise ValueError("Unsupported brand PNG filter")
        current = bytearray(raw[start + 1:start + 1 + stride])
        for i in range(stride):
            left = current[i - channels] if i >= channels else 0
            up = previous[i]
            corner = previous[i - channels] if i >= channels else 0
            predictor = left + up - corner
            distances = (abs(predictor - left), abs(predictor - up), abs(predictor - corner))
            paeth = (left, up, corner)[distances.index(min(distances))]
            current[i] = (current[i] + (0, left, up, (left + up) // 2, paeth)[kind]) & 255
        pixels.extend(current)
        previous = current
    if channels == 3:
        pixels = bytearray(v for i in range(0, len(pixels), 3) for v in (*pixels[i:i + 3], 255))
    return width, height, bytes(pixels)


def read_font(path):
    path = Path(path)
    if path.is_symlink() or not path.is_file() or not 12 <= path.stat().st_size <= 4 * 1024**2:
        raise ValueError("Invalid brand font file")
    data = path.read_bytes()
    if data[:4] not in (b"\x00\x01\x00\x00", b"OTTO"):
        raise ValueError("Unsupported brand font container")
    count = struct.unpack_from(">H", data, 4)[0]
    if not 1 <= count <= 256 or 12 + count * 16 > len(data):
        raise ValueError("Invalid brand font directory")
    for entry in range(count):
        offset, length = struct.unpack_from(">II", data, 12 + entry * 16 + 8)
        if offset > len(data) or length > len(data) - offset:
            raise ValueError("Brand font table exceeds its input")
    return data


def generate(root, output):
    root, output = Path(root), Path(output)
    paths = {"wordmark": root / "logos/png/ngen-wordmark-dark-640.png",
             "mark": root / "icons/png/ngen-mark-dark-64.png",
             "icon": root / "icons/app/ngen-app-dark-64.png"}
    fonts = {"sans_font": root / "web/fonts/NeoSansPro-Regular.ttf",
             "mono_font": root / "web/fonts/OperatorMonoLigNerdFont-Regular.otf"}
    tokens = root / "web/ngen-tokens.json"
    for path in [*paths.values(), *fonts.values(), tokens]:
        if any(p.is_symlink() for p in [path, *path.parents]):
            raise ValueError("Brand inputs must not use links")
    if not tokens.is_file() or tokens.stat().st_size > 65536:
        raise ValueError("Invalid brand tokens")
    document = json.loads(tokens.read_text(encoding="utf-8"))
    palette = document["themes"]["dark"]
    if not isinstance(palette, dict) or len(palette) > 32:
        raise ValueError("Invalid dark brand palette")
    lines = ["#pragma once", "#include <array>", "#include <cstdint>",
             "namespace ngen::tools::brand {"]
    for key, value in sorted(palette.items()):
        if not re.fullmatch(r"[a-z][a-z-]{0,31}", key) or not re.fullmatch(r"#[0-9A-Fa-f]{6}", value):
            raise ValueError("Invalid brand colour token")
        red, green, blue = (int(value[i:i + 2], 16) for i in (1, 3, 5))
        lines.append(f"inline constexpr std::uint32_t {key.replace('-', '_')} = 0x{(255 << 24) | (blue << 16) | (green << 8) | red:08x}U;")
    metadata = {"text": palette["text"], "materials": {}}
    materials = document["components"]["dark"]
    if not isinstance(materials, dict) or len(materials) > 8:
        raise ValueError("Invalid dark brand materials")
    for name in ("panel", "panel-2", "line", "line-strong"):
        value = materials.get(name)
        match = re.fullmatch(r"color-mix\(in srgb, (#[0-9A-Fa-f]{6}) (100|[0-9]{1,2})%, transparent\)", value) if isinstance(value, str) else None
        if not match:
            raise ValueError("Invalid brand material: " + name)
        colour, percentage = match.groups()
        red, green, blue = (int(colour[i:i + 2], 16) for i in (1, 3, 5))
        alpha = (int(percentage) * 255 + 50) // 100
        packed = (alpha << 24) | (blue << 16) | (green << 8) | red
        lines.append(f"inline constexpr std::uint32_t material_{name.replace('-', '_')} = 0x{packed:08x}U;")
        metadata["materials"][name] = (red, green, blue, alpha)
    for name, path in paths.items():
        width, height, pixels = read_png(path)
        metadata[name] = (width, height)
        lines += [f"inline constexpr unsigned {name}_width = {width};",
                  f"inline constexpr unsigned {name}_height = {height};",
                  f"inline constexpr std::array<unsigned char, {len(pixels)}> {name}_pixels{{{{"]
        lines += [",".join(str(v) for v in pixels[i:i + 32]) + "," for i in range(0, len(pixels), 32)]
        lines.append("}};")
    metadata["fonts"] = {}
    for name, path in fonts.items():
        data = read_font(path)
        metadata["fonts"][name] = len(data)
        lines.append(f"inline constexpr std::array<unsigned char, {len(data)}> {name}{{{{")
        lines += [",".join(str(v) for v in data[i:i + 32]) + "," for i in range(0, len(data), 32)]
        lines.append("}};")
    lines += ["} // namespace ngen::tools::brand", ""]
    content = "\n".join(lines)
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.is_symlink() or output.suffix != ".hpp":
        raise ValueError("Invalid generated brand header path")
    if not output.exists() or output.read_text() != content:
        output.write_text(content, encoding="utf-8")
    return metadata


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--brand", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    generate(args.brand, args.output)
