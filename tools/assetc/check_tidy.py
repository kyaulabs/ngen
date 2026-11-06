"""Report asset-consumer AST results; known vendor findings remain advisory.

Requires PyYAML, already installed by the project's Conan environment. This does
not change clang-tidy checks or compiler arguments, or claim a clean native run.
"""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

import yaml

SOURCES = ["tools/assetc/import.cpp", "tools/assetc/metadata.cpp", "tools/assetc/png.cpp",
           "tools/assetc/tangents.cpp", "tools/assetc/main.cpp", "tests/asset_import.cpp"]
KNOWN = {
    ("fastgltf/core.hpp", "The value '32832' provided to the cast expression is not in the valid range of values for 'Extensions'"),
    ("catch2/internal/catch_result_type.hpp", "The value '6' provided to the cast expression is not in the valid range of values for 'Flags'"),
}


def known_vendor(diagnostic, cache):
    if diagnostic.get("DiagnosticName") != "clang-analyzer-optin.core.EnumCastOutOfRange":
        return False
    message = diagnostic.get("DiagnosticMessage", {})
    if not isinstance(message, dict):
        return False
    path = Path(message.get("FilePath", ""))
    try:
        relative = path.relative_to(cache / "p" / "b")
    except ValueError:
        return False
    # Exactly a Conan package's exported header, never a similar owned filename.
    if len(relative.parts) < 4 or relative.parts[1:3] != ("p", "include"):
        return False
    return (Path(*relative.parts[3:]).as_posix(), message.get("Message")) in KNOWN


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", required=True)
    parser.add_argument("--tidy", default="clang-tidy")
    parser.add_argument("--conan-home", type=Path, default=Path.home() / ".conan2")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="ngen-asset-tidy-") as directory:
        report = Path(directory) / "diagnostics.yaml"
        result = subprocess.run([args.tidy, "-p", args.build, *SOURCES,
                                 "--export-fixes=" + str(report)],
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, check=False)
        print(result.stdout, end="")
        print(f"Native clang-tidy exit={result.returncode}; compiler arguments/checks unchanged.")
        if result.returncode not in (0, 1) or "Error while processing file" in result.stdout:
            return 1
        if not report.exists():
            return 0 if result.returncode == 0 else 1
        if report.stat().st_size > 8 * 1024 * 1024:
            return 1
        try:
            data = yaml.safe_load(report.read_text())
            diagnostics = data["Diagnostics"]
            if not isinstance(diagnostics, list):
                return 1
            unexpected = [item for item in diagnostics
                          if not isinstance(item, dict) or not known_vendor(item, args.conan_home)]
        except (OSError, ValueError, TypeError, KeyError, yaml.YAMLError):
            return 1
        if unexpected or (result.returncode != 0 and not diagnostics):
            print("Owned, new, malformed or unavailable AST result: failing the gate.")
            return 1
        if diagnostics:
            print(f"::warning::Asset AST: {len(diagnostics)} known vendor findings remain advisory; NOT a clean clang-tidy pass.")
        return 0


if __name__ == "__main__":
    sys.exit(main())
