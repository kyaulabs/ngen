#!/bin/sh
set -eu
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
export VK_LAYER_PATH="$PWD/build/validation-layer/usr/share/vulkan/explicit_layer.d"
export LD_LIBRARY_PATH="$PWD/build/validation-layer/usr/lib"
export LD_BIND_NOW=1
: > /tmp/ngen-resource-final-status.txt
for mode in debug release asan gcc; do
  cmake --build "build/renderer-$mode" --parallel 2 > "/tmp/ngen-resource-final-$mode-build.txt" 2>&1
  ctest --test-dir "build/renderer-$mode" --output-on-failure > "/tmp/ngen-resource-final-$mode-tests.txt" 2>&1
  for check in startup calibration presentation resources; do
    case "$check" in
      startup) executable=render_probe; argument=; observer= ;;
      calibration) executable=render_probe; argument=--check-validation-callback; observer= ;;
      presentation) executable=render_probe; argument=--check-presentation; observer="$PWD/build/surface-check/presentation-observer.so:$PWD/build/surface-check/require-sync-validation.so" ;;
      resources) executable=resource_probe; argument=; observer="$PWD/build/resource-check/host-acquire.so:$PWD/build/surface-check/require-sync-validation.so" ;;
    esac
    set --
    if [ -n "$argument" ]; then set -- "$argument"; fi
    timeout 40s env LD_PRELOAD="$observer" "build/renderer-$mode/$executable" "$@" > "/tmp/ngen-resource-final-$mode-$check.txt" 2>&1
    if [ "$check" != calibration ] && grep -E 'ERROR:|LeakSanitizer|runtime error:|VUID-|SYNC-HAZARD|Vulkan validation:' "/tmp/ngen-resource-final-$mode-$check.txt"; then exit 1; fi
    printf '%s %s exit=0\n' "$mode" "$check" | tee -a /tmp/ngen-resource-final-status.txt
  done
done
