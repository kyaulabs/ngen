# Windows full-frigate NVIDIA evidence

**The full-frigate execution slice passes in genuine MSVC and clang-cl, Debug and Release, at clean signed `ef984ef18c6fb6ed9886f45003dc67e85d42170c`.** These owner-run results cover the later generated IBL/shadow renderer, not only the earlier small Debug fixture. No repeat or SDK/driver setup is requested for this slice. [Final M2 acceptance](../acceptance.md) remains open.

| Archive | Compiler | Configurations | Original ZIP SHA256 |
|---|---|---|---|
| `evidence1.zip` | MSVC 19.44.35229.0 / toolset 14.44.35207 | Debug + Release, 8/8 CTests each | `bef6dfa2f2f22163997111054d62a7381d422935ca0d1287fcc3e1dd235246e7` |
| `evidence2.zip` | clang-cl 19.1.5, x86_64 Windows/MSVC ABI / v144 | Debug + Release, 8/8 CTests each | `e438a9a0a982c26f6a9ddf858db3a282d077d80ae16ad49662fdd53660436890` |

The ZIPs are 328,067 / 331,525 bytes, containing 61 / 62 regular flat entries and 8,301,533 / 8,318,332 uncompressed bytes. Path/type/count/size bounds were checked before extraction. Selected text/JSON passed redacted secret scans before inspection. Account-bearing transcripts and setup/detection logs were excluded. Text paths/endings/trailing whitespace are normalized; native JSON manifests/reflections/package identities remain byte-exact. `originals.sha256` records selected original bytes, including uncommitted captures; `files.sha256` verifies retained files. Raw extractions remain ignored under `build/windows-frigate-owner-{1,2}`.

Both compiler configurations actually use **C++20, extensions off, x64 and dynamic Debug/Release CRT**. The base MSVC detected profile says C++14; the retained Conan install settings and CMake configure output prove its C++20 override. CMake distinguishes the MSVC and clang-cl frontends, and the clang version/profile identifies its actual executable/ABI. Each native scene/resource/shader executable links and subsequently runs; the Ninja logs end at 109/110, so no 110/110 log-completion claim is made. CPU replays retain sum 11930 and zero measured new/scheduler allocations; those CPU timings are not graphics performance.

All four combinations report **RTX 5070**, Windows presentation, driver raw value `2588573696` (617.42), Vulkan device API 1.4, and SDK root `C:\VulkanSDK\1.4.363.1`. The pinned consumer headers remain 1.3.296. Ordinary startup/presentation/resource/shader/small-scene/full-package logs contain no unexpected validation diagnostics; the deliberate callback calibration is retained separately.

Full-package verification:

- Canonical input **175,854,711 bytes**, SHA256 `e0ee6a023900c78b2c313f493f2a0e78c9639c39b6d830a1eaf3a3f6c0a29b6d`, recorded before execution and verified unchanged by the helper afterward.
- Complete readback of **1,304,323 vertices, 3,309,060 indices and 89,478,480 mip bytes / four images**; 13 materials, 188 primitives/draws, all 612 point-light definitions and instances retained.
- Selected **LOD2 / 238,080 triangles**, HDR and actual WSI extent **1280×720**, generated IBL and **1024 directional shadow depth**, 65,501 finite nonblack HDR pixels, queued WSI, cancellation, distinct versions and owning frame-pin retirement/close invalidation.
- Two-version VMA allocation **351,527,296 bytes**, reservation **353,624,448 bytes**, measured before draw targets. This is not total graphics residency or a whole-scene reload plateau.

Resource/shader controls also pass in all four combinations: exact mips/HDR/depth/cube transfers, production post/exposure/FXAA, indexed PBR, shadow model/light/index/facing/mirrored/cutout UV/sampler/alpha and visible-receiver checks, generated smooth/rough IBL, two-slot/gated retirement and 128 replacements. WSI controls queue 96 frames over three generations, verify two resizes and Windows minimize/restore. Native shader fixtures report six counts/1,152 exact values, 16 material draws/two windings, UV1/per-map samplers/absent normals and 4,096 matrix pixels. Seven reflection hashes match each actual manifest and shader source identities match the tested tree; Slang remains 2026.21, Vulkan 1.3/SPIR-V 1.5.

All four GPU post-processed BMPs are **byte-identical**: 3,686,538 bytes, 1280×720, 32-bit, SHA256 `621978da723790ad88190eae45ebf08fb2b20875e67ec3d48a57248108f3b924`, with 66,244 nonblack RGB pixels after post/FXAA. `captures.json` records those identities. The inspected image visibly contains the frigate/hull and engine lighting, not a blank target. Model/package/capture bytes remain ignored and redistribution remains unapproved. The capture is offscreen post output, not a desktop/scanout screenshot, and identical bytes on this driver do not establish universal cross-device equality.

This closes the requested **MSVC/clang-cl Debug/Release full-frigate NVIDIA execution** gate for this source. It does not provide personal owner visual approval or independent graphics review. The 100-instanced-asteroid workload, inspector/UI and disabled-tooling build, bloom, final material/scheduling quality, warmed frame/pass/allocation distributions and whole-memory budgets remain open. No runtime source was changed to obtain these results.
