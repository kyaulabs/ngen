"""Public replay CLI controls using actual local inputs/tools, without mocks."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("base", "source", "ktx", "probe", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    replay = Path(__file__).with_name("reproduce.py")
    common = [sys.executable, "-O", str(replay), "--base", str(args.base),
              "--source", str(args.source), "--ktx", str(args.ktx), "--probe", str(args.probe)]
    with tempfile.TemporaryDirectory(prefix=".replay-check-", dir=args.output.parent) as directory:
        root = Path(directory)
        existing_file = root / "existing-file"
        existing_file.write_bytes(b"owner-file-preserved")
        existing_dir = root / "existing-directory"
        existing_dir.mkdir()
        marker = existing_dir / "marker"
        marker.write_bytes(b"owner-directory-preserved")
        link = root / "existing-link"
        link.symlink_to(existing_dir, target_is_directory=True)
        for output in (existing_file, existing_dir, link):
            result = subprocess.run([*common, "--output", str(output)],
                                    capture_output=True, text=True, timeout=10)
            assert result.returncode == 1 and "output must be absent" in result.stderr
        assert existing_file.read_bytes() == b"owner-file-preserved"
        assert marker.read_bytes() == b"owner-directory-preserved" and link.is_symlink()
        wrong_source = root / "wrong-source.glb"
        wrong_source.write_bytes(b"not-the-approved-source")
        result = subprocess.run([*common, "--source", str(wrong_source),
                                 "--output", str(root / "rejected")],
                                capture_output=True, text=True, timeout=15)
        assert result.returncode == 1 and "pinned input identity differs" in result.stderr
        assert not (root / "rejected").exists()
        print("PASS existing file/directory/link preservation and source identity rejection (python -O)", flush=True)
    result = subprocess.run([*common, "--output", str(args.output)],
                            capture_output=True, text=True, timeout=120)
    assert result.returncode == 0 and not result.stderr, result.stderr
    print("PASS actual pinned tools/native admission/byte-exact accepted package replay", flush=True)
    print(result.stdout, end="")


if __name__ == "__main__":
    main()
