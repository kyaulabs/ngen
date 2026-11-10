"""Replay the accepted private frigate presentation with pinned offline inputs.

This is an asset-specific reproduction receipt, not a general material editor or
an untrusted-input sandbox. It does not need the private reference photograph.
The exact original authoring scripts/provenance remain separate receipts.
"""
import argparse
import importlib.util
import json
import math
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[4]
SPEC = importlib.util.spec_from_file_location("ngen_cook", ROOT / "tools/assetc/cook.py")
cook = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(cook)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def load_sections(path):
    raw = path.read_bytes()  # Hash-pinned, native-probe-admitted local package.
    result = {}
    for index in range(13):
        kind, stride, count, reserved, offset, length = struct.unpack_from(
            "<4I2Q", raw, 64 + index * 32)
        require(kind == index + 1 and stride == cook.STRIDES[index] and
                reserved == 0 and stride * count == length,
                "unexpected pinned section directory")
        result[kind] = raw[offset:offset + length]
    return result


def palette_sections(parts, recipe, gltf):
    material_bytes = bytearray(parts[4])
    changed = set()
    for offset in range(0, len(material_bytes), 128):
        start, length = struct.unpack_from("<2I", material_bytes, offset + 100)
        name = parts[12][start:start + length].decode("utf-8")
        if name in recipe["materials"]:
            values = recipe["materials"][name]
            struct.pack_into("<4f", material_bytes, offset, *values["base_color"])
            struct.pack_into("<3f", material_bytes, offset + 16, *values["emissive"])
            changed.add(name)
    require(changed == set(recipe["materials"]), "pinned drive materials differ")
    lights = gltf["extensions"]["KHR_lights_punctual"]["lights"]
    require(len(lights) == len(parts[10]) // 32 == 612, "pinned lights differ")
    light_bytes = bytearray(parts[10])
    groups = {"luminous_collar": 0, "inner_coil": 0, "core": 0}
    for index, light in enumerate(lights):
        offset = index * 32
        require(struct.pack("<3f", *light.get("color", [1, 1, 1])) ==
                parts[10][offset + 4:offset + 16], "source light order/color differs")
        group = next((key for key in groups if "_" + key in light.get("name", "")), None)
        if group is None:
            continue
        groups[group] += 1
        color = recipe["core_light_color" if group == "core" else "drive_light_color"]
        setting = recipe["light_groups"][group]
        intensity = struct.unpack_from("<f", light_bytes, offset + 16)[0]
        struct.pack_into("<3f", light_bytes, offset + 4, *color)
        struct.pack_into("<2f", light_bytes, offset + 16,
                         intensity * setting["intensity_scale"], setting["range"])
    require(groups == {"luminous_collar": 48, "inner_coil": 6, "core": 6},
            "pinned drive light groups differ")
    parts[4], parts[10] = bytes(material_bytes), bytes(light_bytes)
    return lights


def sprite_levels():
    def smooth(a, b, value):
        t = max(0.0, min(1.0, (value - a) / (b - a)))
        return t * t * (3 - 2 * t)
    linear = []
    for y in range(64):
        row = []
        v = 2 * y / 63 - 1
        for x in range(128):
            u = x / 127
            hot = math.exp(-((u - .08) / .0525) ** 2) * (1 - smooth(.75, 1, abs(v)))
            alpha = (.25 * smooth(0, .03, u) * (1 - smooth(.45, 1, u)) *
                     math.exp(-1.6 * max(0, u - .08)) * (1 - smooth(.65, 1, abs(v))))
            row.append((.015 + .985 * hot, .22 + .78 * hot, 1.0, alpha))
        linear.append(row)
    while True:
        data = bytearray()
        for row in linear:
            for pixel in row:
                for channel in pixel[:3]:
                    value = (12.92 * channel if channel <= .0031308 else
                             1.055 * channel ** (1 / 2.4) - .055)
                    data.append(round(max(0, min(1, value)) * 255))
                data.append(round(max(0, min(1, pixel[3])) * 255))
        yield data
        height, width = len(linear), len(linear[0])
        if width == height == 1:
            break
        resized = []
        for y in range(max(1, height // 2)):
            row = []
            for x in range(max(1, width // 2)):
                samples = [linear[yy][xx]
                           for yy in range(y * 2, min(height, y * 2 + 2))
                           for xx in range(x * 2, min(width, x * 2 + 2))]
                alpha = sum(pixel[3] for pixel in samples)
                color = [sum(pixel[c] * pixel[3] for pixel in samples) / alpha
                         if alpha > 0 else 0 for c in range(3)]
                row.append((*color, alpha / len(samples)))
            resized.append(row)
        linear = resized


def plume_sections(parts, lights, node_base, ktx, scratch):
    counts = {kind: len(data) // cook.STRIDES[kind - 1] for kind, data in parts.items()}
    native = scratch / "native"
    native.mkdir()
    names, texture_bytes = [], 0
    for index, data in enumerate(sprite_levels()):
        name = f"image-0-mip-{index}.rgba"
        (native / name).write_bytes(data)
        names.append(name)
        texture_bytes += len(data)
    require(len(names) == 8 and texture_bytes == 43692, "sprite mip accounting differs")
    report = {"images": [{"width": 128, "height": 64, "format": 2, "role": 4,
                           "files": names}], "texture_bytes": texture_bytes}
    encoded = cook.texture_sections(native, report, ktx, scratch)
    blob = parts[8] + b"\0" * (-len(parts[8]) % 8)
    record = bytearray(encoded[7])
    struct.pack_into("<Q", record, 0, len(blob))
    parts[7] += record
    parts[8] = blob + encoded[8]
    parts[6] += struct.pack("<4I", 9987, 9729, 33071, 33071)
    parts[5] += struct.pack("<2I", counts[7], counts[6])
    material = bytearray(128)
    struct.pack_into("<4f", material, 0, 0, 0, 0, 1)
    struct.pack_into("<3f", material, 16, 1, 1, 1)
    struct.pack_into("<6f", material, 28, 3, 1, 1, 1, 1, .5)
    struct.pack_into("<2I", material, 52, 2, 1)
    for index in range(5):
        struct.pack_into("<2I", material, 60 + index * 8,
                         counts[5] if index in (0, 4) else 0xffffffff, 0)
    name = b"M2_BlueWhiteDrivePlume"
    struct.pack_into("<2I", material, 100, len(parts[12]), len(name))
    parts[12] += name
    parts[4] += material
    vertices, indices = [], []

    def vertex(position, normal, uv, tangent):
        vertices.append(struct.pack("<14f", *position, *normal, *uv, *uv, *tangent))

    vertex((.065, 0, 0), (1, 0, 0), (.08, .5), (0, 1, 0, 1))
    for index in range(16):
        angle = math.tau * index / 16
        vertex((.065, math.cos(angle), math.sin(angle)), (1, 0, 0),
               (.08, 1), (0, 1, 0, 1))
    for index in range(16):
        indices.extend((0, 1 + index, 1 + (index + 1) % 16))
    for index in range(8):
        angle = math.pi * index / 8
        cy, sz = math.cos(angle), math.sin(angle)
        positions = [(0, -cy, -sz), (1, -cy, -sz), (1, cy, sz), (0, cy, sz)]
        first = len(vertices)
        for position, uv in zip(positions, [(0, 0), (1, 0), (1, 1), (0, 1)]):
            vertex(position, (0, -sz, cy), uv, (1, 0, 0, 1))
        indices.extend(first + i for i in (0, 1, 2, 0, 2, 3))
    require(len(vertices) == 49 and len(indices) == 96, "plume geometry accounting differs")
    primitive = bytearray(80)
    struct.pack_into("<4I6f", primitive, 0, counts[1], len(vertices), counts[4], 1,
                     0, -1, -1, 1, 1, 1)
    struct.pack_into("<2If", primitive, 40, counts[2], len(indices), 0)
    parts[1] += b"".join(vertices)
    parts[2] += struct.pack("<96I", *indices)
    parts[3] += primitive
    cores = []
    for offset in range(0, len(parts[11]), 72):
        light, node, *matrix = struct.unpack_from("<2I16f", parts[11], offset)
        if "_core" in lights[light].get("name", ""):
            cores.append((lights[light]["name"], matrix))
    require(len(cores) == 6 and node_base == 654, "drive positions/node catalog differ")
    for index, (name, matrix) in enumerate(cores):
        radius = 6.1 if "Dorsal" in name or "Ventral" in name else 7.2
        model = [radius * 3.6, 0, 0, 0,
                 0, radius * .8 * .981468915939331, 0, 0,
                 0, 0, radius * .8 * .9505835771560669, 0,
                 159.5, matrix[13], matrix[14], 1]
        parts[9] += struct.pack("<2I16f", counts[3], node_base + index, *model)


def reproduce(args):
    require(not args.output.exists() and not args.output.is_symlink(),
            "output must be absent; existing owner paths are never replaced")
    recipe = json.loads(cook.regular(HERE / "palette.json", 16384).read_text())
    manifest = json.loads(cook.regular(HERE / "approved-manifest.json", 131072).read_text())
    for path, expected in [(args.base, recipe["base_package_sha256"]),
                           (args.source, recipe["original_source_sha256"]),
                           (args.ktx, manifest["exhaust_plume"]["ktx_sha256"])]:
        require(cook.digest(cook.regular(path, cook.LIMIT)) == expected,
                "pinned input identity differs: " + path.name)
    cook.regular(args.probe, cook.LIMIT)
    cook.run([args.probe, args.base], timeout=60)
    parts = load_sections(args.base)
    with args.source.open("rb") as stream:
        stream.read(12)
        length, kind = struct.unpack("<2I", stream.read(8))
        require(kind == 0x4e4f534a and length <= 8 * 1024 * 1024, "GLB JSON differs")
        gltf = json.loads(stream.read(length))
    lights = palette_sections(parts, recipe, gltf)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".ngen-exhaust-", dir=args.output.parent) as directory:
        scratch = Path(directory)
        plume_sections(parts, lights, manifest["source_nodes"], args.ktx, scratch)
        # Snapshot of the accepted creators' exact provenance, not a claim that
        # this replay script is one of those original authoring scripts.
        parts[13] = cook.regular(HERE / "provenance.json", 65537).read_bytes().removesuffix(b"\n")
        staged = scratch / "package"
        staged.mkdir()
        scene = staged / "scene.ngas"
        scene.write_bytes(cook.assemble(parts))
        require(cook.digest(scene) == manifest["package_sha256"],
                "replay did not reproduce the exact accepted package")
        runtime = json.loads(cook.run([args.probe, scene], timeout=60))
        require(runtime == manifest["runtime"], "native admission/statistics differ")
        require(cook.digest(args.base) == recipe["base_package_sha256"] and
                cook.digest(args.source) == recipe["original_source_sha256"],
                "original source/package changed during replay")
        (staged / "manifest.json").write_bytes((HERE / "approved-manifest.json").read_bytes())
        require(not args.output.exists() and not args.output.is_symlink(), "output appeared during replay")
        staged.rename(args.output)
    print(json.dumps({"package_sha256": manifest["package_sha256"],
                      "reproducer_sha256": cook.digest(Path(__file__)),
                      "runtime": runtime}, sort_keys=True))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("base", "source", "ktx", "probe", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    try:
        reproduce(parser.parse_args())
    except (OSError, ValueError, KeyError, TypeError, struct.error,
            RuntimeError, subprocess.SubprocessError) as error:
        print("exhaust replay: " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
