"""Source-only NGEN release automation; never used by the engine runtime."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import tarfile
import tempfile
import time
import sys
from pathlib import PurePosixPath

from package_demo import private_path, regular, sha256

ROOT = Path(__file__).resolve().parents[1]


SEMVER = r"(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)"


def version(branch):
    if not isinstance(branch, str) or len(branch) > 80 or not re.fullmatch("release/" + SEMVER, branch):
        raise ValueError("expected canonical release/X.Y.Z branch")
    values = re.findall(r"\bproject\s*\(\s*NGEN\s+VERSION\s+([0-9.]+)\s+LANGUAGES\s+CXX\s*\)",
                        regular(ROOT / "CMakeLists.txt", 1024**2, root=ROOT).read_text())
    if values != [branch.removeprefix("release/")]:
        raise ValueError("CMake version does not match release branch")
    return values[0]


def command(arguments):
    return subprocess.run(arguments, cwd=ROOT, check=True, capture_output=True,
                          text=True, timeout=60).stdout


def canonical(value):
    if not isinstance(value, str) or len(value) > 64 or not re.fullmatch(SEMVER, value):
        raise ValueError("expected canonical X.Y.Z version")
    return value


def assets(value):
    value = canonical(value)
    name = f"ngen-{value}-source.tar.gz"
    return [name, name + ".sha256", f"ngen-{value}-SHA256SUMS"]


def source_name(name):
    path = PurePosixPath(name)
    if (not name or len(name) > 512 or path.is_absolute() or ".." in path.parts
            or "\\" in name or path.as_posix() != name or private_path(path)
            or any(private_path(PurePosixPath(part)) for part in path.parts)):
        raise ValueError("unsafe source member")
    return path


def revision():
    value = command(["git", "rev-parse", "HEAD"]).strip()
    if not re.fullmatch(r"[0-9a-f]{40}", value):
        raise ValueError("expected full source commit")
    return value


def clean():
    command(["git", "diff", "--quiet"])
    command(["git", "diff", "--cached", "--quiet"])


def verify(directory, value, commit):
    names = assets(value)
    if directory.is_symlink() or {p.name for p in directory.iterdir()} != set(names):
        raise ValueError("unexpected release asset inventory")
    for name in names:
        regular(directory / name, 128 * 1024**2 if name == names[0] else 4096)
    expected = f"{sha256(directory / names[0])}  {names[0]}\n"
    if any((directory / name).read_text() != expected for name in names[1:]):
        raise ValueError("source checksum mismatch")
    with tarfile.open(directory / names[0], "r:gz") as archive:
        if archive.pax_headers.get("comment") != commit:
            raise ValueError("source archive commit mismatch")
        seen, total = set(), 0
        for member in archive:
            path = source_name(member.name.rstrip("/"))
            if path.parts[0] != f"ngen-{value}" or not (member.isfile() or member.isdir()):
                raise ValueError("unsafe release archive entry")
            key = path.as_posix().casefold()
            total += member.size
            if key in seen or len(seen) >= 8192 or total > 128 * 1024**2:
                raise ValueError("source archive bounds or duplicate name")
            seen.add(key)
        if not {f"ngen-{value}/cmakelists.txt", f"ngen-{value}/license"} <= seen:
            raise ValueError("incomplete source archive")
    return names


def package(branch, output):
    value = version(branch)
    commit = revision()
    clean()
    entries = command(["git", "ls-tree", "-r", "-z", "--full-tree", "HEAD"]).split("\0")
    if not 1 <= len(entries) - 1 <= 4096:
        raise ValueError("source entry count exceeds bound")
    for entry in filter(None, entries):
        metadata, name = entry.split("\t", 1)
        if metadata.split()[0] not in {"100644", "100755"}:
            raise ValueError("source archive cannot include links or submodules")
        source_name(name)
    output = Path(output).absolute()
    if output.exists() or output.is_symlink():
        raise FileExistsError("release output must be fresh")
    if any(p.is_symlink() or private_path(p) for p in (output, *output.parents)):
        raise ValueError("unsafe release output path")
    output.parent.mkdir(parents=True, exist_ok=True)
    names = assets(value)
    with tempfile.TemporaryDirectory(prefix="ngen-release-", dir=output.parent) as temporary:
        stage = Path(temporary)
        command(["git", "archive", "--format=tar.gz", "--prefix=" + f"ngen-{value}/",
                 "--output=" + str(stage / names[0]), "HEAD"])
        checksum = f"{sha256(regular(stage / names[0], 128 * 1024**2))}  {names[0]}\n"
        for name in names[1:]:
            (stage / name).write_text(checksum)
        verify(stage, value, commit)
        output.mkdir()  # Exclusive admission preserves existing owner data.
        for name in names:
            (stage / name).rename(output / name)
    return commit


def repository():
    value = os.environ.get("GITHUB_REPOSITORY", "")
    if (not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9-]{0,38}/[A-Za-z0-9_.-]{1,100}", value)
            or value.split("/")[-1] in {".", ".."}):
        raise ValueError("expected owner/repository")
    return value


def api(path):
    return json.loads(command(["gh", "api", path]))


def notes(branch, tagged=False):
    value = version(branch)
    overview = regular(ROOT / "docs/releases" / (value + ".md"), 1024**2, root=ROOT).read_text()
    if not overview.strip() or "<img " not in overview.splitlines()[0] or "X.Y.Z" in overview:
        raise ValueError("release overview needs its version-pinned logo and filled template")
    arguments = ["git-cliff", "--current" if tagged else "--unreleased", "--use-branch-tags"]
    if not tagged:
        arguments += ["--tag", "v" + value]
    result = overview.rstrip() + "\n\n" + command(arguments)
    fence = None
    previous_text = False
    for line in result.splitlines():
        marker = re.match(r"^\s*(`{3,}|~{3,})", line)
        if marker:
            previous_text = False
            delimiter = marker[1]
            if fence is None:
                fence = delimiter
            elif delimiter[0] == fence[0] and len(delimiter) >= len(fence):
                fence = None
        elif fence is None:
            if (re.match(r"^#(?:\s|$)", line) or re.search(r"<h1(?:\s|>)", line, re.IGNORECASE)
                    or previous_text and re.fullmatch(r"\s*=+\s*", line)):
                raise ValueError("release description must have no H1 below its logo")
            previous_text = bool(line.strip())
    return result


def remote_inventory(repo, tag, value, *, complete):
    data = json.loads(command(["gh", "release", "view", tag, "--repo", repo,
                               "--json", "isDraft,isPrerelease,tagName,assets"]))
    if data["tagName"] != tag or data["isPrerelease"] or not isinstance(data["isDraft"], bool):
        raise ValueError("unexpected release identity or prerelease")
    expected = assets(value)
    remote = data["assets"]
    names = [item["name"] for item in remote]
    if (len(names) != len(set(names)) or not set(names) <= set(expected)
            or complete and set(names) != set(expected)):
        raise ValueError("unexpected remote release inventory")
    for item in remote:
        limit = 128 * 1024**2 if item["name"] == expected[0] else 4096
        if not isinstance(item["size"], int) or not 0 < item["size"] <= limit:
            raise ValueError("remote asset exceeds bound")
    return data


def publish(branch, directory):
    value = version(branch)
    repo, commit = repository(), revision()
    if commit != os.environ.get("RELEASE_COMMIT"):
        raise ValueError("checkout does not match exact release merge")
    clean()
    tag = "v" + value
    tagged = bool(command(["git", "tag", "--list", tag]).strip())
    if tagged and (command(["git", "cat-file", "-t", "refs/tags/" + tag]).strip() != "tag"
                   or command(["git", "rev-parse", f"refs/tags/{tag}^{{commit}}"]).strip() != commit):
        raise ValueError("existing tag is lightweight or points at another commit")
    pages = json.loads(command(["gh", "api", "--paginate", "--slurp", f"repos/{repo}/releases"]))
    releases = [item for page in pages for item in page if item["tag_name"] == tag]
    if len(releases) > 1 or releases and releases[0]["prerelease"]:
        raise ValueError("ambiguous release or prerelease")
    published = bool(releases and not releases[0]["draft"])
    if releases and not tagged:
        raise ValueError("existing release has no matching fetched annotated tag")
    if not published and api(f"repos/{repo}/branches/main")["commit"]["sha"] != commit:
        raise ValueError("unpublished release must use the exact current main merge")
    names = assets(value)
    with tempfile.TemporaryDirectory(prefix="ngen-publication-") as temporary:
        work = Path(temporary)
        if not published:
            verify(directory, value, commit)
            description = work / "notes.md"
            description.write_text(notes(branch, tagged))
            if not tagged:
                command(["git", "-c", "user.name=github-actions[bot]", "-c",
                         "user.email=41898282+github-actions[bot]@users.noreply.github.com",
                         "tag", "--annotate", tag, "--message", f"NGEN {tag}"])
            command(["git", "push", "origin", "refs/tags/" + tag])
            if not releases:
                command(["gh", "release", "create", tag, "--repo", repo, "--draft",
                         "--verify-tag", "--title", f"NGEN {tag}", "--notes-file", str(description)])
            if not remote_inventory(repo, tag, value, complete=False)["isDraft"]:
                raise ValueError("release no longer draft; refusing asset replacement")
            command(["gh", "release", "upload", tag, "--repo", repo, "--clobber",
                     *[str(directory / name) for name in names]])
        remote_inventory(repo, tag, value, complete=True)
        download = work / "download"
        download.mkdir()
        arguments = ["gh", "release", "download", tag, "--repo", repo, "--dir", str(download)]
        for name in names:
            arguments += ["--pattern", name]
        command(arguments)
        verify(download, value, commit)
        if not published:
            if any(sha256(directory / name) != sha256(download / name) for name in names):
                raise ValueError("draft bytes differ from validated input")
            if not remote_inventory(repo, tag, value, complete=True)["isDraft"]:
                raise ValueError("release changed during verification")
            command(["gh", "release", "edit", tag, "--repo", repo, "--draft=false"])


def back_merge(value):
    value = canonical(value)
    version("release/" + value)
    repo, tag = repository(), "v" + value
    if api("user").get("login") != "kyaulabs-bot":
        raise ValueError("back-merge requires the authorized bot identity")
    if remote_inventory(repo, tag, value, complete=True)["isDraft"]:
        raise ValueError("back-merge requires a published release")
    main = api(f"repos/{repo}/branches/main")["commit"]["sha"]
    if (not re.fullmatch(r"[0-9a-f]{40}", main) or revision() != main
            or command(["git", "cat-file", "-t", "refs/tags/" + tag]).strip() != "tag"
            or command(["git", "rev-parse", f"refs/tags/{tag}^{{commit}}"]).strip() != main):
        raise ValueError("main checkout and annotated release tag must match")
    ahead = api(f"repos/{repo}/compare/develop...main")["ahead_by"]
    if not isinstance(ahead, int) or ahead < 0:
        raise ValueError("invalid branch comparison")
    if ahead == 0:
        return "main is already integrated into develop"
    existing = json.loads(command(["gh", "pr", "list", "--repo", repo, "--base", "develop",
                                   "--head", "main", "--state", "open", "--json", "number"]))
    if len(existing) > 1:
        raise ValueError("ambiguous open back-merge pull requests")
    if existing:
        return f"back-merge PR #{existing[0]['number']} already exists"
    body = f"""## 📋 Summary

