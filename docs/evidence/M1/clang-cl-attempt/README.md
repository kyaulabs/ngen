# First Windows clang-cl attempt

Owner-executed on 2026-11-04 at `63d0a538f48881e8abe210d233b13ef939740369`. The script found Visual Studio's clang-cl 19.1.5, targeting x86_64 Windows with the MSVC ABI. Conan generated a Clang 19 host profile with dynamic runtime family v144 and an MSVC 194 build profile. CMake confirmed C++20 and the Debug DLL CRT. Configuration succeeded; owned-code compilation failed before tests. Release was not attempted.

The first failure is a warning-policy defect, not a missing C++20 setting. The Clang compiler-ID branch passed `-Wall` to clang-cl. That driver interprets it as all warnings, including C++98/pre-C++20 compatibility diagnostics. With `-Werror`, valid C++20 code therefore failed. The corrected policy selects `/W4` for MSVC-frontends and `-Wall` for GNU-frontends. Clang's explicit extra/pedantic/conversion warnings and `-Werror` remain enabled; native MSVC retains `/WX` and `/permissive-`. No diagnostic was blanket-disabled and the language standard was not changed.

A Linux-hosted clang-cl 23.1.4 regression consumed the actual public `ngen_options` target. The header-free `valid_cxx20` object failed with the same compatibility diagnostics before the change and compiled afterward. A separate narrowing conversion still failed as `-Werror,-Wfloat-conversion`, verifying that warning enforcement remains active. The fixture uses an existing Linux Conan metadata directory only to configure the surrounding targets; it does not compile or link the engine/dependencies for Windows and needs no Windows SDK. This is warning-policy evidence, not a full Windows build.

To repeat the warning-only check from the repository root after preparing Linux Debug dependencies:

```sh
fixture=$(mktemp -d "$PWD/build/clang-warning-XXXXXX")
cp docs/evidence/M1/clang-cl-attempt/fixture/* "$fixture/"
cmake -S "$fixture" -B "$fixture/out" -G Ninja -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_CXX_COMPILER="$(command -v clang-cl)" -DCMAKE_AR="$(command -v llvm-lib)" -DCMAKE_LINKER="$(command -v lld-link)" -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="$PWD/build/debug"
cmake --build "$fixture/out" --target valid_cxx20 --verbose
# Expected to fail on the floating-point-to-integer conversion:
cmake --build "$fixture/out" --target conversion_error --verbose
```

All five existing local CPU configurations (Clang Debug/Release, GCC, ASan/UBSan and TSan) built and passed four tests each after the fix. The PowerShell script-boundary/profile tests passed. Full Windows clang-cl execution must still be rerun using the unchanged command in [the handoff](../../M2/windows-checks.md).

Original archive SHA-256: `52f0eeefcbf0d2d08337ea7a354093e29c07c6f538ef42489c6c9edef53ac878`. Selected owner logs and local regression outputs are retained here with LF endings and trailing whitespace removed. The original archive passed a redacted Gitleaks archive scan before inspection. No credentials or account details from the full transcript are retained.
