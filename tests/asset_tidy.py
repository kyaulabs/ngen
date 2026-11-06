"""Process-boundary tests: preserve advisory reports; fail closed otherwise."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

POLICY = Path(__file__).resolve().parents[1] / "tools/assetc/check_tidy.py"


class AstPolicy(unittest.TestCase):
    def run_policy(self, kind):
        with tempfile.TemporaryDirectory(prefix="ngen AST policy ") as directory:
            root = Path(directory)
            cache = root / "cache"
            vendor = str(cache / "p/b/pkg/p/include/fastgltf/core.hpp")
            known = {"DiagnosticName": "clang-analyzer-optin.core.EnumCastOutOfRange",
                     "DiagnosticMessage": {"FilePath": vendor,
                         "Message": "The value '32832' provided to the cast expression is not in the valid range of values for 'Extensions'"}}
            payload = {"status": 1, "output": "native diagnostics retained\n",
                       "report": {"Diagnostics": [known]}}
            if kind == "clean":
                payload = {"status": 0, "output": "native clean\n", "report": {"Diagnostics": []}}
            elif kind in ("owned", "zero-exit-owned"):
                if kind == "zero-exit-owned":
                    payload["status"] = 0
                known["DiagnosticName"] = "clang-analyzer-core.NullDereference"
                known["DiagnosticMessage"]["FilePath"] = str(root / "owned.cpp")
            elif kind == "similar-owned-path":
                known["DiagnosticMessage"]["FilePath"] = str(root / "include/fastgltf/core.hpp")
            elif kind == "new-vendor-finding":
                known["DiagnosticMessage"]["Message"] = "a different enum finding"
            elif kind == "mixed":
                payload["report"]["Diagnostics"].append({"DiagnosticName": "clang-diagnostic-error"})
            elif kind == "setup":
                payload["output"] += "Error while processing file\n"
            elif kind == "missing":
                payload["report"] = None
            elif kind == "malformed":
                payload["report"] = {"Diagnostics": {"not": "an array"}}
            elif kind == "abnormal-exit":
                payload["status"] = 2
            executable = root / "fake tidy"
            executable.write_text("#!" + sys.executable + "\n" + '''import json,sys
from pathlib import Path
payload=json.loads(Path(__file__).with_suffix('.json').read_text())
report=next(arg.split('=',1)[1] for arg in sys.argv if arg.startswith('--export-fixes='))
if payload['report'] is not None:
    Path(report).write_text(json.dumps(payload['report']))
print(payload['output'],end='')
sys.exit(payload['status'])
''')
            executable.with_suffix(".json").write_text(json.dumps(payload))
            executable.chmod(0o700)
            return subprocess.run([sys.executable, str(POLICY), "--build", str(root),
                                   "--tidy", str(executable), "--conan-home", str(cache)],
                                  capture_output=True, text=True, timeout=15, check=False)

    def test_clean_native_run(self):
        result = self.run_policy("clean")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("::warning::", result.stdout)

    def test_known_vendor_finding_stays_visible_and_is_not_called_clean(self):
        result = self.run_policy("known")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("native diagnostics retained", result.stdout)
        self.assertIn("Native clang-tidy exit=1", result.stdout)
        self.assertIn("NOT a clean clang-tidy pass", result.stdout)

    def test_owned_and_unexpected_results_remain_fatal(self):
        for kind in ("owned", "zero-exit-owned", "similar-owned-path", "new-vendor-finding", "mixed",
                     "setup", "missing", "malformed", "abnormal-exit"):
            with self.subTest(kind=kind):
                result = self.run_policy(kind)
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
