"""Assemble and verify an immutable scene using pinned offline CLI tools.

Inputs/tools are trusted local build inputs, not a filesystem or code sandbox.
The runtime package uses CRC32 for corruption detection, not authentication.
SHA256 source/tool/output identities are retained by this offline manifest.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib

LIMIT = 256 * 1024 * 1024
STRIDES = (56, 4, 80, 128, 8, 16, 32, 1, 72, 32, 72, 1, 1)
NATIVE_SECTIONS = {1, 2, 3, 4, 5, 6, 9, 10, 11, 12}


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def regular(path, limit):
    if path.is_symlink() or not path.is_file() or path.stat().st_size > limit:
        raise ValueError("expected a bounded nonsymlink build input: " + path.name)
    return path


def run(arguments, timeout=180):
    result = subprocess.run(list(map(str, arguments)), capture_output=True,
                            timeout=timeout, check=False)
    if len(result.stdout) > 1024 * 1024 or len(result.stderr) > 1024 * 1024:
        raise ValueError("offline tool diagnostic limit exceeded")
    if result.stderr:
        print(result.stderr.decode("utf-8", errors="replace"), file=sys.stderr, end="")
    if result.returncode != 0:
        if result.stdout:
            print(result.stdout.decode("utf-8", errors="replace"), file=sys.stderr, end="")
        raise ValueError("offline tool failed: " + Path(arguments[0]).name + " exit=" + str(result.returncode))
    return result.stdout


def integer(value, minimum, maximum):
    if type(value) is not int or not minimum <= value <= maximum:
        raise ValueError("native staging integer outside admitted range")
    return value


def stage_sections(native, report):
    entries = report["sections"]
    if not isinstance(entries, list) or len(entries) != len(NATIVE_SECTIONS):
        raise ValueError("native section catalog mismatch")
    parts = {}
    for entry in entries:
        kind = integer(entry["kind"], 1, 13)
        if kind not in NATIVE_SECTIONS or kind in parts:
            raise ValueError("unexpected or repeated native section")
        stride = integer(entry["stride"], 1, 128)
        count = integer(entry["count"], 0, LIMIT)
        filename = "section-" + str(kind) + ".bin"
        if stride != STRIDES[kind - 1] or entry["file"] != filename or count * stride > LIMIT:
            raise ValueError("native section layout mismatch")
        path = regular(native / filename, LIMIT)
        if path.stat().st_size != count * stride:
            raise ValueError("native section size mismatch")
        parts[kind] = path.read_bytes()
    return parts


def texture_sections(native, report, ktx, scratch):
    images = report["images"]
    if not isinstance(images, list) or len(images) > 256:
        raise ValueError("derived image count limit exceeded")
    records = bytearray()
    data = bytearray()
    admitted = 0
    for index, image in enumerate(images):
        width = integer(image["width"], 1, 8192)
        height = integer(image["height"], 1, 8192)
        image_format = integer(image["format"], 1, 2)
        role = integer(image["role"], 0, 4)
        if image_format != (2 if role in (0, 1, 4) else 1):
            raise ValueError("derived image role/format mismatch")
        files = image["files"]
        levels = max(width, height).bit_length()
        if not isinstance(files, list) or len(files) != levels:
            raise ValueError("derived image must contain a complete mip chain")
        paths = []
        for mip, name in enumerate(files):
            expected_name = "image-" + str(index) + "-mip-" + str(mip) + ".rgba"
            if name != expected_name:
                raise ValueError("unexpected raw mip filename")
            path = regular(native / name, LIMIT)
            expected = max(1, width >> mip) * max(1, height >> mip) * 4
            if path.stat().st_size != expected or expected > LIMIT - admitted:
                raise ValueError("derived mip size or aggregate budget mismatch")
            admitted += expected
            paths.append(path)
        output = scratch / ("image-" + str(index) + ".ktx2")
        transfer = "srgb" if image_format == 2 else "linear"
        vk_format = "R8G8B8A8_SRGB" if image_format == 2 else "R8G8B8A8_UNORM"
        diagnostics = run([ktx, "create", "--raw", "--format", vk_format,
                           "--width", str(width), "--height", str(height),
                           "--levels", str(levels), "--assign-tf", transfer,
                           "--assign-primaries", "bt709", "--assign-texcoord-origin", "top-left",
                           *paths, output], timeout=60)
        if diagnostics:
            print(diagnostics.decode("utf-8", errors="replace"), file=sys.stderr, end="")
        diagnostics = run([ktx, "validate", output], timeout=60)
        if diagnostics:
            print(diagnostics.decode("utf-8", errors="replace"), file=sys.stderr, end="")
        regular(output, LIMIT)
        padding = -len(data) % 8
        if padding + output.stat().st_size > LIMIT - len(data):
            raise ValueError("encoded image blob budget exceeded")
        data.extend(b"\0" * padding)
        records.extend(struct.pack("<QQ4I", len(data), output.stat().st_size,
                                   width, height, levels, image_format))
        data.extend(output.read_bytes())
    if admitted != report["texture_bytes"]:
        raise ValueError("native texture accounting mismatch")
    return {7: bytes(records), 8: bytes(data)}


def assemble(parts):
    if set(parts) != set(range(1, 14)):
        raise ValueError("incomplete runtime section set")
    output = bytearray(480)
    for kind, stride in enumerate(STRIDES, 1):
        data = parts[kind]
        if len(data) % stride:
            raise ValueError("runtime section stride mismatch")
        padding = -len(output) % 8
        if padding + len(data) > LIMIT - len(output):
            raise ValueError("runtime package byte budget exceeded")
        output.extend(b"\0" * padding)
        struct.pack_into("<4I2Q", output, 64 + (kind - 1) * 32,
                         kind, stride, len(data) // stride, 0, len(output), len(data))
        output.extend(data)
    checksum = zlib.crc32(memoryview(output)[64:]) & 0xffffffff
    struct.pack_into("<8sIIQII", output, 0, b"NGASSET\0", 1, 64, len(output), checksum, 13)
    return output


def cook(args):
    source = regular(args.source, LIMIT)
    for tool in (args.assetc, args.ktx, args.probe):
        regular(tool, LIMIT)
    output = args.output
    if output.is_symlink() or (output.exists() and (not output.is_dir() or any(output.iterdir()))):
        raise ValueError("package output must be a new or empty directory")
    output.parent.mkdir(parents=True, exist_ok=True)
    source_hash = digest(source)
    tools = {"assetc": digest(args.assetc), "ktx": digest(args.ktx), "runtime_probe": digest(args.probe),
             "assembler": digest(Path(__file__))}
    reported_version = run([args.ktx, "--version"], timeout=20).decode("utf-8").strip()
    provenance = {"schema": 1, "preset": "laptop", "default_lod": 2,
                  "maximum_texture_dimension": 2048, "source_sha256": source_hash,
                  "rights": "owner-provided development input; redistribution not approved",
                  "tool_sha256": tools, "ktx_reported_version": reported_version,
                  "lod_target_ratios": [1, 0.4, 0.1], "relative_combined_error_bound": 0.01,
                  "normal_attribute_weight": 0.5, "used_uv_seams": "protected",
                  "vertex_fetch_order": "coarsest-first"}
    if args.source_manifest:
        provenance["source_manifest_sha256"] = digest(regular(args.source_manifest, 1024 * 1024))
    with tempfile.TemporaryDirectory(prefix=".ngen-cook-", dir=output.parent) as directory:
        scratch = Path(directory)
        native = scratch / "native"
        run([args.assetc, "stage", "--preset", "laptop", source, native])
        stage_file = regular(native / "stage.json", 256 * 1024)
        report = json.loads(stage_file.read_text())
        if report["schema"] != 1 or report["preset"] != "laptop":
            raise ValueError("unsupported native staging schema/preset")
        parts = stage_sections(native, report)
        parts.update(texture_sections(native, report, args.ktx, scratch))
        parts[13] = json.dumps(provenance, sort_keys=True, separators=(",", ":")).encode("utf-8")
        if len(parts[13]) > 64 * 1024:
            raise ValueError("provenance byte limit exceeded")
        package = scratch / "package"
        package.mkdir()
        scene = package / "scene.ngas"
        scene.write_bytes(assemble(parts))
        runtime = json.loads(run([args.probe, scene], timeout=60).decode("utf-8"))
        if runtime["package_valid"] != 1 or runtime["lod_triangles"] != report["lod_triangles"] or runtime["texture_bytes"] != report["texture_bytes"]:
            raise ValueError("runtime verification disagrees with native cooking")
        if digest(source) != source_hash:
            raise ValueError("source changed during cooking")
        manifest = {**provenance, "package_sha256": digest(scene), "runtime": runtime,
                    "source_nodes": report["source_nodes"], "source_meshes": report["source_meshes"],
                    "source_vertices": report["source_vertices"],
                    "invalid_source_tangents": report["invalid_source_tangents"],
                    "rebuilt_corners": report["rebuilt_corners"]}
        (package / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
        if output.exists():
            if output.is_symlink() or not output.is_dir() or any(output.iterdir()):
                raise ValueError("package output changed before publication")
            output.rmdir()
        package.rename(output)
    print(json.dumps({"package": str(output / "scene.ngas"), "package_sha256": manifest["package_sha256"]}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assetc", type=Path, required=True)
    parser.add_argument("--ktx", type=Path, required=True)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--source-manifest", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        cook(args)
        return 0
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        print("asset cook: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
