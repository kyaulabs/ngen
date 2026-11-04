#!/bin/sh
set -eu
preset=${1:-debug}
profile=linux-clang
type=Debug
case "$preset" in
  debug) ;;
  asan) profile=linux-asan ;;
  tsan) profile=linux-tsan ;;
  release) type=Release ;;
  gcc) profile=linux-gcc ;;
  *) echo "Usage: sh tools/prepare.sh {debug|release|gcc|asan|tsan}" >&2; exit 2 ;;
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
conan install . -pr:h "profiles/$profile" -pr:b profiles/linux-clang -s:h "build_type=$type" --lockfile conan.lock --build=missing -of "build/$preset"
