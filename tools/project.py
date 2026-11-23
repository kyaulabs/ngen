"""Create, validate and source-package one standalone NGEN starter project."""

import argparse
import fcntl
import hashlib
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
PROJECT_NAME = r"^[a-z][a-z0-9-]{0,39}$"
LIMITS = {MANIFEST: 4096, "CMakeLists.txt": 64 * 1024, "main.cpp": 256 * 1024,
          "marker.glb": 16 * 1024**2, "LICENSE": 64 * 1024}
EDITS = {"rename_project": MANIFEST, "replace_source": "main.cpp", "edit_scene": creator_scene.SCENE}


def project_name(name):
    if not isinstance(name, str) or not re.fullmatch(PROJECT_NAME, name):
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


def digest(data):
    return hashlib.sha256(data).hexdigest()


def edit_revisions(directory):
    with creator_scene.locked(directory, fcntl.LOCK_SH) as fd:
        files = project_files(fd)
        validate(files)
        return {"rename_project": digest(files[MANIFEST]), "replace_source": digest(files["main.cpp"]),
                "edit_scene": str(creator_scene.load(fd)["revision"])}


def line_range(before, after):
    """Return a common-prefix/suffix diff with no source text in the result."""
    left, right = before.splitlines(keepends=True), after.splitlines(keepends=True)
    prefix = 0
    while prefix < min(len(left), len(right)) and left[prefix] == right[prefix]:
        prefix += 1
    last_left, last_right = len(left), len(right)
    while last_left > prefix and last_right > prefix and left[last_left - 1] == right[last_right - 1]:
        last_left -= 1
        last_right -= 1
    return [prefix + 1, last_left - prefix, last_right - prefix]


def edit(directory, operation, expected, data, *, apply):
    if operation not in EDITS:
        raise ValueError("unknown edit operation")
    name = EDITS[operation]
    if not 0 < len(data) <= (creator_scene.MAX_BYTES if name == creator_scene.SCENE else LIMITS[name]):
        raise ValueError("invalid edit byte count")
    with creator_scene.locked(directory, fcntl.LOCK_EX if apply else fcntl.LOCK_SH) as fd:
        files = project_files(fd)
        manifest = validate(files)
        if operation == "edit_scene":
            if not re.fullmatch(r"0|[1-9][0-9]{0,18}", expected) or int(expected) > creator_scene.MAX_REVISION:
                raise ValueError("invalid scene revision")
            old, document = creator_scene.prepare(fd, "edit", int(expected), data)
            new = document["entities"]
            diff = {"added": sorted(new.keys() - old.keys()), "removed": sorted(old.keys() - new.keys()),
                    "changed": sorted(key for key in old.keys() & new.keys() if old[key] != new[key])}
            payload, revision = creator_scene.serialize(document), str(document["revision"])
        else:
            if not re.fullmatch(r"[0-9a-f]{64}", expected):
                raise ValueError("invalid file revision")
            current = files[name]
            if digest(current) != expected:
                raise creator_scene.StaleRevision("stale file revision")
            text = data.decode("utf-8")
            if operation == "rename_project":
                renamed = project_name(text)
                diff = {"name": [manifest["name"], renamed]}
                payload = (json.dumps({**manifest, "name": renamed}) + "\n").encode("utf-8")
            else:
                if not text.strip() or any(ord(c) < 32 and c not in "\r\n\t" for c in text):
                    raise ValueError("source must be nonempty UTF-8 text without control bytes")
                diff = {"line_range": line_range(current, data)}
                payload = data
            revision = digest(payload)
        if apply:
            creator_scene.atomic_replace(fd, name, payload)
        return {"applied": apply, "before": expected, "after": revision, "diff": diff}


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
    revisions = commands.add_parser("edit-revisions")
    revisions.add_argument("project", type=Path)
    edit_command = commands.add_parser("edit")
    edit_command.add_argument("project", type=Path)
    edit_command.add_argument("--operation", dest="edit_operation", required=True, choices=EDITS)
    edit_command.add_argument("--expect", required=True)
    edit_command.add_argument("--preview", action="store_true")
    scene = commands.add_parser("scene")
    scene_commands = scene.add_subparsers(dest="action", required=True)
    for action in ("show", "edit", "undo", "redo"):
        command = scene_commands.add_parser(action)
        command.add_argument("project", type=Path)
        if action != "show":
            command.add_argument("--expect", required=True, type=int)
    play = commands.add_parser("play")
    play.add_argument("project", type=Path)
    play.add_argument("--expect", required=True, type=int)
    for tool in ("assetc", "ktx", "probe", "player"):
        play.add_argument(f"--{tool}", required=True, type=Path)
    args = parser.parse_args()
    try:
        if args.operation == "new":
            result = create(args.project, args.name)
        else:
            directory = os.open(args.project, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
            try:
                if args.operation == "play":
                    import creator_play
                    creator_play.run(directory, lambda fd: validate(project_files(fd)), args)
                    return 0
                if args.operation == "edit-revisions":
                    result = {"revisions": edit_revisions(directory)}
                elif args.operation == "edit":
                    data = sys.stdin.buffer.read(creator_scene.MAX_BYTES + 1)
                    result = edit(directory, args.edit_operation, args.expect, data, apply=not args.preview)
                elif args.operation == "scene" and args.action != "show":
                    data = sys.stdin.buffer.read(creator_scene.MAX_BYTES + 1) if args.action == "edit" else None
                    with creator_scene.locked(directory, fcntl.LOCK_EX) as fd:
                        validate(project_files(fd))
                        _, document = creator_scene.prepare(fd, args.action, args.expect, data)
                        creator_scene.save(fd, document)
                        result = creator_scene.view(document)
                else:
                    with creator_scene.locked(directory, fcntl.LOCK_SH) as fd:
                        files = project_files(fd)
                        result = validate(files)
                        if args.operation == "scene":
                            result = creator_scene.view(creator_scene.load(fd))
                        else:
                            files.update(creator_scene.authored_files(fd))
                    if args.operation == "package":
                        package(files, result, args.output)
                        result["archive"] = str(args.output)
            finally:
                os.close(directory)
    except (OSError, RuntimeError, ValueError) as error:
        print(json.dumps({"ok": False, "operation": args.operation, "error": str(error)}), file=sys.stderr)
        return 1
    print(json.dumps({"ok": True, "operation": args.operation, **result}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
