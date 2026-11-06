"""Generate synthetic runtime fixtures through the pinned offline KTX CLI."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ktx", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--linear", action="store_true")
    args = parser.parse_args()
    if args.output.exists() or args.output.is_symlink():
        raise ValueError("fixture output must not already exist")
    with tempfile.TemporaryDirectory(prefix="ngen KTX fixture ") as directory:
        root = Path(directory)
        base = root / "base.rgba"
        mip = root / "mip.rgba"
        base.write_bytes(bytes([255, 0, 0, 255, 0, 255, 0, 255,
                               0, 0, 255, 255, 255, 255, 255, 255]))
        # A half-linear-intensity sRGB mip, independently specified.
        value = 128 if args.linear else 188
        mip.write_bytes(bytes([value, value, value, 255]))
        output = root / "fixture.ktx2"
        image_format = "R8G8B8A8_UNORM" if args.linear else "R8G8B8A8_SRGB"
        transfer = "linear" if args.linear else "srgb"
        subprocess.run([args.ktx, "create", "--format", image_format, "--raw",
                        "--width", "2", "--height", "2", "--levels", "2",
                        "--assign-tf", transfer, "--assign-primaries", "bt709",
                        "--assign-texcoord-origin", "top-left",
                        str(base), str(mip), str(output)],
                       check=True, capture_output=True, timeout=30)
        subprocess.run([args.ktx, "validate", str(output)],
                       check=True, capture_output=True, timeout=30)
        if output.stat().st_size > 4096:
            raise ValueError("unexpected synthetic fixture size")
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open("xb") as target, output.open("rb") as source:
            shutil.copyfileobj(source, target)


if __name__ == "__main__":
    main()
