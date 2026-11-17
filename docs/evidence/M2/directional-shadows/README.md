# Tracked directional shadows and full-scene captures

`SceneRenderer::draw` now submits indexed shadow depth and the PBR color batch together through Context's existing resource timeline. Opaque/masked geometry casts; blended geometry does not. Shadow vertices use each draw's model and light-view-projection matrices. Cutouts preserve base-map UV0/UV1 selection, sampler, material alpha and cutoff. The scene fixture creates a **1024×1024 D32** target instead of uploading a constant unoccluded depth texel. This is a fixed directional key shadow, not point-light cube shadows, cascades or final shadow quality acceptance.

`MeshPass::generate_shadow` explicitly selects generated depth; the existing provided-shadow path remains useful for analytic controls. Generated targets must be distinct live same-context, single-mip sampled D32 depth attachments. Nine finite prewarmed mesh pipelines cover facing/blend and shadow variants. Seven offline entries now pass the unchanged Vulkan 1.3/SPIR-V 1.5 capability baseline, reflection/layout checks and byte-exact runtime embedding. No dependency, runtime compiler or shader loader was added.

| Stage | Resource use |
|---|---|
| Shadow depth | Read tracked geometry, copied frame/material constants and base texture/sampler; clear/write D32 in depth-attachment layout |
| Opaque/mask/ordered blend | D32 shadow write → fragment sampled-read barrier; read material/IBL textures; clear/write HDR color and camera depth; blend does not write camera depth |
| Post/capture | Sample HDR; write tracked sRGB attachment; transfer completed pixels to readback for bounded BMP encoding |
| WSI | Sample the tracked HDR source through production post; graphics completion remains distinct from presentation/display retirement |

All native use joins successful-submit-only last-use/layout publication. The two existing resource command contexts and two owning scene frame pins are unchanged. Descriptor sets are updated **once before either pass binds them**, not between the shadow and color recordings. The first native implementation incorrectly updated bound sets and triggered command-buffer invalidation; its diagnostic is retained in `logs/descriptor-reuse-failure.txt`. No update-after-bind feature, validation suppression or larger descriptor pool was used to hide it.

The new capture mode is `scene_probe scene.ngas new-capture.bmp`. It renders offscreen at 1280×720, records actual WSI extent and selected scene counts, and saves the production tone-map/FXAA sRGB output. Encoding uses at most 4 MiB, followed by exclusive file creation (`fopen_s` on Windows, `fopen` elsewhere). Existing files/directories/links and a file created during GPU preparation are preserved. A genuine concurrent-writer Red exposed the earlier check-then-truncate implementation. GCC also rejected an attributed `fclose` pointer as a template argument under unchanged warnings-as-errors; a lambda deleter fixed that without suppression. The normal no-capture fixture remains 256×128.

Verification performed:

- Clang Debug/Release/ASan+UBSan and GCC: **10/10 CTests each**, explicit native shader/resource probes, and **3/3 physical capture-process tests each** against the complete frigate. ASan/UBSan/leak and ordinary synchronization/object-lifetime validation remain enabled.
- Native depth pixels independently check nonzero first index, base vertex, model/light transforms, nearest depth, background clear and all three facing variants including a mirrored model. Cutout controls distinguish repeat/clamp, UV1, alpha factor/cutoff and blended-caster exclusion. An off-camera caster shadows a visible receiver, proving sampled visibility rather than merely reusing camera depth.
- A real timeline gate holds both generated-shadow/color submissions. Releasing six source/depth owners preserves them until completion, proves two-slot backpressure and retirement. Invalid generated target format rejects before recording.
- Seven instrumented ASan native controls: first/last shadow-pipeline construction failures, disabled shadow depth, wrong UV1 attribute, wrong clamp sampler, disabled shadow culling and failed scene submission. Each exits 1 for its intended diagnostic without unexpected validation/sanitizer/leak reports; failed scene submission publishes no frame pin. `failure.cpp` retains the boundary injector.
- **28 actual Clang AST consumer analyses:** eight TUs in Debug/Release/ASan, four in the shader-disabled graph. The latter builds and passes **8/8 tests**; CPU **4/4** and real-child PowerShell checks pass. GCC's incompatible clang-tidy module flags remain unfiltered; no GCC AST pass is claimed.

The final standalone Release frigate run verifies 1,304,323 vertices, 3,309,060 indices and all 89,478,480 image mip bytes. It selects LOD2's **238,080 triangles across 188 draws**, retains 13 materials/612 light definitions and instances, reports **65,501 finite nonblack HDR pixels**, queues WSI at 1280×720 and writes the post-processed capture. The explicitly selected forward point-light subset remains at most 16; source definitions are not deleted. Two resident versions still measure 374,669,680 VMA allocation bytes / 376,766,832 reserved before targets, excluding WSI/driver memory.

An outer 170-second verification batch interrupted the additional GCC suite at `shader_offline` after the other three suites passed; a dedicated GCC rerun passed 10/10. The interruption is retained separately, not reported as an engine failure or a completed suite.

The original GLB and canonical package hashes remain unchanged. Model/package/captures stay ignored; selected normalized text logs and original/retained byte hashes are retained here. The private transfer ZIP and [Windows workflow](../windows-frigate/README.md) are prepared, **not executed NVIDIA evidence**. Earlier owner Debug evidence at `e387dde` does not cover these additions.

These are correctness/capture controls, not a warmed ship-plus-100-asteroid replay, scanout proof, benchmark, independent review or [M2 acceptance](../acceptance.md). Bloom, asteroid instancing, queued inspector/UI and disabled-tooling Release, final material/scheduling/visual review, required-platform execution and whole-memory/performance gates remain open.
