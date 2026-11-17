"""Real Conan generator contract; no Windows compiler or vendor source execution."""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
CONAN = shutil.which("conan")


class JoltRuntime(unittest.TestCase):
    def test_declared_windows_crt_controls_jolt_for_both_compiler_frontends(self):
        self.assertIsNotNone(CONAN, "Conan must be available for the recipe generator check")
        with tempfile.TemporaryDirectory(prefix="ngen jolt crt ") as directory:
            root = Path(directory)
            recipe = root / "recipe"
            recipe.mkdir()
            for name in ("conanfile.py", "conandata.yml"):
                shutil.copy2(ROOT / "recipes/joltphysics" / name, recipe / name)
            for compiler, profile in (("msvc", "windows-msvc"),
                                      ("clang", "windows-clang-cl")):
                for configuration in ("Debug", "Release"):
                    for runtime in ("dynamic", "static"):
                        with self.subTest(compiler=compiler, configuration=configuration,
                                          runtime=runtime):
                            output = root / f"{compiler}-{configuration}-{runtime}"
                            result = subprocess.run(
                                [CONAN, "install", str(recipe), "-pr:h", str(ROOT / "profiles" / profile),
                                 "-pr:b", str(ROOT / "profiles/windows-msvc"),
                                 "-s:h", f"build_type={configuration}", "-s:h", f"compiler.runtime={runtime}",
                                 "-c:h", "tools.microsoft.msbuild:installation_path=",
                                 "-c:b", "tools.microsoft.msbuild:installation_path=", "-of", str(output)],
                                capture_output=True, text=True, timeout=60, check=False)
                            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                            presets = list(output.rglob("CMakePresets.json"))
                            self.assertEqual(len(presets), 1)
                            data = json.loads(presets[0].read_text())
                            variables = data["configurePresets"][0]["cacheVariables"]
                            self.assertIn("USE_STATIC_MSVC_RUNTIME_LIBRARY", variables,
                                          "Jolt must not fall back to its static CRT default")
                            self.assertEqual(variables["USE_STATIC_MSVC_RUNTIME_LIBRARY"],
                                             "ON" if runtime == "static" else "OFF")


if __name__ == "__main__":
    unittest.main()
