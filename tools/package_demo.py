"""Build a private offline flight bundle from a Release build and local cooked asset."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE_PATHS = ["CMakeLists.txt", "CMakePresets.json", "conanfile.py", "conan.lock",
                "conan-demo.lock", "conan-physics.lock", "conan-render.lock", "conan-scene.lock",
                "conan-inspector.lock", "conan-assets.lock", "conan-runtime-assets.lock", "LICENSE",
                "README.md", "engine", "game", "apps", "shaders", "tools", "recipes", "profiles",
                "licenses", "tests"]
MIB = 1024 * 1024
SYSTEM_LINUX = {"libc.so.6", "libm.so.6", "libdl.so.2", "librt.so.1", "libpthread.so.0",
                "libstdc++.so.6", "libgcc_s.so.1", "libEGL.so.1", "libvulkan.so.1",
                "libpulse.so.0", "libdecor-0.so.0", "ld-linux-x86-64.so.2"}


def private_path(path):
    name = path.name.lower()
    return (name in {"auth.json", "mcp-auth.json", ".netrc", ".git-credentials"}
            or name == ".env" or name.startswith(".env.") and name != ".env.example"
            or any(part in {".ssh", ".aws"} for part in path.parts)
            or "/etc/ssl/private" in path.as_posix()
            or "intelephense" in name and ("license" in name or name.endswith(".lic")))


def regular(path, limit, *, root=None, allow_link=False):
    path = Path(path)
    if private_path(path) or path.is_symlink() and not allow_link:
        raise ValueError("unsafe package input path")
    resolved = path.resolve(strict=True)
    if private_path(resolved) or root is not None and not resolved.is_relative_to(root.resolve(strict=True)):
        raise ValueError("package input escapes its declared root")
    info = resolved.stat()
    if not stat.S_ISREG(info.st_mode) or info.st_size > limit:
        raise ValueError("package input is not a bounded regular file")
    return resolved


def external(arguments):
    return subprocess.run(arguments, check=True, capture_output=True, text=True, timeout=60).stdout


def sha256(path):
    value = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(MIB), b""):
            value.update(block)
    return value.hexdigest()


def dependency_directory(package, directory):
    root = Path(package["root"]).resolve(strict=True)
    path = Path(directory)
    if not path.is_absolute():
        path = root / path
    resolved = path.resolve(strict=True)
    if not resolved.is_relative_to(root) or private_path(resolved):
        raise ValueError("dependency directory escapes its declared root")
    return resolved


def corresponding_source(destination):
    changed = external(["git", "-C", str(ROOT), "status", "--porcelain", "--untracked-files=normal", "--", *SOURCE_PATHS])
    if changed.strip():
        raise ValueError("offline delivery requires committed source for its archived HEAD")
    revision = external(["git", "-C", str(ROOT), "rev-parse", "HEAD"]).strip()
    if not re.fullmatch(r"[0-9a-f]{40}", revision):
        raise ValueError("invalid source revision")
    entries = external(["git", "-C", str(ROOT), "ls-tree", "-r", "-z", "HEAD", "--", *SOURCE_PATHS])
    for entry in entries.split("\0"):
        if not entry:
            continue
        metadata, name = entry.split("\t", 1)
        path = PurePosixPath(name)
        if metadata.split()[0] != "100644" and metadata.split()[0] != "100755":
            raise ValueError("source archive contains a link or special entry")
        if path.is_absolute() or ".." in path.parts or private_path(path):
            raise ValueError("unsafe source archive path")
    external(["git", "-C", str(ROOT), "archive", "--format=tar.gz",
              "--output=" + str(destination), "HEAD", "--", *SOURCE_PATHS])
    regular(destination, 128 * MIB)
    return revision


def create_bundle(build: Path, asset: Path, output: Path) -> Path:
    build = Path(build).resolve(strict=True)
    asset = regular(asset, 256 * MIB)
    if asset.suffix.lower() != ".ngas":
        raise ValueError("the local flight asset must be a cooked .ngas package")
    metadata = json.loads(regular(build / "demo-runtime.json", 2 * MIB).read_text())
    if not isinstance(metadata, dict) or metadata.get("schema") != 1 or metadata.get("configuration") != "Release" or metadata.get("sanitizer", "") != "":
        raise ValueError("offline delivery requires an uninstrumented Release runtime manifest")
    platform = metadata.get("os")
    if not isinstance(platform, str) or platform not in {"Linux", "Windows"}:
        raise ValueError("unsupported offline flight platform")
    packages = metadata.get("packages")
    if not isinstance(packages, list) or not 1 <= len(packages) <= 64:
        raise ValueError("invalid runtime package collection")
    for package in packages:
        if not isinstance(package, dict) or not isinstance(package.get("reference"), str) or not isinstance(package.get("root"), str):
            raise ValueError("invalid runtime package shape")
        for field in ["libdirs", "bindirs"]:
            value = package.get(field)
            if not isinstance(value, list) or len(value) > 16 or any(not isinstance(item, str) or len(item) > 4096 for item in value):
                raise ValueError("invalid runtime directory collection")
        if not package["root"] or private_path(Path(package["root"])):
            raise ValueError("unsafe runtime package root")
    names = [package.get("name") for package in packages]
    if any(not isinstance(name, str) or not re.fullmatch(r"[a-z0-9][a-z0-9_.-]*", name) for name in names) or len(set(names)) != len(names):
        raise ValueError("invalid or duplicate runtime package identity")
    sdl = next((package for package in packages if package["name"] == "sdl"), None)
    if not sdl or not re.match(r"sdl/3\.4\.16(?:[@#]|$)", sdl["reference"]):
        raise ValueError("the pinned SDL runtime is missing")
    executable = regular(build / ("flight_demo.exe" if platform == "Windows" else "flight_demo"), 128 * MIB)
    output = Path(output).absolute()
    output.mkdir(mode=0o755, exist_ok=False)  # Exclusive reservation, including empty owner dirs.
    try:
        with tempfile.TemporaryDirectory(prefix=".building-", dir=output) as temporary:
            payload = Path(temporary) / "ngen-flight"
            payload.mkdir()
            (payload / "assets").mkdir()
            shutil.copyfile(executable, payload / executable.name)
            shutil.copyfile(asset, payload / "assets/ship.ngas")
            runtimes = []
            if platform == "Linux":
                library = None
                for directory in sdl.get("libdirs", []):
                    candidate = dependency_directory(sdl, directory) / "libSDL3.so.0"
                    if candidate.exists():
                        library = regular(candidate, 128 * MIB, root=Path(sdl["root"]), allow_link=True)
                        break
                if library is None:
                    raise ValueError("SDL shared runtime was not found")
                (payload / "lib").mkdir()
                copied = payload / "lib/libSDL3.so.0"
                shutil.copyfile(library, copied)
                external(["patchelf", "--set-rpath", "$ORIGIN/lib", str(payload / executable.name)])
                external(["patchelf", "--set-rpath", "$ORIGIN", str(copied)])
                for binary in (payload / executable.name, copied):
                    needed = external(["patchelf", "--print-needed", str(binary)]).splitlines()
                    if set(needed) - SYSTEM_LINUX - {"libSDL3.so.0"}:
                        raise ValueError("unexpected non-platform runtime dependency")
                (payload / "run.sh").write_text('#!/bin/sh\nset -eu\nroot=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\nexec "$root/flight_demo" "$root/assets/ship.ngas" "$@"\n')
                (payload / "run.sh").chmod(0o755)
                (payload / executable.name).chmod(0o755)
                runtimes = ["lib/libSDL3.so.0"]
            else:
                found = []
                for directory in sdl.get("bindirs", []):
                    candidate = dependency_directory(sdl, directory) / "SDL3.dll"
                    if candidate.exists():
                        found.append(regular(candidate, 128 * MIB, root=Path(sdl["root"]), allow_link=True))
                if len(found) != 1:
                    raise ValueError("SDL3.dll was not uniquely resolved")
                shutil.copyfile(found[0], payload / "SDL3.dll")
                runtimes.append("SDL3.dll")
                redist = regular(build / "demo-msvc-runtime.txt", MIB).read_text().splitlines()
                redist = [line for line in redist if line.strip()]
                if not 1 <= len(redist) <= 32:
                    raise ValueError("MSVC app-local runtime list is missing or unbounded")
                for name in redist:
                    source = regular(name, 128 * MIB)
                    if source.suffix.lower() != ".dll" or (payload / source.name).exists():
                        raise ValueError("invalid or duplicate compiler runtime")
                    shutil.copyfile(source, payload / source.name)
                    runtimes.append(source.name)
                (payload / "run.cmd").write_text('@echo off\r\n"%~dp0flight_demo.exe" "%~dp0assets\\ship.ngas"\r\nexit /b %errorlevel%\r\n', newline="")
            license_count = 0
            sdl_notices = 0
            for package in packages:
                root = Path(package["root"]).resolve(strict=True)
                source = root / "licenses"
                if not source.is_dir():
                    continue
                if source.is_symlink():
                    raise ValueError("dependency license directory must not be a link")
                for directory, directories, files in os.walk(source, followlinks=False):
                    if any((Path(directory) / item).is_symlink() for item in directories):
                        raise ValueError("dependency licenses contain linked directories")
                    for name in files:
                        original = regular(Path(directory) / name, 2 * MIB, root=root)
                        license_count += 1
                        if license_count > 1024:
                            raise ValueError("dependency license collection exceeds its bound")
                        destination = payload / "licenses" / package["name"] / original.relative_to(source)
                        destination.parent.mkdir(parents=True, exist_ok=True)
                        shutil.copyfile(original, destination)
                        if package["name"] == "sdl":
                            sdl_notices += 1
            if not sdl_notices:
                raise ValueError("the bundled SDL license notice is missing")
            shutil.copyfile(regular(ROOT / "LICENSE", MIB), payload / "LICENSE")
            revision = corresponding_source(payload / "source.tar.gz")
            (payload / "README.txt").write_text(
                "NGEN / offline flight demonstration\n\n"
                "Launch run.sh (Linux) or run.cmd (Windows). No SDK, Conan, shader compiler,\n"
                "login or Internet startup step is required. Linux targets the build host ABI\n"
                "and needs compatible glibc/libstdc++, Vulkan/EGL, libdecor, libpulse.so.0 and\n"
                "a desktop audio server (PipeWire's PulseAudio service or PulseAudio). NGEN\n"
                "does not open ALSA hardware directly. Windows needs its Vulkan-capable driver.\n\n"
                "TAB switches tactical/direct; RMB orbits/steers; ALT+RMB free-looks; wheel\n"
                "zooms. WASD thrusts, SHIFT/CTRL moves vertically, QE rolls, SPACE brakes.\n"
                "Release thrust to coast. G/O/F approach/orbit/focus a selected asteroid.\n"
                "R resets; M mutes; ESC releases; F1 shows help. Closing the window exits.\n\n"
                "The local asset is private owner-authorized content: do not publish this bundle.\n"
                "NGEN code is AGPL-3.0; source.tar.gz records the committed project source and\n"
                "locked dependency recipes, not a reproducible-build attestation or vendor mirror.\n"
                "Preserve applicable source/relinking obligations before redistribution.\n"
                "Dependency notices accompany the required SDL runtime. Other sound/graphics\n"
                "libraries are supplied by the operating system, not a development SDK.\n")
            files = sorted(path for path in payload.rglob("*") if path.is_file())
            if len(files) > 1100 or sum(path.stat().st_size for path in files) > 1024 * MIB:
                raise ValueError("offline bundle exceeds its declared file/byte profile")
            manifest = {"schema": 1, "platform": platform, "source_revision": revision,
                        "private_asset": True, "runtime_files": runtimes,
                        "platform_runtimes": sorted(SYSTEM_LINUX) if platform == "Linux" else ["Windows API", "Vulkan driver"],
                        "dependency_references": [package["reference"] for package in packages],
                        "files": {path.relative_to(payload).as_posix(): sha256(path) for path in files}}
            (payload / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
            final = output / "ngen-flight"
            payload.rename(final)
            return final
    except BaseException:
        try:
            output.rmdir()  # Only remove our now-empty reservation; never user files.
        except OSError:
            pass
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--asset", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        print(create_bundle(args.build, args.asset, args.output))
        return 0
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"Package failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
