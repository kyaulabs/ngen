# Windows GPU resource verification

The owner's two archives verify clean source `dbdd3bad93c9205fcfab287e854714ff69f293ed`, not the earlier first-light revision. Both were listed, hashed and redacted-Gitleaks-scanned before selected-file extraction/inspection. Scans covered 62,506 and 63,140 uncompressed bytes with no leaks detected. Full session transcripts, tool-install output and profile-detection chatter are excluded. Retained logs use LF and strip trailing whitespace; `original-files.sha256` identifies the selected original bytes and `files.sha256` identifies retained evidence. Archive hashes are identities, not proof of reproducible binaries or exhaustive secret detection.

| Archive | Compiler | SHA-256 |
|---|---|---|
| `evidence1.zip` | MSVC | `746f129cefa57fa62e4bbd4c7532dc9215f2696d19511e0c48a442786ea68cb9` |
| `evidence2.zip` | clang-cl | `6d65287355178462405a7bd57d27a4a3d6bb8dc58bb6b09b6068616f16d85c05` |

All four configurations compile/link the new VMA implementation, resources, shared probe support and resource executable in **66/66 build steps**, with no build warnings/errors. Each enumerates and passes **5/5 CTest entries**, without skips. Both explicit CPU replays per configuration preserve sum 11930 and `allocation_probe=1 new_allocations=0 scheduler_allocations=0`. Hidden-window startup has no validation diagnostics. Separate callback calibration contains exactly its one labelled intentional diagnostic.

| Compiler/configuration | Resource clear requests / generations / resizes / minimize | Image replacements | Allocation / reserved plateau |
|---|---|---|---|
| MSVC Debug | 96 / 3 / 2 / 1 | 471 | 4,752 / 3,145,728 bytes |
| MSVC Release | 96 / 3 / 2 / 1 | 463 | 4,752 / 3,145,728 bytes |
| clang-cl Debug | 96 / 3 / 2 / 1 | 467 | 4,752 / 3,145,728 bytes |
| clang-cl Release | 96 / 3 / 2 / 1 | 450 | 4,752 / 3,145,728 bytes |

Every resource log reports:

```text
buffer_lifetime=1 gpu_roundtrip=1 pending_retirement=1 resource_slots=64 replacement_cycles=128 image_lifetime=1 image_roundtrip=1 image_retirement=1 close_invalidated=1
```

Successful probe completion verifies real buffer and 140-byte image transfer equality, generation/weak-owner lifetime, gated pending retirement/reclamation, bounded capacity/backpressure, foreign-context identities, invalid/closed access and repeated replacement across clear-only presentation/resize. The two device identification lines are expected: the probe also creates/closes a temporary foreign context. Normal startup, standalone presentation and resource logs have no unexpected validation diagnostics. Both presentation scenarios enable synchronization validation. The separate presentation probe also records 96 accepted requests, three generations, two resizes and minimize/restore in all four configurations.

The plateau is checked against the fixed workload's own warmup in each run, not across different GPUs. The Windows/NVIDIA allocation total is lower than the Linux/Intel 8,348-byte result; native requirements/alignment differ. Both report 3 MiB reserved in this workload. Replacement counts vary with skipped frame attempts. Neither is a physical-residency, total-VRAM, throughput or performance acceptance claim.

Compiler identities are MSVC 19.44.35229.0 and genuine clang-cl 19.1.5 targeting `x86_64-pc-windows-msvc`. Effective Conan/CMake inputs are x64/C++20, extensions off, matching dynamic Debug/Release CRTs; clang-cl uses Clang 19 host/MSVC 194 build profiles and runtime family v144. The detected MSVC host profile still says C++14; per-configuration Conan overrides and CMake output establish C++20. Compatible cached dependencies are not represented as freshly compiled C++20 dependencies.

Both graphs select stock shared SDL 3.4.16 recipe `fe421775c8e1866eb22a3ba23ef8fd62`, VMA 3.3.0 recipe `5393b5bc9a6b8a2fb9612df877706608` and Vulkan-Headers 1.3.296.0 recipe `4ea73880426698c2e903c3e7acd892dc`. The MSVC Debug run downloads the two new header-only packages; later graphs use cache. VMA's implementation itself is compiled into NGEN in every configuration. SDK import-library discovery uses `C:\VulkanSDK\1.4.363.1`; the reported CMake header version is now the pinned 1.3.296, not a claim that the installed SDK was downgraded. Device identity remains RTX 5070 / driver 2588573696 (617.42), Vulkan API 1.4 and the Windows display backend. No driver, SDK or persistent environment/registry change was needed.

This closes the requested Windows/NVIDIA execution gap for the bounded resource slice. No further setup or rerun is requested for this slice. The [Linux evidence](../gpu-resources/README.md) separately records instrumented workloads, 21 controlled failures and strict owned-code analysis; those checks were not rerun with Windows sanitizers or injected faults. This evidence does not establish shader use of these resources, texture display, captured pixels/scanout, cooked assets/PBR, arbitrary recovery, performance or independent/formal milestone acceptance. M2 remains incomplete.

Same-session review checked both archive/source identities, clean trees, distinct compiler/CRT/standard/dependency selection, all build/test/replay counters, normal versus intentional diagnostics and resource/frame/resize/minimize/plateau outputs. It is not independent code review or owner approval of the full milestone. The next planned implementation is shaders/reflection, followed by the bounded cooked-asset/PBR pipeline, rather than another Windows setup exercise.
