"""Independent allowed-byte changes and instanced budget check for the accepted asset."""
from pathlib import Path
import hashlib
import json
import math
import struct
import sys


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def sections(path):
    with path.open("rb") as stream:
        header = stream.read(480)
        parts = {}
        for index in range(13):
            kind, stride, count, reserved, offset, length = struct.unpack_from(
                "<4I2Q", header, 64 + index * 32)
            assert kind == index + 1 and reserved == 0 and stride * count == length
            stream.seek(offset)
            parts[kind] = stream.read(length)
        return parts


def main():
    base, approved, source = map(Path, sys.argv[1:4])
    here = Path(__file__).resolve().parent
    recipe = json.loads((here / "palette.json").read_text())
    manifest = json.loads((here / "approved-manifest.json").read_text())
    assert digest(base) == recipe["base_package_sha256"]
    assert digest(source) == recipe["original_source_sha256"]
    assert digest(approved) == manifest["package_sha256"]
    old, new = sections(base), sections(approved)
    for kind in (1, 2, 3, 5, 6, 7, 8, 9, 12):
        assert new[kind][:len(old[kind])] == old[kind], f"original section {kind} prefix changed"
    assert new[11] == old[11], "light instances/transforms changed"
    materials = []
    for offset in range(0, len(old[4]), 128):
        start, length = struct.unpack_from("<2I", old[4], offset + 100)
        name = old[12][start:start + length].decode()
        original, changed = old[4][offset:offset + 128], new[4][offset:offset + 128]
        if name in recipe["materials"]:
            assert original[28:] == changed[28:], "authored strength/maps/flags changed"
            assert struct.unpack_from("<4f", changed) == struct.unpack("<4f", struct.pack(
                "<4f", *recipe["materials"][name]["base_color"]))
            assert struct.unpack_from("<3f", changed, 16) == struct.unpack("<3f", struct.pack(
                "<3f", *recipe["materials"][name]["emissive"]))
            materials.append(name)
        else:
            assert original == changed, f"unrelated material changed: {name}"
    assert len(materials) == 2
    with source.open("rb") as stream:
        stream.read(12)
        length, kind = struct.unpack("<2I", stream.read(8))
        assert kind == 0x4e4f534a and length < 8 * 1024 * 1024
        gltf = json.loads(stream.read(length))
    lights = gltf["extensions"]["KHR_lights_punctual"]["lights"]
    changed_lights = []
    groups = {"luminous_collar": 0, "inner_coil": 0, "core": 0}
    for index, light in enumerate(lights):
        start = index * 32
        original, changed = old[10][start:start + 32], new[10][start:start + 32]
        group = next((g for g in groups if "_" + g in light.get("name", "")), None)
        if group is None:
            assert original == changed, "bridge/navigation/other light changed"
            continue
        groups[group] += 1
        assert original[:4] == changed[:4] and original[24:] == changed[24:]
        setting = recipe["light_groups"][group]
        original_power = struct.unpack_from("<f", original, 16)[0]
        assert changed[16:24] == struct.pack("<2f", original_power *
                                            setting["intensity_scale"], setting["range"])
        color = recipe["core_light_color" if group == "core" else "drive_light_color"]
        assert changed[4:16] == struct.pack("<3f", *color)
        assert struct.unpack_from("<f", changed, 16)[0] > 0
        if group == "core":
            assert original[16:20] == changed[16:20]
        changed_lights.append(index)
    assert groups == {"luminous_collar": 48, "inner_coil": 6, "core": 6}
    assert len(new[10]) == len(old[10]) and len(changed_lights) == 60
    instance_triangles, added_triangles = 0, 0
    low, high = [math.inf] * 3, [-math.inf] * 3
    for offset in range(0, len(new[9]), 72):
        primitive, node, *matrix = struct.unpack_from("<2I16f", new[9], offset)
        at = primitive * 80
        lod = min(2, struct.unpack_from("<I", new[3], at + 12)[0] - 1)
        triangles = struct.unpack_from("<I", new[3], at + 44 + lod * 12)[0] // 3
        instance_triangles += triangles
        if offset >= len(old[9]):
            assert primitive == 188 and 654 <= node <= 659
            added_triangles += triangles
        bounds = struct.unpack_from("<6f", new[3], at + 16)
        for bits in range(8):
            point = [bounds[j + (3 if bits & (1 << j) else 0)] for j in range(3)]
            for row in range(3):
                value = matrix[12 + row] + sum(matrix[col * 4 + row] * point[col]
                                              for col in range(3))
                low[row], high[row] = min(low[row], value), max(high[row], value)
    assert instance_triangles == 238272 and added_triangles == 192
    assert instance_triangles + 100 * 80 == 246272 <= 250000
    assert manifest["runtime"]["texture_bytes"] == 89522172 < 256 * 1024 ** 2
    center = [(a + b) / 2 for a, b in zip(low, high)]
    radius = math.sqrt(sum(((b - a) / 2) ** 2 for a, b in zip(low, high)))
    candidates = []
    for offset in range(0, len(new[11]), 72):
        light, node, *matrix = struct.unpack_from("<2I16f", new[11], offset)
        power, reach = struct.unpack_from("<2f", new[10], light * 32 + 16)
        distance2 = sum((matrix[12 + axis] - center[axis]) ** 2 for axis in range(3))
        if reach > 0 and math.sqrt(distance2) > reach + radius:
            continue
        if power > 0:
            candidates.append((power / max(1, distance2), offset // 72, light))
    selected = sorted(candidates, key=lambda value: (-value[0], value[1]))[:16]
    core_names = [lights[light]["name"] for score, instance, light in selected
                  if "_core" in lights[light].get("name", "")]
    assert len(selected) == 16 and len(core_names) == 6
    result = {"approved_package_sha256": digest(approved), "original_prefixes_unchanged": True,
              "changed_source_materials": materials, "changed_light_indices": changed_lights,
              "light_groups": groups, "source_light_instances_unchanged": True,
              "selection_rank_mirror": {"scope": "independent mirror, not GPU readback",
                                        "selected_point_instances": 16,
                                        "selected_core_names": core_names},
              "added_instances": 6, "added_vertices": (len(new[1]) - len(old[1])) // 56,
              "added_shared_triangles": 32, "added_visible_triangles": added_triangles,
              "visible_ship_triangles": instance_triangles, "visible_total_triangles": 246272,
              "world_bounds": {"minimum": low, "maximum": high},
              "original_source_and_package_unchanged":
                  digest(source) == recipe["original_source_sha256"] and
                  digest(base) == recipe["base_package_sha256"]}
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
