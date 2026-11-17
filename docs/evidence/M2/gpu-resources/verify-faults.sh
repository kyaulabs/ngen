#!/bin/sh
set -eu
: > /tmp/ngen-resource-fault-status.txt
run() {
  name=$1; library=$2; shift 2
  log="/tmp/ngen-resource-fault-$name.txt"
  set +e
  timeout 35s env VK_LAYER_PATH="$PWD/build/validation-layer/usr/share/vulkan/explicit_layer.d" LD_LIBRARY_PATH="$PWD/build/validation-layer/usr/lib" LD_BIND_NOW=1 ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 LD_PRELOAD="$PWD/build/resource-check/$library:$PWD/build/surface-check/require-sync-validation.so" "$@" build/renderer-asan/resource_probe > "$log" 2>&1
  code=$?
  set -e
  printf '%s exit=%s expected=1\n' "$name" "$code" | tee -a /tmp/ngen-resource-fault-status.txt
  test "$code" -eq 1
  if grep -E 'ERROR:|LeakSanitizer|runtime error:|VUID-|SYNC-HAZARD|Vulkan validation:' "$log"; then exit 1; fi
}
for fault in memory:1 buffer:1 image:1 commands:1 commands:2 bind-buffer:1 bind-image:1 submit:1 submit:2; do
  api=${fault%:*}; ordinal=${fault#*:}
  run "$api-$ordinal" native-failure-asan.so NGEN_RESOURCE_FAULT_API="$api" NGEN_RESOURCE_FAULT_AT="$ordinal"
  grep -q "injected_resource_failure=$api:" "/tmp/ngen-resource-fault-$api-$ordinal.txt"
done
for fault in pool:1 pool:2 pool:3 semaphore:2 view:1 view:2; do
  api=${fault%:*}; ordinal=${fault#*:}
  run "$api-$ordinal" allocation-failure-asan.so NGEN_FAULT_API="$api" NGEN_FAULT_AT="$ordinal"
  grep -q "injected_allocation_failure=$api:" "/tmp/ngen-resource-fault-$api-$ordinal.txt"
done
for mode in admission-buffer admission-image image-unsupported host-wait-failure idle-retry; do
  run "$mode" "$mode-asan.so"
done
run image-limits image-limits.so
grep -q 'preflight_queries=1 native_buffers=0' /tmp/ngen-resource-fault-admission-buffer.txt
grep -q 'preflight_queries=1 native_images=0' /tmp/ngen-resource-fault-admission-image.txt
grep -q 'image_format_queries=1 native_images=0' /tmp/ngen-resource-fault-image-unsupported.txt
grep -q 'image_format_queries=1 native_images=0' /tmp/ngen-resource-fault-image-limits.txt
grep -q 'acquire completed resources: VkResult -13' /tmp/ngen-resource-fault-host-wait-failure.txt
if grep -q 'destroyed_before_retry=1' /tmp/ngen-resource-fault-idle-retry.txt; then exit 1; fi
