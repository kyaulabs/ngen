"""Exercise the offline shader compiler through its public process interface."""
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SLANG = Path(os.environ["NGEN_SLANG_ROOT"]).resolve()
VALIDATOR = Path(os.environ.get("NGEN_SPIRV_VAL") or shutil.which("spirv-val") or "").resolve()


def compile_shaders(source, output, verify_existing=False):
    return subprocess.run(
        [sys.executable, str(ROOT / "tools/shaderc/compile.py"),
         "--slang-root", str(SLANG), "--validator", str(VALIDATOR),
         "--source-dir", str(source), "--output", str(output)] + (["--verify-existing"] if verify_existing else []),
        capture_output=True, text=True, timeout=120, check=False,
    )


class ShaderCompilation(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        directory = tempfile.TemporaryDirectory(prefix="ngen shader contract ")
        cls.addClassCleanup(directory.cleanup)
        cls.output = Path(os.environ.get("NGEN_SHADER_PACKAGE", Path(directory.name) / "cooked shaders"))
        if not os.environ.get("NGEN_SHADER_PACKAGE"):
            result = compile_shaders(ROOT / "shaders", cls.output)
            if result.returncode:
                raise AssertionError(result.stdout + result.stderr)

    def test_compile_and_validate_finite_shader_set(self):
        self.assertEqual({x.name for x in self.output.glob("*.spv")},
                         {"cull.spv", "pbr-vertex.spv", "pbr-fragment.spv", "shadow-vertex.spv", "shadow-fragment.spv", "post-vertex.spv", "post-fragment.spv"})
        for name, stage, entry in (("cull", "compute", "cullMain"),
                                   ("pbr-vertex", "vertex", "vertexMain"),
                                   ("pbr-fragment", "fragment", "fragmentMain"),
                                   ("shadow-vertex", "vertex", "shadowVertex"),
                                   ("shadow-fragment", "fragment", "shadowFragment"),
                                   ("post-vertex", "vertex", "postVertex"),
                                   ("post-fragment", "fragment", "postFragment")):
            with self.subTest(stage=stage):
                module = (self.output / (name + ".spv")).read_bytes()
                self.assertEqual(struct.unpack_from("<II", module), (0x07230203, 0x00010500))
                reflected = json.loads((self.output / (name + ".reflection.json")).read_text())
                self.assertEqual([(x["stage"], x["name"]) for x in reflected["entryPoints"]], [(stage, entry)])
                validated = subprocess.run(
                    [str(VALIDATOR), "--target-env", "vulkan1.3", str(self.output / (name + ".spv"))],
                    capture_output=True, text=True, timeout=30, check=False,
                )
                self.assertEqual(validated.returncode, 0, validated.stdout + validated.stderr)

    def test_descriptor_contract(self):
        cull = json.loads((self.output / "cull.reflection.json").read_text())
        bindings = {p["name"]: (p["binding"]["space"], p["binding"]["index"]) for p in cull["parameters"]}
        self.assertEqual(bindings, {"cull": (2, 0), "spheres": (2, 1), "visible": (2, 2)})
        self.assertEqual(cull["entryPoints"][0]["threadGroupSize"], [64, 1, 1])
        for name in ("pbr-vertex", "pbr-fragment", "shadow-vertex", "shadow-fragment"):
            pbr = json.loads((self.output / (name + ".reflection.json")).read_text())
            bindings = {p["name"]: (p["binding"].get("space", 0), p["binding"]["index"]) for p in pbr["parameters"]}
            self.assertEqual(bindings, {
                "frame": (0, 0), "localLights": (0, 1), "shadowMap": (0, 2), "shadowSampler": (0, 3), "instances": (0, 4),
                "material": (1, 0), "baseColorMap": (1, 1), "normalMap": (1, 2),
                "metallicRoughnessMap": (1, 3), "occlusionMap": (1, 4), "emissiveMap": (1, 5),
                "materialSamplers": (1, 6), "irradianceMap": (1, 7), "prefilteredMap": (1, 8), "brdfMap": (1, 9),
                "environmentSampler": (1, 10),
            })

            samplers = next(p for p in pbr["parameters"] if p["name"] == "materialSamplers")
            self.assertEqual(samplers["type"]["kind"], "array")
            self.assertEqual(samplers["type"]["elementCount"], 5)

    def test_uniform_layouts_match_compiled_cpu_types(self):
        native = subprocess.run([os.environ["NGEN_SHADER_LAYOUT"]], capture_output=True,
                                text=True, timeout=30, check=False)
        self.assertEqual(native.returncode, 0, native.stdout + native.stderr)
        layouts = {}
        for line in native.stdout.splitlines():
            name, size, alignment, *fields = line.split()
            layouts[name] = {"size": int(size), "alignment": int(alignment),
                             "fields": {key: int(value) for key, value in (x.split(":") for x in fields)}}
        checked = set()

        def check_struct(shader_type):
            name = shader_type["name"]
            layout = layouts[name]
            size = next(x for x in shader_type["sizes"] if x["kind"] == "uniform")
            self.assertEqual(size["value"], layout["size"], name + " byte size")
            self.assertEqual(size["alignment"], layout["alignment"], name + " alignment")
            self.assertEqual({x["name"] for x in shader_type["fields"]}, set(layout["fields"]), name + " fields")
            for field in shader_type["fields"]:
                self.assertEqual(field["binding"]["offset"], layout["fields"][field["name"]], name + "." + field["name"])
                element = field["type"]
                if element["kind"] == "array" and element["elementType"]["kind"] == "struct":
                    check_struct(element["elementType"])
                    self.assertEqual(element["uniformStride"], layouts[element["elementType"]["name"]]["size"])
            checked.add(name)

        for name in ("cull", "pbr-vertex", "pbr-fragment", "shadow-vertex", "shadow-fragment", "post-vertex", "post-fragment"):
            reflected = json.loads((self.output / (name + ".reflection.json")).read_text())
            for parameter in reflected["parameters"]:
                if parameter["type"]["kind"] == "constantBuffer":
                    check_struct(parameter["type"]["elementType"])
        self.assertEqual(checked, {"FrameConstants", "InstanceConstants", "InstanceTransform", "LightConstants", "LocalLight", "MaterialConstants", "CullConstants", "PostConstants"})
        vertex = json.loads((self.output / "pbr-vertex.reflection.json").read_text())
        fields = vertex["entryPoints"][0]["parameters"][0]["type"]["fields"]
        self.assertEqual([(x["name"], x["binding"]["index"], x["type"]["elementCount"]) for x in fields],
                         [("position", 0, 3), ("normal", 1, 3), ("uv", 2, 2), ("uv1", 4, 2), ("tangent", 3, 4)])
        self.assertEqual(layouts["Vertex"]["size"], 56)
        self.assertEqual(layouts["Vertex"]["fields"], {"position": 0, "normal": 12, "uv": 24, "uv1": 32, "tangent": 40})

    def test_tool_and_artifact_identity(self):
        metadata = json.loads((self.output / "manifest.json").read_text())
        self.assertEqual((metadata["target"], metadata["spirv"], metadata["matrix_layout"]),
                         ("vulkan1.3", "1.5", "column-major"))
        self.assertEqual(metadata["compiler"]["version"], "2026.21")
        executable = SLANG / "bin" / ("slangc.exe" if os.name == "nt" else "slangc")
        self.assertEqual(metadata["compiler"]["sha256"], hashlib.sha256(executable.read_bytes()).hexdigest())
        self.assertEqual(metadata["validator"]["sha256"], hashlib.sha256(VALIDATOR.read_bytes()).hexdigest())
        self.assertTrue(metadata["libraries"])
        for library in metadata["libraries"]:
            self.assertEqual(library["sha256"], hashlib.sha256((SLANG / library["file"]).read_bytes()).hexdigest())
        for shader in metadata["shaders"]:
            self.assertEqual(shader["sha256"], hashlib.sha256((self.output / (shader["name"] + ".spv")).read_bytes()).hexdigest())
            self.assertEqual(shader["reflection_sha256"], hashlib.sha256((self.output / (shader["name"] + ".reflection.json")).read_bytes()).hexdigest())

    def test_embedded_runtime_modules_match_validated_artifact_bytes(self):
        header = self.output / "ngen_shader_code.hpp"
        self.assertTrue(header.is_file(), "shipping shader modules were not embedded")
        metadata = json.loads((self.output / "manifest.json").read_text())
        self.assertEqual(metadata["runtime_header_sha256"], hashlib.sha256(header.read_bytes()).hexdigest())
        modules = {name: bytes().join(struct.pack("<I", int(word, 16)) for word in re.findall(r"0x[0-9a-f]{8}", words))
                   for name, words in re.findall(r"std::array<std::uint32_t, \d+> (\w+) = \{([^}]+)\};", header.read_text())}
        self.assertEqual(set(modules), {shader["name"].replace("-", "_") for shader in metadata["shaders"]})
        for shader in metadata["shaders"]:
            self.assertEqual(modules[shader["name"].replace("-", "_")], (self.output / (shader["name"] + ".spv")).read_bytes())

    def test_oversized_source_is_rejected_before_publication(self):
        with tempfile.TemporaryDirectory(prefix="ngen shader size ") as directory:
            source = Path(directory) / "source"
            shutil.copytree(ROOT / "shaders", source)
            with (source / "cull.slang").open("a") as stream:
                stream.write(" " * (4 * 1024 * 1024))
            output = Path(directory) / "package"
            result = compile_shaders(source, output)
            self.assertNotEqual(result.returncode, 0, "oversized shader source admitted")
            self.assertIn("shader source size", result.stderr)
            self.assertFalse(output.exists())

    def test_failed_later_entry_does_not_publish_partial_package(self):
        with tempfile.TemporaryDirectory(prefix="ngen shader transaction ") as directory:
            source = Path(directory) / "source"
            shutil.copytree(ROOT / "shaders", source)
            with (source / "pbr.slang").open("a") as stream:
                stream.write("\n#error NGEN_LATER_ENTRY_FAILURE\n")
            output = Path(directory) / "package"
            result = compile_shaders(source, output)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("NGEN_LATER_ENTRY_FAILURE", result.stderr)
            self.assertFalse(output.exists())
            self.assertEqual(list(Path(directory).glob(".ngen-shaders-*")), [])

    def test_existing_package_is_preserved(self):
        with tempfile.TemporaryDirectory(prefix="ngen shader preserve ") as directory:
            output = Path(directory) / "package"
            output.mkdir()
            (output / "owner-data").write_bytes(b"keep this package")
            result = compile_shaders(ROOT / "shaders", output)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(list(output.iterdir()), [output / "owner-data"])
            self.assertEqual((output / "owner-data").read_bytes(), b"keep this package")

    def test_incremental_recompile_verifies_identical_package_without_modification(self):
        with tempfile.TemporaryDirectory(prefix="ngen shader incremental ") as directory:
            output = Path(directory) / "package"
            shutil.copytree(self.output, output)
            before = {path.name: (path.read_bytes(), path.stat().st_mtime_ns) for path in output.iterdir()}
            result = compile_shaders(ROOT / "shaders", output, verify_existing=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual({path.name: (path.read_bytes(), path.stat().st_mtime_ns) for path in output.iterdir()}, before)

    def test_incremental_verification_rejects_corruption_and_extra_files_without_rewriting(self):
        with tempfile.TemporaryDirectory(prefix="ngen shader changed output ") as directory:
            for change in ("corrupt", "extra"):
                with self.subTest(change=change):
                    output = Path(directory) / change
                    shutil.copytree(self.output, output)
                    if change == "corrupt":
                        module = output / "cull.spv"
                        data = bytearray(module.read_bytes())
                        data[-1] ^= 1
                        module.write_bytes(data)
                    else:
                        (output / "owner-data").write_bytes(b"preserve this file")
                    before = {path.name: (path.read_bytes(), path.stat().st_mtime_ns) for path in output.iterdir()}
                    result = compile_shaders(ROOT / "shaders", output, verify_existing=True)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("existing shader package", result.stderr)
                    self.assertEqual({path.name: (path.read_bytes(), path.stat().st_mtime_ns) for path in output.iterdir()}, before)

    def test_repeat_canonical_compile_has_equal_artifact_bytes(self):
        with tempfile.TemporaryDirectory(prefix="ngen shader repeat ") as directory:
            output = Path(directory) / "package"
            result = compile_shaders(ROOT / "shaders", output)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            for artifact in self.output.iterdir():
                self.assertEqual((output / artifact.name).read_bytes(), artifact.read_bytes(), artifact.name)

    def test_reject_compiler_warning_without_publishing(self):
        with tempfile.TemporaryDirectory(prefix="ngen shader warning ") as directory:
            source = Path(directory) / "source"
            shutil.copytree(ROOT / "shaders", source)
            with (source / "cull.slang").open("a") as stream:
                stream.write("\n#warning NGEN_SHADER_WARNING_FIXTURE\n")
            output = Path(directory) / "package"
            result = compile_shaders(source, output)
            self.assertNotEqual(result.returncode, 0, "compiler warning was silently admitted")
            self.assertIn("NGEN_SHADER_WARNING_FIXTURE", result.stderr)
            self.assertFalse(output.exists())

    def test_reject_unenabled_draw_parameters_semantic(self):
        with tempfile.TemporaryDirectory(prefix="ngen post baseline ") as directory:
            source = Path(directory) / "source"
            shutil.copytree(ROOT / "shaders", source)
            post = source / "post.slang"
            text = post.read_text()
            self.assertIn("SV_VulkanVertexID", text)
            post.write_text(text.replace("SV_VulkanVertexID", "SV_VertexID"))
            output = Path(directory) / "package"
            result = compile_shaders(source, output)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("unsupported SPIR-V capability 4427", result.stderr)
            self.assertFalse(output.exists())

    def test_reject_unenabled_float64_without_publishing_package(self):
        with tempfile.TemporaryDirectory(prefix="ngen shader profile ") as directory:
            source = Path(directory) / "source"
            shutil.copytree(ROOT / "shaders", source)
            cull = source / "cull.slang"
            cull.write_text(cull.read_text()
                            .replace("StructuredBuffer<float4> spheres", "StructuredBuffer<double4> spheres")
                            .replace("float4 sphere = spheres[thread.x];", "float4 sphere = float4(spheres[thread.x]);"))
            output = Path(directory) / "package"
            result = compile_shaders(source, output)
            self.assertNotEqual(result.returncode, 0, "shader package admitted unenabled Float64")
            self.assertIn("unsupported SPIR-V capability 10", result.stderr)
            self.assertFalse(output.exists(), "rejected shader package was published")


if __name__ == "__main__":
    unittest.main()
