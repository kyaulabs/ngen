"""Offline compilation of NGEN's finite Vulkan 1.3 shader set."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile


ENTRIES = (("cull", "cull", "compute", "cullMain"),
           ("pbr-vertex", "pbr", "vertex", "vertexMain"),
           ("pbr-fragment", "pbr", "fragment", "fragmentMain"),
           ("shadow-vertex", "pbr", "vertex", "shadowVertex"),
           ("shadow-fragment", "pbr", "fragment", "shadowFragment"),
           ("post-vertex", "post", "vertex", "postVertex"),
           ("post-fragment", "post", "fragment", "postFragment"))
MAX_ARTIFACT_BYTES = 4 * 1024 * 1024


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def run(arguments, environment):
    with tempfile.TemporaryFile() as log:
        result = subprocess.run(arguments, env=environment, stdout=log,
                                stderr=subprocess.STDOUT, timeout=30, check=False)
        if log.tell() > MAX_ARTIFACT_BYTES:
            raise ValueError("shader tool diagnostic limit exceeded")
        log.seek(0)
        diagnostics = log.read().decode("utf-8", errors="replace")
    if result.returncode:
        raise ValueError(f"{Path(arguments[0]).name} exited {result.returncode}: {diagnostics}")
    return diagnostics.strip()


def check_profile(module):
    """Admit capabilities used by this finite shader set, not all of Vulkan 1.3."""
    data = module.read_bytes()
    if len(data) < 20 or len(data) % 4 or struct.unpack_from("<II", data) != (0x07230203, 0x00010500):
        raise ValueError("invalid SPIR-V 1.5 module header")
    offset = 20
    while offset < len(data):
        instruction = struct.unpack_from("<I", data, offset)[0]
        size, opcode = (instruction >> 16) * 4, instruction & 0xffff
        if size < 4 or offset + size > len(data):
            raise ValueError("malformed SPIR-V instruction")
        if opcode == 17:  # OpCapability: Matrix and Shader need no optional device feature.
            if size != 8:
                raise ValueError("malformed SPIR-V capability")
            capability = struct.unpack_from("<I", data, offset + 4)[0]
            if capability not in (0, 1):
                raise ValueError(f"unsupported SPIR-V capability {capability}")
        if opcode == 10:  # Promoted in SPIR-V 1.3; no optional Vulkan extension needed.
            extension = data[offset + 4:offset + size].rstrip(b"\0").decode("ascii")
            if extension != "SPV_KHR_storage_buffer_storage_class":
                raise ValueError(f"unsupported SPIR-V extension {extension}")
        offset += size


def compile_package(slang_root, validator, source_dir, output, verify_existing=False):
    slang_root = slang_root.resolve(strict=True)
    compiler = slang_root / "bin" / ("slangc.exe" if os.name == "nt" else "slangc")
    validator = validator.resolve(strict=True)
    if not compiler.is_file() or not validator.is_file():
        raise ValueError("compiler and validator must be executable files")
    if output.is_symlink() or (output.exists() and not output.is_dir()):
        raise ValueError("output must be absent or an empty directory")
    existing = output.exists() and any(output.iterdir())
    if existing and not verify_existing:
        raise ValueError("output must be absent or an empty directory")
    # Isolate bundled compiler libraries and omit credentials/unrelated tool settings.
    environment = {name: os.environ[name] for name in ("SystemRoot", "WINDIR", "TEMP", "TMP") if name in os.environ}
    environment["PATH"] = str(slang_root / "bin") + os.pathsep + os.defpath
    environment["LD_LIBRARY_PATH"] = str(slang_root / "lib")
    version = run([str(compiler), "-version"], environment)
    manifest = {"schema": 1, "target": "vulkan1.3", "spirv": "1.5", "matrix_layout": "column-major",
                "compiler": {"version": version, "sha256": digest(compiler)},
                "validator": {"version": run([str(validator), "--version"], environment), "sha256": digest(validator)},
                "libraries": [], "shaders": []}
    for directory in (slang_root / "bin", slang_root / "lib"):
        if directory.is_dir():
            for library in sorted(directory.rglob("*")):
                if library.is_file() and (".so" in library.name or library.suffix in (".dll", ".slang-module", ".slang")):
                    if not library.resolve().is_relative_to(slang_root):
                        raise ValueError("compiler library escapes package root")
                    manifest["libraries"].append({"file": library.relative_to(slang_root).as_posix(), "sha256": digest(library)})
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".ngen-shaders-", dir=output.parent) as directory:
        temporary = Path(directory) / "package"
        temporary.mkdir()
        for name, source_name, stage, entry in ENTRIES:
            source = (source_dir / (source_name + ".slang")).resolve(strict=True)
            if not (source.is_file() and 0 < source.stat().st_size <= MAX_ARTIFACT_BYTES):
                raise ValueError("shader source size outside admitted range")
            module = temporary / (name + ".spv")
            reflection = temporary / (name + ".reflection.json")
            run([str(compiler), str(source), "-entry", entry, "-stage", stage,
                 "-target", "spirv", "-profile", "spirv_1_5", "-matrix-layout-column-major",
                 "-fvk-use-gl-layout", "-restrictive-capability-check", "-warnings-as-errors", "all", "-emit-spirv-directly",
                 "-reflection-json", str(reflection), "-o", str(module)], environment)
            if not (0 < module.stat().st_size <= MAX_ARTIFACT_BYTES and
                    0 < reflection.stat().st_size <= MAX_ARTIFACT_BYTES):
                raise ValueError("shader artifact size outside admitted range")
            run([str(validator), "--target-env", "vulkan1.3", str(module)], environment)
            check_profile(module)
            json.loads(reflection.read_text(encoding="utf-8"))
            manifest["shaders"].append({"name": name, "stage": stage, "entry": entry,
                                        "source_sha256": digest(source), "sha256": digest(module),
                                        "reflection_sha256": digest(reflection)})
        header = ["#pragma once", "#include <array>", "#include <cstdint>", "namespace ngen::render::compiled {"]
        for shader in manifest["shaders"]:
            data = (temporary / (shader["name"] + ".spv")).read_bytes()
            words = struct.unpack("<" + "I" * (len(data) // 4), data)
            header.append("inline constexpr std::array<std::uint32_t, " + str(len(words)) + "> " + shader["name"].replace("-", "_") + " = {")
            for offset in range(0, len(words), 8):
                header.append("    " + ", ".join(f"0x{word:08x}" for word in words[offset:offset + 8]) + ",")
            header.append("};")
        header.append("} // namespace ngen::render::compiled")
        runtime_header = temporary / "ngen_shader_code.hpp"
        runtime_header.write_text("\n".join(header) + "\n", encoding="utf-8")
        if runtime_header.stat().st_size > MAX_ARTIFACT_BYTES:
            raise ValueError("embedded shader header size exceeds admitted range")
        manifest["runtime_header_sha256"] = digest(runtime_header)
        (temporary / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        if existing:
            if output.is_symlink() or not output.is_dir():
                raise ValueError("existing shader package changed during compilation")
            expected = {path.name: path for path in temporary.iterdir()}
            files = {}
            for path in output.iterdir():
                if (path.name not in expected or path.is_symlink() or not path.is_file() or
                        path.stat().st_nlink != 1 or path.stat().st_size > MAX_ARTIFACT_BYTES):
                    raise ValueError("existing shader package is not the exact canonical output")
                files[path.name] = path
            if files.keys() != expected.keys() or any(
                    path.read_bytes() != expected[name].read_bytes() for name, path in files.items()):
                raise ValueError("existing shader package differs from freshly compiled output")
            return  # Never rewrite a verified immutable package, including its timestamps.
        if output.exists():
            output.rmdir()  # Ninja pre-creates output parents; remove only that still-empty directory.
        temporary.rename(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--slang-root", type=Path, required=True)
    parser.add_argument("--validator", type=Path, required=True)
    parser.add_argument("--source-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--verify-existing", action="store_true", help="recompile and verify an existing identical immutable package")
    args = parser.parse_args()
    try:
        compile_package(args.slang_root, args.validator, args.source_dir, args.output, args.verify_existing)
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f"shader compilation: {error}\n")


if __name__ == "__main__":
    main()
