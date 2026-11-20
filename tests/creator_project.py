"""Exercise the standalone starter through the public project CLI."""

import fcntl
import json
import os
from pathlib import Path
import select
import struct
import subprocess
import sys
import tarfile
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
CLI = ROOT / "tools/project.py"
IDENTITY = {"position_m": [0, 0, 0], "rotation_xyzw": [0, 0, 0, 1], "scale": [1, 1, 1]}


def play_args(project, revision):
    arguments = [sys.executable, str(CLI), "play", str(project), "--expect", str(revision)]
    for tool, variable in (("assetc", "NGEN_ASSETC_EXE"), ("ktx", "NGEN_KTX_EXE"),
                           ("probe", "NGEN_PACKAGE_PROBE"), ("player", "NGEN_CREATOR_PLAYER")):
        arguments += [f"--{tool}", os.environ[variable]]
    return arguments


def command(*args, payload=None):
    return subprocess.run([sys.executable, str(CLI), *map(str, args)], capture_output=True,
                          input=payload, text=True, timeout=30, check=False)


class CreatorProject(unittest.TestCase):
    def setUp(self):
        folder = tempfile.TemporaryDirectory(prefix="ngen creator ")
        self.addCleanup(folder.cleanup)
        self.root = Path(folder.name)
        self.project = self.root / "starter project"
        result = command("new", self.project, "--name", "starter")
        self.assertEqual(result.returncode, 0, result.stderr)

    def scene(self, operation, *, revision=None, edit=None):
        args = ("scene", operation, self.project)
        if revision is not None:
            args += ("--expect", revision)
        payload = json.dumps(edit) if edit is not None else None
        return command(*args, payload=payload)

    def scene_ok(self, operation, **args):
        result = self.scene(operation, **args)
        self.assertEqual(result.returncode, 0, result.stderr)
        return json.loads(result.stdout)

    @unittest.skipUnless(os.environ.get("NGEN_PLAY_SANDBOX_TEST"), "requires Linux bubblewrap")
    def test_owned_play_reports_a_checked_board_snapshot_and_stops(self):
        board = {"asset": "marker.glb", "transform": {**IDENTITY, "position_m": [1, 0, 0]}}
        self.scene_ok("edit", revision=0, edit={"board": board})
        result = subprocess.run(play_args(self.project, 1), capture_output=True, text=True,
                                input='{"op":"inspect"}\n{"op":"stop"}\n', timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)
        rows = [json.loads(line) for line in result.stdout.splitlines()]
        first = {"ok": True, "generation": 1, "revision": 1, "tick": 1,
                 "marker_mm": 1042, "instances": 1, "asset_cooks": 1}
        self.assertEqual(rows, [first, first, {"ok": True, "stopped": True}])

    @unittest.skipUnless(os.environ.get("NGEN_PLAY_SANDBOX_TEST"), "requires Linux bubblewrap")
    def test_owned_reload_reuses_geometry_and_keeps_active_state_on_failure(self):
        def board(x, asset="marker.glb"):
            return {"board": {"asset": asset, "transform": {**IDENTITY, "position_m": [x, 0, 0]}}}

        self.scene_ok("edit", revision=0, edit=board(0))
        with subprocess.Popen(play_args(self.project, 1), stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                              stderr=subprocess.DEVNULL, bufsize=0) as session:
            def reply(command=None):
                if command is not None:
                    session.stdin.write(json.dumps(command).encode() + b"\n")
                self.assertTrue(select.select([session.stdout], [], [], 20)[0], "no owned play reply")
                return json.loads(session.stdout.readline())

            try:
                first = reply()
                self.assertEqual(first["marker_mm"], 42)
                for x in range(1, 4):
                    self.scene_ok("edit", revision=x, edit=board(x))
                    active = reply({"op": "reload", "expect": x, "revision": x + 1})
                    self.assertEqual(active["marker_mm"], 1000 * x + 42)
                self.assertEqual(active["asset_cooks"], 1, "the four transform-only revisions reuse one cook")
                self.assertEqual(reply({"op": "inspect"}), active)
                stale = reply({"op": "reload", "expect": 1, "revision": 5})
                self.assertFalse(stale["ok"])
                self.assertEqual(stale["asset_cooks"], 1)

                valid = (self.project / "marker.glb").read_bytes()
                chunk_size = struct.unpack_from("<I", valid, 12)[0]
                metadata = json.loads(valid[20:20 + chunk_size])
                metadata["accessors"][0]["bufferView"] = 999
                text = json.dumps(metadata).encode()
                text += b" " * (-len(text) % 4)
                tail = valid[20 + chunk_size:]
                malformed = struct.pack("<5I", 0x46546c67, 2, 20 + len(text) + len(tail),
                                        len(text), 0x4e4f534a) + text + tail
                (self.project / "asset-broken.glb").write_bytes(malformed)
                self.scene_ok("edit", revision=4, edit=board(7, "asset-broken.glb"))
                rejected = reply({"op": "reload", "expect": 4, "revision": 5})
                self.assertFalse(rejected["ok"])
                self.assertIn("cook failed", rejected["error"])
                self.assertEqual(reply({"op": "inspect"}), {**active, "asset_cooks": 2})
                self.assertEqual(self.scene_ok("show")["revision"], 5)
                self.scene_ok("undo", revision=5)
                restored = reply({"op": "reload", "expect": 4, "revision": 6})
                self.assertEqual(restored, {**active, "asset_cooks": 2, "revision": 6, "generation": 5})
                invalid = self.scene("edit", revision=6, edit=board(10000))
                self.assertNotEqual(invalid.returncode, 0)
                self.assertEqual(self.scene_ok("show")["revision"], 6)
                self.assertEqual(reply({"op": "inspect"}), restored)
                self.assertEqual(reply({"op": "stop"}), {"ok": True, "stopped": True})
                self.assertEqual(session.wait(timeout=20), 0)
            finally:
                if session.poll() is None:
                    session.stdin.close()
                    try:
                        session.wait(timeout=20)
                    except subprocess.TimeoutExpired:
                        session.kill()
                        session.wait(timeout=10)

    def test_author_entities_and_reload_components_from_versioned_save(self):
        def pose(position):
            return {"position_m": position, "rotation_xyzw": [0, 0, 0, 1], "scale": [1, 1, 1]}

        entities = {
            "board": {"transform": pose([1, 0, 0]), "asset": "marker.glb",
                      "material": {"base_color": [1, 0.25, 0, 1], "metallic": 0, "roughness": 0.5}},
            "view": {"transform": pose([0, 2, 4]), "camera": {"vertical_fov_deg": 55,
                                                             "near_m": 0.1, "far_m": 100}},
            "key": {"transform": pose([3, 3, 2]), "light": {"color": [1, 0.8, 0.6], "lumens": 500}},
        }
        self.assertEqual(self.scene_ok("show")["revision"], 0)
        self.assertEqual(self.scene_ok("edit", revision=0, edit=entities)["revision"], 1)
        saved = self.scene_ok("show")
        self.assertEqual(saved["schema"], 1)
        self.assertEqual(saved["entities"], entities)
        board = {**entities["board"], "transform": pose([2, 0, -4])}
        edited = self.scene_ok("edit", revision=1, edit={"key": None, "board": board})
        self.assertEqual(edited["revision"], 2)
        self.assertEqual(self.scene_ok("show")["entities"], {"board": board, "view": entities["view"]})

    def test_undo_and_redo_survive_restart_and_branch_on_new_edit(self):
        original = {"transform": IDENTITY}
        self.scene_ok("edit", revision=0, edit={"board": original})
        self.scene_ok("edit", revision=1, edit={"board": None})
        undone = self.scene_ok("undo", revision=2)
        self.assertEqual(undone["revision"], 3)
        self.assertEqual(self.scene_ok("show")["entities"], {"board": original})
        redone = self.scene_ok("redo", revision=3)
        self.assertEqual(redone["revision"], 4)
        self.assertEqual(redone["entities"], {})
        self.scene_ok("undo", revision=4)
        self.scene_ok("edit", revision=5, edit={"new-board": original})
        rejected = self.scene("redo", revision=6)
        self.assertEqual(rejected.returncode, 1, rejected.stdout + rejected.stderr)
        self.assertIn("nothing to redo", json.loads(rejected.stderr)["error"])
        self.assertEqual(self.scene_ok("show")["revision"], 6)

    def test_authored_archive_reloads_scene_and_only_current_embedded_references(self):
        marker = (self.project / "marker.glb").read_bytes()
        for name in ("asset-board.glb", "asset-retired.glb", "asset-untracked.glb"):
            (self.project / name).write_bytes(marker)
        entity = {"transform": IDENTITY, "asset": "asset-retired.glb"}
        self.scene_ok("edit", revision=0, edit={"board": entity})
        entity["asset"] = "asset-board.glb"
        self.scene_ok("edit", revision=1, edit={"board": entity})
        archive = self.root / "authored.tar.gz"
        result = command("package", self.project, "--output", archive)
        self.assertEqual(result.returncode, 0, result.stderr)
        with tarfile.open(archive) as source:
            self.assertEqual({member.name for member in source}, {
                "starter/" + name for name in ("CMakeLists.txt", "main.cpp", "marker.glb",
                                               "ngen-project.json", "LICENSE", "ngen-scene.json",
                                               "asset-board.glb")})
            source.extractall(self.root / "authored", filter="data")
        self.project = self.root / "authored" / "starter"
        reloaded = self.scene_ok("show")
        self.assertEqual(reloaded["revision"], 2)
        self.assertEqual(reloaded["entities"], {"board": entity})
        self.assertIn("nothing to undo", self.scene("undo", revision=2).stderr)

    def test_invalid_scene_edits_preserve_the_committed_bytes(self):
        self.scene_ok("edit", revision=0, edit={"board": {"transform": IDENTITY}})
        path = self.project / "ngen-scene.json"
        original = path.read_bytes()
        reference = self.project / "asset-local.glb"
        entity = {"transform": IDENTITY, "asset": reference.name}

        def rejected(result, message):
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn(message, json.loads(result.stderr)["error"])
            self.assertEqual(path.read_bytes(), original)

        rejected(self.scene("edit", revision=0, edit={"stale": entity}), "stale scene revision")
        rejected(self.scene("undo", revision=0), "stale scene revision")
        rejected(self.scene("edit", revision=1, edit={"bad": {**entity, "asset": "../asset-bad.glb"}}),
                 "invalid asset reference")
        rejected(self.scene("edit", revision=1, edit={"bad": entity}), "referenced asset is missing")
        reference.symlink_to(self.project / "marker.glb")
        rejected(self.scene("edit", revision=1, edit={"bad": entity}), "asset-local.glb")
        reference.unlink()
        os.mkfifo(reference)
        rejected(self.scene("edit", revision=1, edit={"bad": entity}), "expected a nonempty regular file")
        reference.unlink()
        reference.write_bytes(struct.pack("<4sII", b"glTF", 2, 20) + bytes(8))
        rejected(self.scene("edit", revision=1, edit={"good": {"transform": IDENTITY}, "bad": entity}),
                 "expected an embedded GLB")
        marker = (self.project / "marker.glb").read_bytes()
        length, = struct.unpack_from("<I", marker, 12)
        gltf = json.loads(marker[20:20 + length])
        gltf["buffers"][0]["uri"] = "outside.bin"
        embedded = json.dumps(gltf).encode()
        embedded += b" " * (-len(embedded) % 4)
        chunks = struct.pack("<I4s", len(embedded), b"JSON") + embedded + marker[20 + length:]
        reference.write_bytes(struct.pack("<4sII", b"glTF", 2, len(chunks) + 12) + chunks)
        rejected(self.scene("edit", revision=1, edit={"bad": entity}), "expected an embedded GLB")
        invalid_entity = {"transform": {**IDENTITY, "position_m": [float("nan"), 0, 0]}}
        rejected(self.scene("edit", revision=1, edit={"bad": invalid_entity}), "invalid position")
        rejected(command("scene", "edit", self.project, "--expect", 1, payload='{"id":{},"id":{}}'),
                 "invalid scene JSON")
        rejected(command("scene", "edit", self.project, "--expect", 1, payload=" " * (256 * 1024 + 1)),
                 "invalid edit byte count")

        directory = os.open(self.project, os.O_RDONLY | os.O_DIRECTORY)
        try:
            fcntl.flock(directory, fcntl.LOCK_EX | fcntl.LOCK_NB)
            rejected(self.scene("edit", revision=1, edit={"board": None}), "scene is busy")
            self.assertEqual(self.scene_ok("show")["revision"], 1)
        finally:
            os.close(directory)
        self.assertFalse(list(self.project.glob(".ngen-scene-*")))

    @unittest.skipIf(os.geteuid() == 0, "root can write the read-only failure fixture")
    def test_failed_scene_save_keeps_last_committed_revision(self):
        entity = {"transform": IDENTITY}
        self.scene_ok("edit", revision=0, edit={"board": entity})
        path = self.project / "ngen-scene.json"
        original = path.read_bytes()
        self.project.chmod(0o500)
        try:
            failed = self.scene("edit", revision=1, edit={"board": None})
            self.assertEqual(failed.returncode, 1, failed.stdout + failed.stderr)
            self.assertIn("Permission denied", json.loads(failed.stderr)["error"])
        finally:
            self.project.chmod(0o700)
        self.assertEqual(path.read_bytes(), original)
        self.assertEqual(self.scene_ok("show")["revision"], 1)
        self.assertEqual(self.scene_ok("edit", revision=1, edit={"board": None})["revision"], 2)

    def test_version_and_shape_are_validated(self):
        valid = command("validate", self.project)
        self.assertEqual(valid.returncode, 0, valid.stderr)
        self.assertEqual(json.loads(valid.stdout)["schema"], 1)
        manifest = self.project / "ngen-project.json"
        for content in ('{"schema": 2, "name": "starter"}', '{"schema": true, "name": "starter"}',
                        '{"schema": 1, "schema": 1, "name": "starter"}',
                        '{"schema": 1, "name": "../escape"}',
                        '{"schema": 1, "name": "starter", "extra": []}', '[' * 4000):
            with self.subTest(content=content[:80]):
                manifest.write_text(content, encoding="utf-8")
                rejected = command("validate", self.project)
                self.assertNotEqual(rejected.returncode, 0, rejected.stdout)
                self.assertFalse(json.loads(rejected.stderr)["ok"])

    def test_package_is_source_only_and_cannot_replace_good_output(self):
        archive = self.root / "source.tar.gz"
        (self.project / "scene.ngas").write_bytes(b"not a source file")
        (self.project / "private-local.txt").write_bytes(b"not an archive member")
        packaged = command("package", self.project, "--output", archive)
        self.assertEqual(packaged.returncode, 0, packaged.stderr)
        with tarfile.open(archive) as source:
            self.assertEqual({member.name for member in source},
                             {"starter/" + name for name in ("CMakeLists.txt", "main.cpp", "marker.glb",
                                                            "ngen-project.json", "LICENSE")})
        original = archive.read_bytes()
        duplicate = command("package", self.project, "--output", archive)
        self.assertNotEqual(duplicate.returncode, 0)
        self.assertFalse(json.loads(duplicate.stderr)["ok"])
        self.assertEqual(archive.read_bytes(), original)

        (self.project / "marker.glb").write_bytes(b"not GLB")
        new_archive = self.root / "missing.tar.gz"
        invalid = command("package", self.project, "--output", new_archive)
        self.assertNotEqual(invalid.returncode, 0)
        self.assertIn("invalid GLB", json.loads(invalid.stderr)["error"])
        self.assertFalse(new_archive.exists())

    def test_existing_project_and_nonregular_sources_are_not_admitted(self):
        existing = command("new", self.project, "--name", "replacement")
        self.assertNotEqual(existing.returncode, 0)
        self.assertEqual(json.loads(command("validate", self.project).stdout)["name"], "starter")
        invalid = self.root / "invalid"
        self.assertNotEqual(command("new", invalid, "--name", "../unsafe").returncode, 0)
        self.assertFalse(invalid.exists())

        marker = self.project / "marker.glb"
        marker.unlink()
        target = self.root / "external fixture.glb"
        target.write_bytes(struct.pack("<4sII", b"glTF", 2, 20) + bytes(8))
        marker.symlink_to(target)
        self.assertNotEqual(command("validate", self.project).returncode, 0)
        marker.unlink()
        os.mkfifo(marker)
        self.assertNotEqual(command("validate", self.project).returncode, 0)

    @unittest.skipUnless(os.environ.get("NGEN_STARTER_BUILD"), "requires the built asset SDK and cooker graph")
    def test_fresh_source_archive_builds_cooks_tests_and_runs(self):
        def run(*args, success=True):
            result = subprocess.run(list(map(str, args)), capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
            return result

        build = Path(os.environ["NGEN_STARTER_BUILD"])
        configuration = os.environ["NGEN_STARTER_CONFIG"]
        prefix = self.root / "engine prefix"
        run("cmake", "--install", build, "--config", configuration, "--prefix", prefix)
        (self.project / "asset-board.glb").write_bytes((self.project / "marker.glb").read_bytes())
        entity = {"transform": IDENTITY, "asset": "asset-board.glb"}
        self.scene_ok("edit", revision=0, edit={"board": entity})
        archive = self.root / "starter.tar.gz"
        run(sys.executable, CLI, "package", self.project, "--output", archive)
        original = archive.read_bytes()
        extracted = self.root / "unpacked"
        with tarfile.open(archive) as source:
            source.extractall(extracted, filter="data")
        project = extracted / "starter"
        run(sys.executable, CLI, "validate", project)
        saved = json.loads(run(sys.executable, CLI, "scene", "show", project).stdout)
        self.assertEqual(saved["entities"], {"board": entity})
        asset = project / saved["entities"]["board"]["asset"]
        report = json.loads(run(os.environ["NGEN_ASSETC_EXE"], "inspect", asset).stdout)
        self.assertEqual(report["triangles"], 1)

        def cook(src, output, success=True):
            return run(sys.executable, ROOT / "tools/assetc/cook.py", "--assetc", os.environ["NGEN_ASSETC_EXE"],
                       "--ktx", os.environ["NGEN_KTX_EXE"], "--probe", os.environ["NGEN_PACKAGE_PROBE"],
                       "--source", src, "--output", output, success=success)

        cooked = self.root / "cooked board"
        scene = cooked / "scene.ngas"
        cook(asset, cooked)
        expected = scene.read_bytes()
        invalid = self.root / "invalid.glb"
        invalid.write_bytes(b"not GLB")
        failed = self.root / "failed cook"
        rejected = cook(invalid, failed, success=False)
        self.assertIn("offline tool failed", rejected.stderr)
        self.assertFalse(failed.exists())
        cook(project / "marker.glb", cooked, success=False)
        self.assertEqual(scene.read_bytes(), expected)

        obj = self.root / "consumer build"
        run("cmake", "-S", project, "-B", obj, "-G", "Ninja", f"-DCMAKE_BUILD_TYPE={configuration}",
            f"-DCMAKE_TOOLCHAIN_FILE={os.environ['NGEN_STARTER_TOOLCHAIN']}",
            # Conan can put the producer directory ahead of the install prefix.
            f"-DCMAKE_PREFIX_PATH={build};{prefix}", f"-DNGEN_STARTER_SCENE={scene}")
        run("cmake", "--build", obj, "--parallel", "2")
        run("ctest", "--test-dir", obj, "--output-on-failure", "--no-tests=error")
        self.assertIn("piece moved 42 mm", run(obj / "ngen_starter", scene).stdout)
        scene.write_bytes(b"bad package")
        self.assertIn("scene:", run(obj / "ngen_starter", scene, success=False).stderr)
        (project / "main.cpp").write_text('#error controlled starter build failure\n', encoding="utf-8")
        self.assertIn("controlled starter build failure", run("cmake", "--build", obj, success=False).stdout)
        self.assertEqual(archive.read_bytes(), original)


if __name__ == "__main__":
    unittest.main()
