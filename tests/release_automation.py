"""Source-release public contracts; remote mutations use synthetic command boundaries."""
from pathlib import Path
import io
import json
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import release as RELEASE


class Version(unittest.TestCase):
    def test_mismatched_and_noncanonical_release_branches_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "CMakeLists.txt").write_text("project(NGEN VERSION 0.1.0 LANGUAGES CXX)\n")
            with patch.object(RELEASE, "ROOT", root):
                self.assertEqual(RELEASE.version("release/0.1.0"), "0.1.0")
                for branch in ["release/0.2.0", "release/00.1.0", "release/0.1.00", "release/0.1.0-rc1", "develop", "release/0.1.0/extra"]:
                    with self.subTest(branch=branch), self.assertRaises(ValueError):
                        RELEASE.version(branch)


class ReleaseFixture(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="ngen release ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.revision = "a" * 40
        self.files = {"CMakeLists.txt": b"project(NGEN VERSION 0.1.0 LANGUAGES CXX)\n",
                      "LICENSE": b"synthetic source license\n"}
        for name, content in self.files.items():
            (self.root / name).write_bytes(content)
        self.events = []
        self.addCleanup(patch.stopall)
        patch.object(RELEASE, "ROOT", self.root).start()
        patch.object(RELEASE.subprocess, "run", side_effect=self.system).start()

    def system(self, arguments, **kwargs):
        self.events.append(arguments)
        output = ""
        if arguments[0] == "git":
            if arguments[1:3] == ["rev-parse", "HEAD"]:
                output = self.revision + "\n"
            elif arguments[1] == "ls-tree":
                output = "".join("100644 blob " + "b" * 40 + "\t" + name + "\0" for name in self.files)
            elif arguments[1] == "archive":
                destination = Path(next(v.split("=", 1)[1] for v in arguments if v.startswith("--output=")))
                prefix = next(v.split("=", 1)[1] for v in arguments if v.startswith("--prefix="))
                with tarfile.open(destination, "w:gz", format=tarfile.PAX_FORMAT,
                                  pax_headers={"comment": self.revision}) as archive:
                    for name, content in self.files.items():
                        member = tarfile.TarInfo(prefix + name)
                        member.size = len(content)
                        archive.addfile(member, io.BytesIO(content))
            elif arguments[1] != "diff":
                raise AssertionError(arguments)
        else:
            raise AssertionError(arguments)
        return subprocess.CompletedProcess(arguments, 0, output, "")


class SourceRelease(ReleaseFixture):
    def test_package_is_a_versioned_checked_archive_of_committed_source_only(self):
        output = self.root / "dist"
        RELEASE.package("release/0.1.0", output)
        self.assertEqual({p.name for p in output.iterdir()}, {
            "ngen-0.1.0-source.tar.gz", "ngen-0.1.0-source.tar.gz.sha256", "ngen-0.1.0-SHA256SUMS"})
        with tarfile.open(output / "ngen-0.1.0-source.tar.gz") as archive:
            self.assertEqual(archive.pax_headers["comment"], self.revision)
            self.assertEqual(set(archive.getnames()), {"ngen-0.1.0/CMakeLists.txt", "ngen-0.1.0/LICENSE"})
        self.assertTrue(any(call[1:2] == ["archive"] and call[-1] == "HEAD" for call in self.events))

    def test_protected_names_reject_before_archiving_and_owner_output_survives(self):
        self.files[".env"] = b"synthetic fixture, never an actual credential"
        with self.assertRaisesRegex(ValueError, "unsafe source"):
            RELEASE.package("release/0.1.0", self.root / "unsafe")
        self.assertFalse(any(call[1:2] == ["archive"] for call in self.events))
        del self.files[".env"]
        output = self.root / "owner"
        output.write_text("owner data")
        with self.assertRaises(FileExistsError):
            RELEASE.package("release/0.1.0", output)
        self.assertEqual(output.read_text(), "owner data")

    def test_tampering_extra_assets_and_wrong_commit_are_not_verified(self):
        output = self.root / "dist"
        RELEASE.package("release/0.1.0", output)
        with self.assertRaisesRegex(ValueError, "commit mismatch"):
            RELEASE.verify(output, "0.1.0", "b" * 40)
        (output / "foreign").write_bytes(b"not a release payload")
        with self.assertRaisesRegex(ValueError, "inventory"):
            RELEASE.verify(output, "0.1.0", self.revision)
        (output / "foreign").unlink()
        archive = output / RELEASE.assets("0.1.0")[0]
        archive.write_bytes(archive.read_bytes() + b"tampered")
        with self.assertRaisesRegex(ValueError, "checksum"):
            RELEASE.verify(output, "0.1.0", self.revision)


class Publication(ReleaseFixture):
    def setUp(self):
        super().setUp()
        self.state = {"release": None, "tag": None, "remote": {}, "published": False}
        self.output = self.root / "dist"
        RELEASE.package("release/0.1.0", self.output)
        notes = self.root / "docs/releases/0.1.0.md"
        notes.parent.mkdir(parents=True)
        notes.write_text('<p><img src="https://example.invalid/v0.1.0/logo.png" alt="NGEN"></p>\n\n## Highlights\nSource release.\n')
        patch.dict(RELEASE.os.environ, {"GITHUB_REPOSITORY": "fixture/ngen", "RELEASE_COMMIT": self.revision}).start()

    def system(self, arguments, **kwargs):
        def done(output="", code=0):
            self.events.append(arguments)
            if code and kwargs.get("check", True):
                raise subprocess.CalledProcessError(code, arguments, stderr="synthetic boundary failure")
            return subprocess.CompletedProcess(arguments, code, output, "")
        if arguments[0] == "git":
            if arguments[1:3] == ["tag", "--list"]:
                return done("v0.1.0\n" if self.state["tag"] else "")
            if arguments[1:3] == ["cat-file", "-t"]:
                return done(self.state.get("tag_type", "tag") + "\n")
            if arguments[1] == "rev-parse" and arguments[2] != "HEAD":
                return done(self.state["tag"] + "\n")
            if arguments[1] == "config":
                return done()
            if arguments[1:3] == ["tag", "--annotate"] or arguments[5:7] == ["tag", "--annotate"]:
                self.state["tag"] = self.revision
                return done()
            if arguments[1] == "push":
                return done(code=1 if self.state.get("fail_push") else 0)
            return super().system(arguments, **kwargs)
        if arguments[0] == "git-cliff":
            return done("## Changelog\n\n### 0.1.0\n\n#### Features\n- Source fixture.\n", code=1 if self.state.get("fail_notes") else 0)
        if arguments[0] != "gh":
            raise AssertionError(arguments)
        names = [{"name": name, "size": len(content)} for name, content in self.state["remote"].items()]
        if arguments[1] == "api":
            if self.state.get("fail_api"):
                return done(code=1)
            if arguments[2].endswith("/branches/main"):
                return done(json.dumps({"commit": {"sha": self.state.get("main", self.revision)}}))
            rows = [] if self.state["release"] is None else [{"tag_name": "v0.1.0", "draft": self.state["release"] == "draft", "prerelease": False}]
            return done(json.dumps([rows]))
        if arguments[1:3] == ["release", "create"]:
            self.assertIn("--draft", arguments)
            self.state["release"] = "draft"
            return done()
        if arguments[1:3] == ["release", "view"]:
            return done(json.dumps({"isDraft": self.state["release"] == "draft", "isPrerelease": False,
                                    "tagName": "v0.1.0", "assets": names}))
        if arguments[1:3] == ["release", "upload"]:
            for value in arguments:
                path = Path(value)
                if path.is_file():
                    self.state["remote"][path.name] = path.read_bytes()
                    if self.state.pop("fail_upload_once", False):
                        return done(code=1)
            return done()
        if arguments[1:3] == ["release", "download"]:
            destination = Path(arguments[arguments.index("--dir") + 1])
            for name, content in self.state["remote"].items():
                (destination / name).write_bytes(content)
            if self.state.get("corrupt_download"):
                archive = next(destination.glob("*.tar.gz"))
                archive.write_bytes(archive.read_bytes() + b"tampered")
            return done(code=1 if self.state.get("fail_download") else 0)
        if arguments[1:3] == ["release", "edit"]:
            self.assertIn("--draft=false", arguments)
            self.state["release"] = "published"
            self.state["published"] = True
            return done()
        raise AssertionError(arguments)

    def test_source_draft_is_downloaded_and_verified_before_publication(self):
        RELEASE.publish("release/0.1.0", self.output)
        self.assertTrue(self.state["published"])
        calls = [call[1:3] for call in self.events if call[0] == "gh"]
        self.assertLess(calls.index(["release", "upload"]), calls.index(["release", "download"]))
        self.assertLess(calls.index(["release", "download"]), calls.index(["release", "edit"]))
        self.assertEqual(set(self.state["remote"]), set(RELEASE.assets("0.1.0")))

    def mutations(self):
        return [call for call in self.events if call[:2] == ["git", "push"]
                or call[0] == "git" and "--annotate" in call
                or call[0] == "gh" and call[1:3] in [["release", "create"], ["release", "upload"], ["release", "edit"]]]

    def test_failed_preflight_and_notes_do_not_mutate_remote_state(self):
        for flag in ["fail_api", "fail_notes"]:
            with self.subTest(flag=flag):
                self.events.clear()
                self.state[flag] = True
                with self.assertRaises(subprocess.CalledProcessError):
                    RELEASE.publish("release/0.1.0", self.output)
                self.assertEqual(self.mutations(), [])
                self.state[flag] = False
        with patch.dict(RELEASE.os.environ, {"RELEASE_COMMIT": "b" * 40}):
            with self.assertRaisesRegex(ValueError, "exact release merge"):
                RELEASE.publish("release/0.1.0", self.output)
        self.assertEqual(self.mutations(), [])

    def test_an_unmerged_checkout_cannot_be_published_by_setting_the_environment(self):
        self.state["main"] = "b" * 40
        with self.assertRaisesRegex(ValueError, "current main"):
            RELEASE.publish("release/0.1.0", self.output)
        self.assertEqual(self.mutations(), [])

    def test_lightweight_or_conflicting_tags_are_never_moved(self):
        for tag, kind in [("b" * 40, "tag"), (self.revision, "commit")]:
            with self.subTest(kind=kind):
                self.events.clear()
                self.state.update(tag=tag, tag_type=kind)
                with self.assertRaisesRegex(ValueError, "existing tag"):
                    RELEASE.publish("release/0.1.0", self.output)
                self.assertEqual(self.mutations(), [])

    def test_corrupt_download_never_publishes(self):
        self.state["corrupt_download"] = True
        with self.assertRaisesRegex(ValueError, "checksum"):
            RELEASE.publish("release/0.1.0", self.output)
        self.assertEqual(self.state["release"], "draft")
        self.assertFalse(self.state["published"])

    def test_interrupted_upload_resumes_one_draft(self):
        self.state["fail_upload_once"] = True
        with self.assertRaises(subprocess.CalledProcessError):
            RELEASE.publish("release/0.1.0", self.output)
        self.assertEqual(self.state["release"], "draft")
        RELEASE.publish("release/0.1.0", self.output)
        self.assertTrue(self.state["published"])
        self.assertEqual(sum(call[1:3] == ["release", "create"] for call in self.events), 1)
        cliffs = [call for call in self.events if call[0] == "git-cliff"]
        self.assertIn("--unreleased", cliffs[0])
        self.assertIn("--current", cliffs[1])
        self.assertTrue(all("--latest" not in call for call in cliffs))

    def test_published_assets_remain_immutable_even_if_local_rebuild_changes(self):
        RELEASE.publish("release/0.1.0", self.output)
        original = dict(self.state["remote"])
        (self.output / RELEASE.assets("0.1.0")[0]).write_bytes(b"different local rebuild")
        self.events.clear()
        RELEASE.publish("release/0.1.0", self.output)
        self.assertEqual(self.mutations(), [])
        self.assertEqual(self.state["remote"], original)


class BackMerge(ReleaseFixture):
    def setUp(self):
        super().setUp()
        self.existing = []
        self.ahead = 1
        self.login = "kyaulabs-bot"
        self.main_sha = self.revision
        patch.dict(RELEASE.os.environ, {"GITHUB_REPOSITORY": "fixture/ngen"}).start()

    def system(self, arguments, **kwargs):
        if arguments[0] == "git" and arguments[1:3] == ["cat-file", "-t"]:
            self.events.append(arguments)
            return subprocess.CompletedProcess(arguments, 0, "tag\n", "")
        if arguments[0] == "git" and arguments[1] == "rev-parse" and arguments[2] != "HEAD":
            self.events.append(arguments)
            return subprocess.CompletedProcess(arguments, 0, self.revision + "\n", "")
        if arguments[0] != "gh":
            return super().system(arguments, **kwargs)
        self.events.append(arguments)
        if arguments[1] == "api":
            path = arguments[2]
            if path == "user":
                value = {"login": self.login}
            elif path.endswith("/branches/main"):
                value = {"commit": {"sha": self.main_sha}}
            elif "/compare/" in path:
                value = {"ahead_by": self.ahead}
            else:
                raise AssertionError(arguments)
        elif arguments[1:3] == ["release", "view"]:
            value = {"isDraft": False, "isPrerelease": False, "tagName": "v0.1.0",
                     "assets": [{"name": n, "size": 100} for n in RELEASE.assets("0.1.0")]}
        elif arguments[1:3] == ["pr", "list"]:
            value = self.existing
        elif arguments[1:3] == ["pr", "create"]:
            self.assertEqual(arguments[arguments.index("--base") + 1], "develop")
            self.assertEqual(arguments[arguments.index("--head") + 1], "main")
            body = Path(arguments[arguments.index("--body-file") + 1]).read_text()
            self.assertIn("## 🧪 Test Plan", body)
            self.assertIn("- [ ]", body)
            return subprocess.CompletedProcess(arguments, 0, "https://example.invalid/pull/7\n", "")
        else:
            raise AssertionError(arguments)
        return subprocess.CompletedProcess(arguments, 0, json.dumps(value), "")

    def test_published_release_opens_reviewable_bot_backmerge(self):
        RELEASE.back_merge("0.1.0")
        self.assertTrue(any(call[1:3] == ["pr", "create"] for call in self.events))

    def test_existing_or_integrated_backmerge_is_a_noop(self):
        self.existing = [{"number": 7}]
        self.assertIn("#7", RELEASE.back_merge("0.1.0"))
        self.existing, self.ahead = [], 0
        self.assertIn("already integrated", RELEASE.back_merge("0.1.0"))
        self.assertFalse(any(call[1:3] == ["pr", "create"] for call in self.events))

    def test_wrong_actor_or_advanced_main_cannot_open_a_mislabelled_pr(self):
        self.login = "not-the-authorized-bot"
        with self.assertRaisesRegex(ValueError, "bot identity"):
            RELEASE.back_merge("0.1.0")
        self.login, self.main_sha = "kyaulabs-bot", "b" * 40
        with self.assertRaisesRegex(ValueError, "must match"):
            RELEASE.back_merge("0.1.0")
        self.assertFalse(any(call[1:3] == ["pr", "create"] for call in self.events))


class Notes(ReleaseFixture):
    def test_shell_comments_are_not_h1_headers_but_real_headings_are_rejected(self):
        path = self.root / "docs/releases/0.1.0.md"
        path.parent.mkdir(parents=True)
        header = '<p><img src="https://example.invalid/v0.1.0/logo.png" alt="NGEN"></p>\n'
        path.write_text(header + '\n## Start\n\n```sh\n# Shell comment, not a Markdown H1\necho ready\n```\n')
        with patch.object(RELEASE.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, "## Changelog\n", "")):
            self.assertIn("# Shell comment", RELEASE.notes("release/0.1.0"))
            for heading in ["# Unwanted release title", "Unwanted release title\n====", "<h1>Unwanted release title</h1>"]:
                path.write_text(header + "\n" + heading + "\n")
                with self.subTest(heading=heading), self.assertRaisesRegex(ValueError, "H1"):
                    RELEASE.notes("release/0.1.0")


