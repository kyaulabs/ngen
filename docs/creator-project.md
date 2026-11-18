# Standalone starter project

The C1-B workflow on `develop` creates one small headless C++20 project. It loads
an offline-cooked board marker and runs a fixed-tick movement test using the
[installed SDK](creator-sdk.md). It does not need a window, GPU, private assets,
or any flagship logic. This is a Linux development workflow, not a project
editor, graphical sample, binary installer, or MCP execution environment.

From the NGEN checkout, prepare the primary Clang toolchain as described in the
[SDK guide](creator-sdk.md), then build the existing asset tools and install the
SDK from the same dependency graph:

```sh
sh tools/prepare.sh asset-release
cmake --preset asset-release -DNGEN_INSTALL_SDK=ON
cmake --build --preset asset-release

workspace=$(mktemp -d)
engine_build="$PWD/build/asset-release"
cmake --install "$engine_build" --prefix "$workspace/sdk"
```

Use the `NGEN_KTX_EXECUTABLE` path reported by that configuration (the pinned
KTX build tool). You can get the configured value without searching the Conan
cache:

```sh
ktx=$(awk -F= '/^NGEN_KTX_EXECUTABLE:/{sub(/^[^=]*=/, ""); print; exit}' \
  "$engine_build/CMakeCache.txt")
test -x "$ktx"
```

Create and validate a fresh project, then use the existing offline cooker and
build commands. The initial dependency build may need the network; the commands
below use already built local tools and an installed SDK.

```sh
python3 tools/project.py new "$workspace/project" --name board-starter
python3 tools/project.py validate "$workspace/project"
python3 tools/assetc/cook.py \
  --assetc "$engine_build/assetc" --ktx "$ktx" \
  --probe "$engine_build/asset_package_probe" \
  --source "$workspace/project/marker.glb" --output "$workspace/cooked"

cmake -S "$workspace/project" -B "$workspace/build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$engine_build/conan_toolchain.cmake" \
  -DCMAKE_PREFIX_PATH="$workspace/sdk" \
  -DNGEN_STARTER_SCENE="$workspace/cooked/scene.ngas"
cmake --build "$workspace/build"
ctest --test-dir "$workspace/build" --output-on-failure --no-tests=error
"$workspace/build/ngen_starter" "$workspace/cooked/scene.ngas"

python3 tools/project.py package "$workspace/project" --output "$workspace/starter.tar.gz"
```

The program reports a loaded board marker and a 42 mm simulated move. The
supplied marker is the original test triangle from `tests/asset_cli.py`, not a
ship asset. `Assets` is an explicit SDK component with the matching zlib build;
the headless default still does not pull in asset or graphics dependencies.

`ngen-project.json` accepts exactly `schema: 1` and a `name` slug (a lowercase
letter, followed by up to 39 lowercase letters, digits, or hyphens). Project
operations report JSON success on stdout or failure on stderr with a nonzero
exit code. Validation checks the manifest, bounded regular root files, the
original license, and the GLB container header. The cooker and the runtime
loader, not the manifest validator, verify the scene contents. Validation and
packaging do not run project code. Only build or run a project you trust.

Use separate build and cook directories for new outputs. The cooker rejects
nonempty output; `package` publishes a new archive and refuses to replace an
existing file. The source archive contains the manifest, CMake file, C++ source,
original GLB and AGPL license, but no cooked package, executable, dependency,
cache or other project file. A recipient needs a matching local SDK and cooker
to repeat the steps. Packaging does not grant redistribution rights to inputs
you substitute. Use an owned, non-concurrently edited workspace; this is not a
filesystem sandbox or a service for untrusted tenants.
