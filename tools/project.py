"""Create, validate and source-package one standalone NGEN starter project."""

import argparse
import io
import json
import os
from pathlib import Path
import re
import shutil
import stat
import struct
import sys
import tarfile
import tempfile


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = "ngen-project.json"
LIMITS = {MANIFEST: 4096, "CMakeLists.txt": 64 * 1024, "main.cpp": 256 * 1024,
          "marker.glb": 16 * 1024**2, "LICENSE": 64 * 1024}


def project_name(name):
    if not isinstance(name, str) or not re.fullmatch(r"[a-z][a-z0-9-]{0,39}", name):
        raise ValueError("name must be a lowercase project slug of 1–40 characters")
    return name


def create(project, name):
    project_name(name)
    project.mkdir()
    try:
        for file in ("CMakeLists.txt", "main.cpp", "marker.glb"):
            shutil.copyfile(ROOT / "tools/project_template" / file, project / file)
        shutil.copyfile(ROOT / "LICENSE", project / "LICENSE")
        (project / MANIFEST).write_text(json.dumps({"schema": 1, "name": name}) + "\n", encoding="utf-8")
    except OSError:
        shutil.rmtree(project)
        raise
    return {"schema": 1, "name": name}


def unique_members(pairs):
    record = dict(pairs)
    if len(record) != len(pairs):
        raise ValueError("manifest contains a duplicate member")
    return record


def project_files(project):
    directory = os.open(project, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        files = {}
        for name, limit in LIMITS.items():
            fd = os.open(name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=directory)
            with os.fdopen(fd, "rb") as stream:
                entry = os.fstat(stream.fileno())
                if not stat.S_ISREG(entry.st_mode) or not 0 < entry.st_size <= limit:
                    raise ValueError(f"{name}: expected a nonempty regular file of at most {limit} bytes")
                data = stream.read(limit + 1)
                if len(data) != entry.st_size:
                    raise ValueError(f"{name}: file size changed during validation")
                files[name] = data
        return files
    finally:
        os.close(directory)


def validate(files):
    try:
        manifest = json.loads(files[MANIFEST].decode("utf-8"), object_pairs_hook=unique_members)
    except (UnicodeError, json.JSONDecodeError, RecursionError) as error:
        raise ValueError("invalid manifest JSON") from error
    if not isinstance(manifest, dict) or manifest.keys() != {"schema", "name"}:
        raise ValueError("manifest must contain only schema and name")
    if type(manifest["schema"]) is not int or manifest["schema"] != 1:
        raise ValueError("expected project schema 1")
    project_name(manifest["name"])
    marker = files["marker.glb"]
    if len(marker) < 20 or struct.unpack_from("<4sII", marker) != (b"glTF", 2, len(marker)):
        raise ValueError("marker.glb: invalid GLB v2 container header")
    if files["LICENSE"] != (ROOT / "LICENSE").read_bytes():
        raise ValueError("LICENSE: the starter requires the supplied NGEN license")
    return manifest


def package(files, manifest, output):
    # Publish once, in the destination filesystem. link() cannot clobber an
    # existing file (including a symlink), even if it appears during staging.
    with tempfile.TemporaryDirectory(prefix=".ngen-project-", dir=output.parent) as scratch:
        staged = Path(scratch) / "source.tar.gz"
        with tarfile.open(staged, "w:gz") as archive:
            for name, data in files.items():
                member = tarfile.TarInfo(f"{manifest['name']}/{name}")
                member.size = len(data)
                archive.addfile(member, io.BytesIO(data))
        os.link(staged, output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="operation", required=True)
    for operation in ("new", "validate", "package"):
        command = commands.add_parser(operation)
        command.add_argument("project", type=Path)
        if operation == "new":
            command.add_argument("--name", required=True)
        elif operation == "package":
            command.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        if args.operation == "new":
            result = create(args.project, args.name)
        else:
            files = project_files(args.project)
            result = validate(files)
            if args.operation == "package":
                package(files, result, args.output)
                result["archive"] = str(args.output)
    except (OSError, ValueError) as error:
        print(json.dumps({"ok": False, "operation": args.operation, "error": str(error)}), file=sys.stderr)
        return 1
    print(json.dumps({"ok": True, "operation": args.operation, **result}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
