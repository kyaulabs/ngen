# M5-F bounded quest rule boundary

The opt-in `ngen_quest_script` target evaluates trusted first-party Lua 5.5.1 **text** in a fresh VM. A rule sees two scalar facts and may propose one integer in `0..255`. The caller must still validate the proposal. There are no standard libraries, host callbacks, external bytecode, or mutable game objects. The adapter is not an untrusted-script sandbox.

The source is limited to 4 KiB. By default, an evaluation admits up to 4,096 VM count-hook events and 64 KiB of retained allocator requests, including block headers. The caller may select limits within the adapter's hard bounds. This is not a bound on RSS, allocator metadata, compiler instruction count, or wall time. No Lua call can invoke blocking native game work. A protected C callback has only trivial live locals across Lua errors; a C++ owner outside that call closes the VM on every return path.

Build with the existing CMake toolchain and a `pkg-config` provider for the exact Lua 5.5.1 C library:

```sh
cmake --preset release -B build/quest-release \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/build/release/conan_toolchain.cmake" \
  -DNGEN_BUILD_QUEST_SCRIPT=ON
cmake --build build/quest-release --target quest_script_tests
ctest --test-dir build/quest-release -R '^quest_script$' --output-on-failure --no-tests=error
```

Linux x86-64 with Clang 23.1.5, CMake 4.5.1, libstdc++ and the existing Conan graph: the opt-in Release build passed all ten CTests. The focused suite also passed with the [upstream SHA-256-verified Lua 5.5.1 source](https://www.lua.org/ftp/) built as a C static library and instrumented together with the adapter under ASan/UBSan/LSan (`-no-pie` for this hardened host's shadow mapping). Tests cover fresh state, instruction and heap exhaustion, syntax/runtime/result failure, rejection of binary chunks, and no standard libraries. Lua is optional, under MIT, and not bundled or exported by the installed engine SDK. This is a partial checkpoint: no quest progression, save migration, or M5 acceptance claim.
