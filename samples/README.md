# Creator SDK samples

- **Board (non-space):** the standalone starter loads a cooked marker and moves a piece 42 mm. [`board/layout.json`](board/layout.json) places the board at X = 1 m for the owned playtest.
- **Docking (space):** [`docking/main.cpp`](docking/main.cpp) replaces the starter's rules. A freighter advances through two simulation ticks toward a berth at 200 mm. It checks one cooked beacon and the simulated position; it does not use the flagship flight code or model physical docking.

Both samples are headless Linux C++20 projects. Neither needs graphics, accounts, private assets, or a game server. The initial dependency preparation may need the network.

## Create, build and run

Follow the [Linux toolchain setup](../docs/build-toolchain.md) in this source
release. From its root, in the same shell, prepare the SDK and tools once:

```sh
sh tools/prepare.sh asset-release
cmake --preset asset-release -DNGEN_INSTALL_SDK=ON
cmake --build --preset asset-release

workspace=$(mktemp -d)
engine_build="$PWD/build/asset-release"
cmake --install "$engine_build" --prefix "$workspace/sdk"
ktx=$(awk -F= '/^NGEN_KTX_EXECUTABLE:/{sub(/^[^=]*=/, ""); print; exit}' \
  "$engine_build/CMakeCache.txt")
test -x "$ktx"
```

Create both projects with the supported CLI, then apply their sample inputs. Use a new workspace when repeating these commands.

```sh
python3 tools/project.py new "$workspace/board" --name board
python3 tools/project.py scene edit "$workspace/board" --expect 0 < samples/board/layout.json
python3 tools/project.py new "$workspace/docking" --name docking
cp samples/docking/main.cpp "$workspace/docking/main.cpp"

for sample in board docking; do
  project="$workspace/$sample"
  cooked="$workspace/$sample-cooked"
  out="$workspace/$sample-build"
  python3 tools/project.py validate "$project"
  python3 tools/assetc/cook.py \
    --assetc "$engine_build/assetc" --ktx "$ktx" \
    --probe "$engine_build/asset_package_probe" \
    --source "$project/marker.glb" --output "$cooked"
  cmake -S "$project" -B "$out" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$engine_build/conan_toolchain.cmake" \
    -DCMAKE_PREFIX_PATH="$workspace/sdk" \
    -DNGEN_STARTER_SCENE="$cooked/scene.ngas"
  cmake --build "$out"
  ctest --test-dir "$out" --output-on-failure --no-tests=error
  "$out/ngen_starter" "$cooked/scene.ngas"
  python3 tools/project.py package "$project" --output "$workspace/$sample-source.tar.gz"
done
```

Board reports one marker and a 42 mm move. Docking reports an approach at 125 mm on tick 1 and capture at 200 mm on tick 2. The compiled board starter reads the cooked mesh, not the saved board transform. To play the saved layout, [start an owned board session](../docs/creator-play.md) using `$workspace/board`, revision `1`, and `assetc`, `asset_package_probe` and `creator_player` from `$engine_build`. With the layout unchanged it reports `marker_mm: 1042`. That session runs the fixed board rules, not the docking executable or arbitrary project code.

To modify rules, edit a project's `main.cpp` and rebuild its existing build directory. To modify the board position without rebuilding, [edit the saved scene](../docs/creator-scene.md) and reload the new revision in the owned session. To rebuild a packaged sample, unpack its source archive into a fresh directory, use the extracted project as `project`, and repeat the validate/cook/configure/build/test/run steps with fresh output directories. Cooked files and binaries are not in the archives. See the [SDK guide](../docs/creator-sdk.md) for component selection and matching toolchains.

## Redistribution

The sample source, layout and original NGEN starter triangle (`marker.glb`) are covered by the repository's [AGPL license](../LICENSE). The generated source packages retain `LICENSE`, the marker, project code, and any saved board layout. They do not include third-party libraries, tools, or private assets. Keep the NGEN license and corresponding source and preserve applicable dependency notices if redistributing built binaries. Keep that distinction when substituting content: packaging a file does not grant redistribution rights to it.