Back-merge published NGEN {tag} from `main` into `develop` to retain release history.

## 📦 Changes by Phase

Preserve the reviewed release merge `{main}` and versioned release metadata.

## 📜 ADRs

No new architecture decision.

## ✅ Verification

Published release inventory, bot identity, annotated tag and exact main checkout validated.
Source/checksum byte verification was performed by the publication job, not inferred here.

## 📝 Commits

Preserve signed history with an ordinary reviewed merge; do not squash/rebase or delete main.

## 🧪 Test Plan

- [x] Confirm published {tag}, expected source inventory and matching annotated tag/main commit.
- [ ] Verify current head `{main}`, diff, conflicts and all applicable CI before review.
- [ ] Review/approve as kyau, then merge as kyaulabs-bot and watch post-merge checks.
"""
    with tempfile.TemporaryDirectory(prefix="ngen-backmerge-") as temporary:
        path = Path(temporary) / "pr.md"
        path.write_text(body)
        return command(["gh", "pr", "create", "--repo", repo, "--base", "develop", "--head", "main",
                        "--reviewer", "kyau", "--title", f"chore(release): back-merge {tag} into develop",
                        "--body-file", str(path)]).strip()


def review_base(base, head, promotion=False):
    if not re.fullmatch(r"[0-9a-f]{40}", head):
        raise ValueError("expected reviewed head commit")
    if promotion or not base or base == "0" * 40:
        base = command(["git", "merge-base", "origin/develop", head]).strip()
    if not re.fullmatch(r"[0-9a-f]{40}", base):
        raise ValueError("expected reviewed base commit")
    return base


def wait_ci(commit):
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise ValueError("expected exact release commit")
    repo = repository()
    deadline = time.monotonic() + 75 * 60
    while time.monotonic() < deadline:
        runs = json.loads(command(["gh", "run", "list", "--repo", repo, "--workflow", "cpu.yml",
                                   "--event", "push", "--commit", commit, "--limit", "20",
                                   "--json", "status,conclusion,headSha"]))
        if any(run.get("headSha") != commit for run in runs):
            raise ValueError("CI revision mismatch")
        if runs and all(run["status"] == "completed" for run in runs):
            if any(run["conclusion"] != "success" for run in runs):
                raise ValueError("exact merge CI did not pass")
            return
        time.sleep(30)
    raise TimeoutError("exact merge CI deadline exceeded; publication not permitted")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    for name in ["validate", "package", "notes", "publish", "verify"]:
        child = commands.add_parser(name)
        child.add_argument("branch")
        if name == "package":
            child.add_argument("--output", type=Path, required=True)
        if name in {"publish", "verify"}:
            child.add_argument("--directory", type=Path, required=True)
        if name == "verify":
            child.add_argument("--commit", required=True)
    child = commands.add_parser("back-merge")
    child.add_argument("version")
    child = commands.add_parser("wait-ci")
    child.add_argument("commit")
    child = commands.add_parser("review-base")
    child.add_argument("--base", default="")
    child.add_argument("--head", required=True)
    child.add_argument("--promotion", action="store_true")
    arguments = parser.parse_args()
    try:
        if arguments.command == "validate":
            print(version(arguments.branch))
        elif arguments.command == "package":
            print(package(arguments.branch, arguments.output))
        elif arguments.command == "notes":
            tag = "v" + version(arguments.branch)
            print(notes(arguments.branch, bool(command(["git", "tag", "--list", tag]).strip())))
        elif arguments.command == "verify":
            verify(arguments.directory, version(arguments.branch), arguments.commit)
        elif arguments.command == "publish":
            publish(arguments.branch, arguments.directory)
        elif arguments.command == "back-merge":
            print(back_merge(arguments.version))
        elif arguments.command == "wait-ci":
            wait_ci(arguments.commit)
        else:
            print(review_base(arguments.base, arguments.head, arguments.promotion))
    except (ValueError, OSError, subprocess.SubprocessError, tarfile.TarError) as error:
        print(f"release: {error}", file=sys.stderr)
        raise SystemExit(1) from None


if __name__ == "__main__":
    main()
