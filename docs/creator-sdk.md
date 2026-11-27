# Source-built creator SDK

The SDK, first shipped in 0.2.0, is an opt-in CMake install for a project
that builds against NGEN without adding the flagship or engine checkout to its
build tree. This is the C1-A boundary, not an installable game, a project
editor, or a stable cross-toolchain binary ABI.

To create, cook, run, and source-package a game first, follow the
[board and space docking samples](../samples/README.md). This guide is for
selecting SDK components and linking them from your own CMake project.

With the [primary toolchain](planning/build-review-infrastructure.md) installed,
build a headless SDK and install it into a new, user-owned prefix:

```sh
sh tools/prepare.sh release
cmake --preset release -DNGEN_INSTALL_SDK=ON
cmake --build --preset release
cmake --install build/release --prefix "$PWD/build/sdk-headless"
```

A separate C++20 project can then use the installed package:

```cmake
cmake_minimum_required(VERSION 3.28)
project(MyGame LANGUAGES CXX)
find_package(NGEN CONFIG REQUIRED)
add_executable(my_game main.cpp)
target_link_libraries(my_game PRIVATE NGEN::simulation)
```

Use `#include "simulation/simulation.hpp"` for the fixed-tick simulation API.
The default package exposes `NGEN::core`, `NGEN::jobs`, and `NGEN::simulation`.
It resolves the matching enkiTS and thread dependencies and does not look up SDL,
Vulkan, or VMA. NGEN's headers and libraries relocate together; dependencies are
resolved separately. Configure the consumer using the source build's matching
Conan-generated toolchain, rather than mixing compiler, standard library,
architecture, build type or instrumentation configurations. When upgrading
from the initial C1-A SDK, use a fresh producer build directory: an old
build-root `NGENConfig.cmake` can shadow the installed SDK. Configure the
consumer from the NGEN checkout:

```sh
cmake -S /path/to/my-game -B /path/to/my-game-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/build/release/conan_toolchain.cmake" \
  -DCMAKE_PREFIX_PATH="$PWD/build/sdk-headless"
cmake --build /path/to/my-game-build
```

For graphics, use `tools/prepare.sh renderer-release` and the `renderer-release`
preset with `-DNGEN_INSTALL_SDK=ON`. Install `build/renderer-release` to a separate
fresh prefix and use that build's matching toolchain. Request the renderer with
`find_package(NGEN CONFIG REQUIRED COMPONENTS Renderer)` and link
`NGEN::renderer`. The component exposes the `render/context.hpp` entry point
and its installed surface, resource and mesh interfaces. Only this request
resolves SDL 3.4, Vulkan 1.3 and VMA 3.3. An install without the renderer reports the missing component if required. Windowed execution needs a supported
SDL/Vulkan device and driver; validation-enabled startup needs the Khronos
validation layer. A queued frame does not prove display scan-out.

The optional `Assets` component exposes `NGEN::assets` and `assets/package.hpp`
when the SDK is built with `NGEN_BUILD_ASSETS=ON`. Only a consumer requesting it
resolves zlib. See the [standalone starter](creator-project.md) for a project
that cooks and loads its own scene without linking a renderer.

The install copies static NGEN archives, explicitly listed headers, package
metadata, the project license, and the VMA notice when the renderer is built.
It does not install external dependency packages, internal renderer headers,
flagship code, demo assets, private fonts, tools, or network/MCP services.
Use matching locked dependencies and preserve their notices and the NGEN
[AGPL license](../LICENSE). Keep the corresponding source and its build inputs
when redistributing binaries as required by the applicable terms. Building a game
against this SDK does not grant a proprietary licensing exception.

The external verification fixtures are in [`tests/sdk_consumer`](../tests/sdk_consumer).
After an SDK-enabled build, run `python3 tests/sdk_consumer.py build/release release`
(or `build/renderer-release release --renderer`). The test installs into an owned
temporary prefix, relocates it, configures separate consumers and builds/runs the
public APIs with display variables absent. The graphical test exercises linking
and surface selection without needing a GPU; running its consumer with
`--first-light` additionally exercises native window, frame queue and teardown.
Linux x86-64/Clang evidence is separate from any unverified Windows SDK or ABI
claim. See [execution order](planning/execution-order.md) for the next creator task.
