# Windows MSVC CPU verification

Owner-executed on 2026-11-04 at `d49b23c57fd3b02235ace2a1c10e85e9f93c97ca`, using `pwsh -NoProfile -File tools/build-windows.ps1`. Both Debug and Release configured, compiled and linked successfully. Each configuration passed all four CTest entries and both explicit replay runs. This verifies the MSVC CPU path, not clang-cl or Windows rendering.

| Item | Observed value |
|---|---|
| OS | Windows kernel 10.0.26200, x86_64 |
| PowerShell | 7.6.9 |
| MSVC | 19.44.35229.0; Conan compiler version 194 |
| VCTools / Windows SDK | 14.44.35207 / 10.0.26100.0 |
| Conan / CMake / Ninja | 2.34.0 / 4.4.4 / 1.13.2 |
| Language | C++20, extensions off |
| Runtime | Dynamic; Debug DLL CRT for Debug, release DLL CRT for Release |
| Graphics | `NGEN_BUILD_RENDERER=OFF` |

The detected base profile in `profile.txt` defaults to Release/C++14. The script overrides both host and build profiles to the selected configuration and C++20; `Debug-conan.txt` and `Release-conan.txt` are the effective profiles. Their runtime types match their build types, and both explicitly select Ninja. `profiles/windows-msvc` now records these verified settings with Debug as its default. Conan-generated Debug and Release host/build settings from that checked-in profile were compared with these logs and matched exactly; that comparison did not run another Windows build.

The locked recipes are enkiTS 1.12 and Catch2 3.16.0. Debug used cached binaries. Release used Conan's compatible C++14 enkiTS binary and built Catch2 from source with C++20. The raw Conan logs retain recipe/package identities and compatibility decisions. This is observed ABI compatibility for these builds, not a promise about arbitrary MSVC releases.

All four explicit replays used seed `0x4e47454e`, 1,000 ticks, eight entities and 32,000 commands. Each reported sum 11930, `allocation_probe=1`, `new_allocations=0` and `scheduler_allocations=0`. The executable also checks each expected final position. Timing samples are retained in the raw replay logs; CPU identity, power state and repeated performance samples were not collected, so these are not capacity claims.

The original owner-provided `evidence.zip` has SHA-256 `f8d2c52b19240b307f5e75304bcf2a059205281f74131805769c5fb4d8101bdf`. Selected command logs are retained here with CRLF normalized to LF and trailing whitespace removed to satisfy repository checks; diagnostic text is otherwise unchanged. `environment-excerpt.txt` contains only the relevant lines from the transcript, omitting account and process identifiers. `files.sha256` checks the retained files. The original archive passed a redacted Gitleaks archive scan before inspection. Git status produced no entries in the transcript; the script did not create a separate file for that empty output.

Remaining gates: clang-cl toolchain/ABI execution, Windows/NVIDIA graphics capability and presentation tests, and authorized formal milestone acceptance. Linux sanitizer and offline evidence remain separate. No Windows sanitizer run, no-network runtime test, graphics validation or independent human approval is claimed by this archive.
