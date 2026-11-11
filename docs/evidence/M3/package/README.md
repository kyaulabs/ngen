# M3 offline packaging and Windows handoff

This checkpoint adds `tools/package_demo.py`, a private Release bundle creator,
and `-FlightDemo` to the trusted Windows workflow. M3 is not complete or accepted.
Current Windows compilation/startup, three warmed laptop runs, scoped RTX results,
recording and owner control/listening feedback remain required.

The bundle copies the accepted local cooked asset, executable, required SDL shared
runtime, dependency notices, project license and an allowlisted committed source
snapshot. Linux patches only copied executable/SDL RPATHs. Windows includes CMake's
app-local compiler DLL collection. A SHA256 manifest records delivered files and
source revision without SDK paths. Existing output directories/files reject;
failed preparation rolls back only its owned reservation. Dirty source cannot be
presented as archived HEAD. Private assets, captures, credentials and `.pi` are not
source-archive inputs.

Eight real-filesystem boundary cases pass, mocking only external Git/patchelf
processes. They cover Linux contents/integrity, Windows DLL layout, existing-owner
preservation, runtime-root escape/tool failure, malformed/instrumented admission,
exact SDL version, mandatory redistributed SDL notice and source correspondence.
Synthetic DLL layout is not a Windows loader or compiler result.

Four package-enabled graphs pass21/21 each. Three complete native Clang commands
for the changed application pass; GCC builds/tests pass without stripping its
unsupported AST flags. Four actual three-frame renderer/audio runs correlate every
CSV `host_ns`/`frame_ns` interval with the real parent monotonic process window.
Debug/ASan enable Vulkan validation; ASan/UBSan/leak failures remain fatal. All HDR
checks/GPU rows pass with empty stderr. These use SDL's dummy audio device and do
not establish listening, unplug or performance acceptance.

The Windows handoff separates validation, clean1080p timing and instrumented
process-memory observation. Flight CSV uses the same steady-clock epoch contract
as the existing M2 workload. The observer now admits up to64 bounded columns for
its60-column trace; byte/row/clock correlation, missing-query, gap and GPU-budget
checks remain active. Both PowerShell boundary suites pass, including real owned
child processes in the original suite. New orchestration tests mock process/memory
boundaries and prove argument preservation, output freshness, failure cleanup,
asset-mutation rejection and SDK-environment restoration. Hosted CI runs both
suites. They do not compile or run Windows on Linux.

An earlier private Linux bundle built from audio HEAD `2837627` starts and renders
with networking unshared, Conan hidden and SDK library variables absent. Its three
frames have finite HDR, all GPU rows, zero measured C++/ImGui allocations and empty
stderr. That bundle predates the final packaging source commit; final delivery
must be regenerated from committed packaging source. It is not clean-machine or
cross-distribution Linux ABI proof. Linux requires compatible platform runtimes;
project source/locked recipes are not a vendor mirror, reproducible-build
attestation or legal approval for external redistribution.

See [Windows handoff](../../../planning/m3-windows-handoff.md) and `review.md`.
Private executable/asset/archive/media bytes stay outside Git. Retained small logs
and result metadata describe the exact exercised scope, not milestone acceptance.
