"""Create, validate and source-package one standalone NGEN starter project."""

import argparse
import io
import json
import os
from pathlib import Path
import re
import shutil
import struct
import sys
import tarfile
import tempfile

import creator_scene


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


def project_files(directory):
    return {name: creator_scene.read_file(directory, name, limit) for name, limit in LIMITS.items()}


def validate(files):
    try:
        manifest = json.loads(files[MANIFEST].decode("utf-8"), object_pairs_hook=creator_scene.unique_members)
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
    scene = commands.add_parser("scene")
    scene_commands = scene.add_subparsers(dest="action", required=True)
    for action in ("show", "edit", "undo", "redo"):
        command = scene_commands.add_parser(action)
        command.add_argument("project", type=Path)
        if action != "show":
            command.add_argument("--expect", required=True, type=int)
    args = parser.parse_args()
    try:
        if args.operation == "new":
            result = create(args.project, args.name)
        else:
            directory = os.open(args.project, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
            try:
                files = project_files(directory)
                result = validate(files)
                if args.operation == "scene":
                    data = sys.stdin.buffer.read(creator_scene.MAX_BYTES + 1) if args.action == "edit" else None
                    expected = None if args.action == "show" else args.expect
                    result = creator_scene.run(directory, args.action, expected, data)
                else:
                    files.update(creator_scene.authored_files(directory))
                    if args.operation == "package":
                        package(files, result, args.output)
                        result["archive"] = str(args.output)
            finally:
                os.close(directory)
    except (OSError, ValueError) as error:
        print(json.dumps({"ok": False, "operation": args.operation, "error": str(error)}), file=sys.stderr)
        return 1
    print(json.dumps({"ok": True, "operation": args.operation, **result}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
