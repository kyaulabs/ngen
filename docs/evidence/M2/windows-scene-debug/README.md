# Windows/NVIDIA combined scene — Debug

Both owner archives record clean signed source **`e387dde6a190b8003e4be719ee366659595d4cbe`** on RTX 5070/driver 617.42. Archive SHA256:

- MSVC: `9ade8487900a0db47c2dc31e023332d25749f6eaf3155d7ec5b234d3f70054a9`
- clang-cl: `846f3a8d5c23dcd0ccdf0fc52aad7e818988a44a7095b61b56449830c3a5062f`

Bounded ZIP path/type/size checks and redacted selected-log secret scans passed before extraction. Account-bearing transcripts are excluded. Text paths/line endings are normalized; original selected-file byte hashes are separate from retained-file hashes. Actual shader manifest and all five reflections are byte-exact. No source model/package is redistributed.

MSVC **19.44.35229.0** and genuine clang-cl **19.1.5**, x64/MSVC ABI/v144, each complete Debug build/link, **7/7 CTests**, both deterministic CPU replays (sum 11930, zero measured steady-state allocations), callback calibration and clean ordinary validation-enabled native checks. CMake records C++20/extensions off and dynamic Debug CRT. Clang identity is established by actual CMake compiler detection, host/build profiles and its native version/target output, not a renamed MSVC run.

Both GPU runs verify six cull counts/1,152 exact values and **16 PBR draws**, including both windings, UV1, independent samplers, absent normal and previous material/matrix controls. Production resources pass HDR post/AA, sampled WSI/timeline retirement, complete mip/HDR/depth/cube transfers, 128 replacements, indexed material/depth draws and gated two-slot retirement. Presentation records 96 requests/three generations/two resizes and verified Windows minimize/restore. Intentional callback calibration is isolated; ordinary startup/resources/shaders/scene logs have no unexpected validation diagnostics.

The checked scene fixture retains two versions, cancels an unpublished replacement safely, proves complete GPU readback and releases a queued frame version only after completion. It produces **5,470 finite nonblack HDR pixels** and a queued WSI request. It contains **3 vertices/3 indices/1 image/20 mip bytes/zero lights**: this is not full-frigate NVIDIA execution. WSI queuing is not displayed-pixel proof. Tiny fixture VMA allocations/reservations do not establish whole-graphics residency or performance.

These results confirm the two reported MSVC compile blockers are corrected. **Release is not present in either archive.** Generated IBL, real shadows/asteroid/inspector changes after this revision, full-frigate NVIDIA rendering, target-resolution performance, final compiler choice and independent/personal human M2 acceptance remain open. Same-session review checked source/compiler identity, markers, diagnostic isolation, exact artifact retention and scope; it is not independent review.
