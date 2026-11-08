#!/bin/sh
set -eu
preset=${1:-debug}
if [ "$#" -gt 0 ]; then shift; fi
profile=linux-clang
type=Debug
renderer=false
asset_tools=false
assets=false
scene=false
inspector=false
case "$preset" in
  debug) ;;
  asan) profile=linux-asan ;;
  tsan) profile=linux-tsan ;;
  release) type=Release ;;
  gcc) profile=linux-gcc ;;
  renderer-debug) renderer=true ;;
  renderer-release) renderer=true; type=Release ;;
  renderer-asan) renderer=true; profile=linux-asan ;;
  asset-debug) asset_tools=true ;;
  asset-release) asset_tools=true; type=Release ;;
  asset-asan) asset_tools=true; profile=linux-asan ;;
  asset-gcc) asset_tools=true; profile=linux-gcc ;;
  asset-runtime-debug) assets=true ;;
  scene-debug) scene=true ;;
  scene-release) scene=true; type=Release ;;
  scene-asan) scene=true; profile=linux-asan ;;
  scene-gcc) scene=true; profile=linux-gcc ;;
  inspector-debug) scene=true; inspector=true ;;
  inspector-release) scene=true; inspector=true; type=Release ;;
  inspector-asan) scene=true; inspector=true; profile=linux-asan ;;
  inspector-gcc) scene=true; inspector=true; profile=linux-gcc ;;
  *) echo "Usage: sh tools/prepare.sh {debug|release|gcc|asan|tsan|renderer-debug|renderer-release|renderer-asan|asset-debug|asset-release|asset-asan|asset-gcc|asset-runtime-debug|scene-debug|scene-release|scene-asan|scene-gcc|inspector-debug|inspector-release|inspector-asan|inspector-gcc} [Conan install options]" >&2; exit 2 ;;
esac
[ "$(cmake --version | head -1)" = 'cmake version 4.4.5' ] || { echo 'Expected CMake 4.4.5' >&2; exit 2; }
[ "$(ninja --version)" = '1.13.2' ] || { echo 'Expected Ninja 1.13.2' >&2; exit 2; }
if [ "$profile" = linux-gcc ]; then
  [ "$(g++ -dumpversion)" = '16' ] || { echo 'Expected GCC 16; see profiles/linux-gcc' >&2; exit 2; }
else
  clang++ --version | grep -q 'clang version 23.1.4' || { echo 'Expected Clang 23.1.4; see profiles/linux-clang' >&2; exit 2; }
fi
command -v conan >/dev/null || { echo 'Conan 2.34.0 required; install it in a virtualenv and add its bin directory to PATH.' >&2; exit 2; }
[ "$(conan --version)" = 'Conan version 2.34.0' ] || { echo 'Expected Conan 2.34.0' >&2; exit 2; }
if [ "$scene" = true ]; then
  lockfile=conan-scene.lock
  inspector_option=False
  if [ "$inspector" = true ]; then
    lockfile=conan-inspector.lock
    inspector_option=True
  fi
  conan export recipes/sdl --version=3.4.16 --user=ngen --channel=wayland
  conan export recipes/slangc --version=2026.21 --user=ngen --channel=tools
  conan install . -pr:h "profiles/$profile" -pr:b profiles/linux-clang -s:h "build_type=$type" -s:b build_type=Release -o '&:renderer=True' -o '&:shaders=True' -o '&:assets=True' -o "&:inspector=$inspector_option" --lockfile "$lockfile" --build=missing -c tools.build:jobs=2 -of "build/$preset" "$@"
elif [ "$asset_tools" = true ]; then
  conan export recipes/mikktspace --version=cci.20200325 --user=ngen --channel=defined-rotate
  conan install . -pr:h "profiles/$profile" -pr:b profiles/linux-clang -s:h "build_type=$type" -s:b build_type=Release -s:b compiler.cppstd=17 -o '&:asset_tools=True' --lockfile conan-assets.lock --build=missing -c tools.build:jobs=2 -of "build/$preset" "$@"
elif [ "$assets" = true ]; then
  conan install . -pr:h "profiles/$profile" -pr:b profiles/linux-clang -s:h "build_type=$type" -o '&:assets=True' --lockfile conan-runtime-assets.lock --build=missing -c tools.build:jobs=2 -of "build/$preset" "$@"
elif [ "$renderer" = true ]; then
  conan export recipes/sdl --version=3.4.16 --user=ngen --channel=wayland
  conan export recipes/slangc --version=2026.21 --user=ngen --channel=tools
  conan install . -pr:h "profiles/$profile" -pr:b profiles/linux-clang -s:h "build_type=$type" -s:b build_type=Release -o '&:renderer=True' -o '&:shaders=True' --lockfile conan-render.lock --build=missing -c tools.build:jobs=2 -of "build/$preset" "$@"
else
  conan install . -pr:h "profiles/$profile" -pr:b profiles/linux-clang -s:h "build_type=$type" --lockfile conan.lock --build=missing -of "build/$preset" "$@"
fi
