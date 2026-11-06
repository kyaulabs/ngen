# Pinned SDL Wayland ownership repair

Linux renderer builds use `sdl/3.4.16@ngen/wayland`; Windows retains upstream `sdl/3.4.16#fe421775c8e1866eb22a3ba23ef8fd62`. The owner explicitly chose a project-local patch after the SDL-only failure was traced to ignored tablet/pad callbacks. This is a temporary dependency repair, not an SDL replacement or an upstream release claim. The CPU graph remains SDL-free.

The recipe is copied from Conan Center revision `fe421775c8e1866eb22a3ba23ef8fd62`, with its options, dependency metadata and packaging behavior retained. Local changes export the notices and patches and apply the patches after fetching the checksum-verified upstream source. The original recipe SHA-256 is `176cdcc8b51c92e9eb96be716b36e11cdda2551fe4e10c6e7ceadf500e45843e`; its original `conandata.yml` SHA-256 is `1b4554d3c776c35c90d0c9acf6308eac19c983931999335ee01eaaa4083969cf`. The recipe's upstream URL identifies that origin. `LICENSE.recipe` preserves Conan Center's MIT notice; `LICENSE.sdl` preserves SDL's Zlib notice. SDL's original notice is also packaged by the recipe. Altered source is labelled in each patch and in the ownership code.

`0001-wayland-tablet-ownership.patch` retains tablet and pad proxies on their seat, owns groups under their pad, unlinks/releases on removal, and drains ownership on seat shutdown. Existing pen/tool handling is unchanged. Ignored rings and strips have no child objects or consumers and are released when received. Allocation failures report SDL's out-of-memory condition and install description listeners that release untracked objects on `done`/`removed`; exhaustive allocation-fault or compositor-disconnect recovery is not claimed. The patch does not disable tablet support or add compensating cleanup to NGEN.

`0002-clang-shared-asan.patch` permits Clang's shared-library sanitizer imports to resolve through the final instrumented executable, as required by [Clang's AddressSanitizer link model](https://clang.llvm.org/docs/AddressSanitizer.html#usage). Normal builds retain `--no-undefined`; ASan/UBSan and fail-fast recovery flags remain enabled in the instrumented build. Conan's sanitizer profile supplies the actual compiler flags; the unrelated upstream `SDL_ASAN` CMake option is not used. Loader checks also run with process-local `LD_BIND_NOW=1`. This is not a warning or leak suppression.

Prepare and run the separate Linux renderer graphs with the native presets:

```sh
sh tools/prepare.sh renderer-debug
cmake --preset renderer-debug
cmake --build --preset renderer-debug
ctest --preset renderer-debug
```

Use `renderer-release` for Release. `renderer-asan` instruments both NGEN and the host Conan dependency libraries, including the patched SDL translation unit. On the tested hardened host, non-PIE executables are needed to avoid ASan shadow-map collisions, including dependency configure probes:

```sh
sh tools/prepare.sh renderer-asan \
  -c:h 'tools.build:exelinkflags=["-fsanitize=address,undefined", "-no-pie"]'
cmake --preset renderer-asan -DCMAKE_EXE_LINKER_FLAGS=-no-pie
cmake --build --preset renderer-asan
ctest --preset renderer-asan
```

These are process/build flags, not persistent system changes or disabled checks. Build tools and host EGL/libdecor/loader/driver libraries are not all rebuilt with sanitizers. Do not reuse a build directory configured with another toolchain. Original CPU presets retain their original behavior. Additional arguments to `tools/prepare.sh` are passed as quoted native Conan install arguments.

The Linux-only tests in `tests/wayland_tablets.cpp` use a private socketpair peer, real SDL and the real Wayland client/server libraries. They require neither a compositor session nor a GPU. The peer checks actual protocol destruction requests for tablet, pad, group, ring and strip objects. The shutdown case rejects premature tablet/pad destruction; the removal case checks cleanup before SDL shutdown as well as exactly-once final counts. The caller retains its externally supplied display long enough to flush and observe destruction, then disconnects it. Actual SDK-owned display cleanup is separately checked by the SDL-only fixture and real renderer probes. The test-only direct Wayland edge uses the already locked 1.24.0 ABI; it adds no new library version to the renderer lock.

`tools/prepare.sh` exports the local recipe before installation. The lock pins its recipe revision, patch bytes, source checksum and transitive graph; no existing cached upstream recipe is edited. CI exports the same source and executes the headless Debug/ASan graphs separately from the default CPU graph. The narrow Git whitespace attribute permits unified-diff blank context syntax, not source-code whitespace errors. System EGL/libdecor/Vulkan integration is not hermetic or byte-identical.

When updating the repair, export the recipe and refresh only its namespaced lock entry using `conan lock remove` followed by `conan lock create --lockfile-partial`, retaining all other pins. Verify both patch application and resulting source bytes, both Windows graphs, the protocol regressions, SDL-only and full GPU startup/calibration, sanitizer diagnostics and ordinary strict linking. Remove this recipe and its namespaced lock entry when a verified upstream release passes those contracts without the local repair. Do not infer a fix from an SDL version number alone; system SDL 3.4.18 also reproduced the original leak.

See [retained verification and limitations](../../docs/evidence/M2/wayland-repair/README.md). M2's frame rendering, presentation, resource retirement and formal acceptance gates remain separate.
