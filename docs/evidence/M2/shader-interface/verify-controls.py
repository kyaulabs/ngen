"""Retain shader-test sensitivity controls without modifying production sources."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--root", type=Path, required=True)
parser.add_argument("--slang-root", type=Path, required=True)
parser.add_argument("--validator", type=Path, required=True)
parser.add_argument("--probe", type=Path, required=True)
parser.add_argument("--logs", type=Path, required=True)
args = parser.parse_args()
args.logs.mkdir(parents=True, exist_ok=False)
cases = (
    ("culling-guard", "cull.slang", "    if (thread.x >= cull.count)\n        return;\n", "", "culling output or out-of-range guard mismatch"),
    ("matrix-convention", "pbr.slang", "mul(frame.model, float4(input.position, 1))", "mul(float4(input.position, 1), frame.model)", "matrix-transformed geometry coverage mismatch"),
)
for name, file, old, new, expected in cases:
    with tempfile.TemporaryDirectory(prefix="ngen shader control ") as directory:
        source = Path(directory) / "source"
        shutil.copytree(args.root / "shaders", source)
        path = source / file
        text = path.read_text()
        if text.count(old) != 1:
            raise RuntimeError(f"{name}: control no longer matches the actual shader")
        path.write_text(text.replace(old, new))
        package = Path(directory) / "package"
        compiled = subprocess.run(
            [sys.executable, str(args.root / "tools/shaderc/compile.py"),
             "--slang-root", str(args.slang_root), "--validator", str(args.validator),
             "--source-dir", str(source), "--output", str(package)],
            capture_output=True, text=True, timeout=120, check=False,
        )
        (args.logs / (name + "-compile.txt")).write_text(compiled.stdout + compiled.stderr)
        if compiled.returncode:
            raise RuntimeError(f"{name}: invalid control setup, compiler exited {compiled.returncode}")
        tested = subprocess.run([str(args.probe), str(package)], capture_output=True, text=True, timeout=50, check=False)
        log = tested.stdout + tested.stderr
        (args.logs / (name + "-native.txt")).write_text(log)
        if tested.returncode != 1 or expected not in log or "Vulkan validation:" in log or "Sanitizer" in log:
            raise RuntimeError(f"{name}: control did not expose the intended behavioral mismatch: {log}")
        print(f"{name}: expected_exit=1 behavioral_failure=1 validation_diagnostic=0")
