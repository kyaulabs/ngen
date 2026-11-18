"""Verify the installed SDK from independent CMake projects, without a display."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


FIXTURE = Path(__file__).resolve().with_name("sdk_consumer")


def run(*args, env=None):
    subprocess.run(args, check=True, timeout=90, env=env)


def verify(build, toolchain, preset, renderer):
    configuration = "Release" if preset == "release" else "Debug"
    if not toolchain.is_file():
        raise ValueError(f"Missing locked build toolchain: {toolchain}")
    with tempfile.TemporaryDirectory(prefix="sdk-consumer-", dir=build) as scratch:
        workspace = Path(scratch)
        staging = workspace / "installed"
        run("cmake", "--install", build, "--config", configuration, "--prefix", staging)
        prefix = staging.rename(workspace / "relocated SDK")
        if not (prefix / "share/doc/ngen/LICENSE").is_file():
            raise ValueError("SDK prefix is missing the project license")
        includes = prefix / "include/ngen"
        for path in includes.rglob("*"):
            if path.is_file() and (path.suffix != ".hpp" or path.stem.endswith("_internal")):
                raise ValueError(f"Unexpected installed header-tree entry: {path.relative_to(includes)}")
        if renderer and not (prefix / "share/doc/ngen/licenses/vma.txt").is_file():
            raise ValueError("The renderer prefix is missing the embedded VMA notice")

        source = shutil.copytree(FIXTURE, workspace / "consumer source")
        configure = ("cmake", "-S", source, "-G", "Ninja", f"-DCMAKE_BUILD_TYPE={configuration}",
                     f"-DCMAKE_TOOLCHAIN_FILE={toolchain}", f"-DSDK_PREFIX={prefix}")
        env = os.environ.copy()
        for key in ("DISPLAY", "WAYLAND_DISPLAY"):
            env.pop(key, None)
        if preset == "asan":
            env.update(ASAN_OPTIONS="detect_leaks=1:halt_on_error=1",
                       UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
        elif preset == "tsan":
            env["TSAN_OPTIONS"] = "halt_on_error=1"
        for name in ("headless", "renderer") if renderer else ("headless",):
            consumer = workspace / name
            flag = "ON" if name == "renderer" else "OFF"
            run(*configure, "-B", consumer, f"-DWITH_RENDERER={flag}")
            run("cmake", "--build", consumer, "--parallel", "2")
            run("ctest", "--test-dir", consumer, "--output-on-failure", "--no-tests=error", env=env)
        if not renderer:
            result = subprocess.run((*configure, "-B", workspace / "missing-renderer", "-DWITH_RENDERER=ON"),
                                    capture_output=True, text=True, timeout=90)
            if result.returncode == 0 or "This SDK prefix has no Renderer component" not in result.stderr:
                raise ValueError(f"Expected an explicit missing Renderer diagnostic:\n{result.stdout}{result.stderr}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("preset", choices=("debug", "release", "asan", "tsan"))
    parser.add_argument("--renderer", action="store_true")
    parser.add_argument("--toolchain", type=Path)
    args = parser.parse_args()
    build = args.build.resolve()
    toolchain = args.toolchain or build / "conan_toolchain.cmake"
    verify(build, toolchain.resolve(), args.preset, args.renderer)


if __name__ == "__main__":
    main()
