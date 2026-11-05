# Windows clang-cl CPU verification

Owner-executed on 2026-11-04 at `54a7a63f03982d198e68c97a802a20ee743028e7`, using `tools/build-windows.ps1 -Compiler ClangCl`. Debug and Release both configured, compiled and linked without reported owned-code warnings. Each configuration passed all four CTest entries and both explicit replay runs. The working-tree report is empty. This closes the missing clang-cl execution evidence, not formal M1 acceptance or Windows rendering.

| Item | Observed value |
|---|---|
| Compiler | clang-cl 19.1.5, x86_64-pc-windows-msvc |
| Host profile | Windows x86_64, Clang 19, C++20, dynamic runtime v144 |
| Build profile | Windows x86_64, MSVC 194, C++20, dynamic runtime |
| Configurations | Debug and Release, matching runtime types |
| Conan / CMake / Ninja | 2.34.0 / 4.4.4 / 1.13.2 |
| Graphics | `NGEN_BUILD_RENDERER=OFF` |

The effective profiles and package identities are retained in `Debug-conan.txt` and `Release-conan.txt`. Debug reused cached Clang binaries; Release built enkiTS 1.12 and Catch2 3.16.0 from source using clang-cl. Its vcvars activation selected Visual Studio 17 x64 with MSVC tools 14.4. This demonstrates these dependency/runtime combinations, not arbitrary mixing of compiler versions or binary libraries.

`profiles/windows-clang-cl` records the verified host settings, with Debug as default and `clang-cl` resolved through PATH. Its build profile is `profiles/windows-msvc`. The owner script continues to discover absolute compiler paths and actual versions rather than assuming that every Windows machine matches this profile. Both checked-in configurations were compared with the owner logs through Conan profile resolution; host/build settings match. The portable executable names intentionally replace the owner's installation-specific absolute paths.

Every explicit replay used seed `0x4e47454e`, 1,000 ticks, eight entities and 32,000 commands. All four report sum 11930, `allocation_probe=1`, `new_allocations=0` and `scheduler_allocations=0`. Each executable also validates every final entity position. Timing samples are retained but are not capacity claims; CPU model and power state were not captured.

The first failed attempt and the warning-policy Red/Green/conversion-error regression remain in [clang-cl-attempt](../clang-cl-attempt/README.md). This successful run verifies the frontend-aware warning correction on the owner's actual compiler.

Original archive SHA-256: `0f96b91208cb66463e1f047edc3f78dc21abdc5302759c3d34bd13eafacbe6c0`. Selected logs are retained with LF endings and trailing whitespace removed; empty evidence files remain empty. `files.sha256` checks the retained files. The archive passed a redacted Gitleaks scan before inspection. No full account-bearing transcript is committed.

Remaining: authorized formal milestone acceptance, Windows GPU/device/driver capture, and all unimplemented or unaccepted M2 graphics outcomes. Neither Windows sanitizers nor Windows network-disabled execution was run here. The passing render-contract test is CPU-only policy validation, not a Vulkan startup or rendering test.
