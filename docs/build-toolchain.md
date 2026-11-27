# Set up the Linux build toolchain

Use this guide from the NGEN source version you intend to build. If you already extracted the checked source archive, stay in that directory; do not switch to a different tag to follow these steps. For a fresh download, choose the version on the [NGEN releases page](https://github.com/kyaulabs/ngen/releases) and follow that release's checksum and extraction instructions.

The primary Linux x86-64 profiles use Clang **23.1.4** with a compatible libstdc++, Python **3.11 or newer** with `venv`, Conan **2.34.0**, CMake **4.4.5**, and Ninja **1.13.2**. Install matching `clang` and `clang++` from your platform's verified packages and make them available on `PATH`. Git and native build tools are also required. The Clang 18 CI compatibility lane uses different setup commands and does not replace these local pins.

Create a Python environment outside the source tree. Install Conan and CMake, not the Ninja wheel:

```sh
venv=$(mktemp -d "$HOME/ngen-venv.XXXXXX")
python3 -m venv "$venv"
. "$venv/bin/activate"
python -m pip install 'conan==2.34.0' 'cmake==4.4.5'
```

Use your installed Ninja if `ninja --version` prints exactly `1.13.2`. Otherwise, on Linux x86-64, install the official [Ninja 1.13.2 Linux binary](https://github.com/ninja-build/ninja/releases/tag/v1.13.2) in a new local directory. This example uses `curl`, `sha256sum` and `unzip`; the checksum is for `ninja-linux.zip` on that release page.

```sh
ninja_dir=$(mktemp -d "$HOME/ngen-ninja.XXXXXX")
curl --fail --location --output "$ninja_dir/ninja-linux.zip" \
  https://github.com/ninja-build/ninja/releases/download/v1.13.2/ninja-linux.zip &&
printf '%s  %s\n' \
  5749cbc4e668273514150a80e387a957f933c6ed3f5f11e03fb30955e2bbead6 \
  "$ninja_dir/ninja-linux.zip" | sha256sum -c - &&
unzip -q "$ninja_dir/ninja-linux.zip" -d "$ninja_dir" &&
export PATH="$ninja_dir:$PATH"
```

Verify the commands this shell will actually run:

```sh
clang++ --version  # clang version 23.1.4
conan --version    # Conan version 2.34.0
cmake --version    # cmake version 4.4.5
command -v ninja
ninja --version    # exactly 1.13.2
```

If `prepare.sh` says `Expected Ninja 1.13.2`, check `PATH`. The PyPI `ninja==1.13.2` wheel can report `1.13.2.git.kitware.jobserver-pipe-1`, which is not the pinned executable. Put the native binary directory before the wheel, and verify again before preparing dependencies. Dependency preparation may need network access; the compiled samples run offline.

Continue with the [SDK guide](creator-sdk.md) or the [creator samples](../samples/README.md).