class ReviewRange(unittest.TestCase):
    def test_only_promotions_lint_since_the_accepted_develop_split(self):
        def git(arguments, **kwargs):
            self.assertEqual(arguments[:3], ["git", "merge-base", "origin/develop"])
            return subprocess.CompletedProcess(arguments, 0, "b" * 40 + "\n", "")
        with patch.object(RELEASE.subprocess, "run", side_effect=git):
            self.assertEqual(RELEASE.review_base("c" * 40, "a" * 40), "c" * 40)
            self.assertEqual(RELEASE.review_base("c" * 40, "a" * 40, True), "b" * 40)


class ExactCI(unittest.TestCase):
    def test_missing_run_reaches_deadline_without_permitting_publication(self):
        with patch.dict(RELEASE.os.environ, {"GITHUB_REPOSITORY": "fixture/ngen"}), \
                patch.object(RELEASE.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, "[]", "")), \
                patch.object(RELEASE.time, "monotonic", side_effect=[0, 0, 4501]), \
                patch.object(RELEASE.time, "sleep"):
            with self.assertRaises(TimeoutError):
                RELEASE.wait_ci("a" * 40)

    def test_failed_merge_revision_ci_prevents_publication(self):
        response = json.dumps([{"status": "completed", "conclusion": "failure", "headSha": "a" * 40}])
        with patch.dict(RELEASE.os.environ, {"GITHUB_REPOSITORY": "fixture/ngen"}), \
                patch.object(RELEASE.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, response, "")):
            with self.assertRaisesRegex(ValueError, "CI did not pass"):
                RELEASE.wait_ci("a" * 40)


@unittest.skipUnless(shutil.which("git-cliff"), "Real git-cliff preview runs on the publication/preflight host")
class Changelog(unittest.TestCase):
    def test_first_release_changelog_links_ngen_and_nests_below_the_logo(self):
        tagged = subprocess.run(["git", "tag", "--list", "v0.1.0"], cwd=ROOT,
                                capture_output=True, text=True, check=True, timeout=15).stdout.strip()
        scope = ["--current"] if tagged else ["--unreleased", "--tag", "v0.1.0"]
        result = subprocess.run(
            ["git-cliff", *scope, "--use-branch-tags"],
            cwd=ROOT, capture_output=True, text=True, check=True, timeout=60)
        self.assertNotIn("https://github.com/kyaulabs/template/", result.stdout)
        self.assertIn("https://github.com/kyaulabs/ngen/commit/", result.stdout)
        self.assertIn("###", result.stdout)
        self.assertFalse(re.search(r"^# ", result.stdout, re.MULTILINE), result.stdout[:1000])


if __name__ == "__main__":
    unittest.main()
