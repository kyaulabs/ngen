# Advanced rendering workstream

Status: optional M11 proposal, researched 2026-11-02. These effects cannot block M3 flight or replace the Vulkan 1.3 basic path. No SDK integration, driver support or visual/performance result was tested.

## Adoption order

First improve material consistency, lighting, shadows, exposure and asset LODs. Then measure GPU-driven culling/indirect draws for real object counts. Add temporal reconstruction only after motion vectors, depth, exposure, jitter, render/output resolution and history-reset rules have stable definitions. Evaluate RT/GI/volumetrics individually so a regression has an identifiable cause.

Each experiment uses fixed camera traces with ship silhouettes, thin antennae, engine trails, transparent particles, rotating objects, occlusion, warp and origin changes. Measure native-render frame time, reconstructed presentation time, input latency, memory and ghosting separately. Generated frames cannot count as faster simulation, lower input latency or a passing base-render FPS gate.

## Technology comparison

| Feature | Current primary-source evidence | NGEN recommendation and fallback |
|---|---|---|
| DLSS / DLAA | NVIDIA describes DLSS 4.5 technologies and DLAA's native-resolution mode. DLSS relies on RTX hardware; the requested Intel/GTX baseline must remain independent. [1] | Optional RTX 5070 quality path. Evaluate super resolution and DLAA first; frame generation is a later latency/pacing experiment. Native rendering remains available. |
| Streamline integration | Retrieved official README identifies Streamline 2.14.16 and Windows prerequisites; it includes Vulkan support and separately distributed feature binaries. [2] | Windows Vulkan candidate. Do not infer native Arch Linux support from the word Vulkan. Verify a supported native Linux/NGX route and redistribution terms before promising Linux DLSS. Keep adapter code outside core/simulation. |
| FSR temporal upscaling | AMD's current SDK 2.4 lists Vulkan support for upscaling/frame-generation, including analytical FSR 3.1.6 and ML Upscaling 4.1.2. Its deployment uses signed DLLs. [3] | Evaluate a source-available FSR 3.1 Vulkan path for Linux; the separate FSR 3 page documents source/Vulkan and a 3.1.5 package. [4] Neither a Windows DLL nor ML support for selected AMD GPUs establishes Iris Xe/GTX/Linux compatibility. Retain native/TAA fallback if the exact source build or quality gate fails. |
| FSR licensing | AMD's overview uses open-source language, but its detailed SDK license distinguishes binary redistribution terms from selected MIT files. [5] | Audit the exact selected source/effect binaries. Do not label the whole current SDK MIT or assume source availability for every effect. Record terms with the dependency manifest. |
| Advanced GI | NVIDIA RTXGI's current repository describes version 2.0 approaches. AMD's current SDK page says its Ray Regeneration and Radiance Caching remain DirectX 12-only. [6, 3] | Compare fixed/baked probes and an appropriate Vulkan dynamic-probe/RT experiment. Do not adopt a DX12-only GI dependency for this Vulkan engine. Verify exact feature, OS, license and performance requirements before choosing RTXGI components or another proven implementation. |
| Volumetrics | No provider algorithm is accepted by this plan | Begin with bounded, lower-resolution nebula/fog integration, explicit depth/temporal history and quality-scaled sampling. Compare against authored geometry/material approximations on the laptop. |

Versions above identify the research snapshot, not dependencies to add now. At M11, recheck released versions and security/compatibility status, then pin the exact tested package/commit. External upscaler source can use its required shader language behind an adapter; this does not authorize rewriting NGEN's main shader pipeline in a second dialect.

Streamline source licensing does not determine DLSS binary rights. Review the [Streamline notices](https://github.com/NVIDIA-RTX/Streamline/blob/main/license.txt) and the separate [NVIDIA RTX SDK agreement used by DLSS](https://github.com/NVIDIA/DLSS/blob/main/LICENSE.txt) for the selected distribution. Record required notices and permitted redistributables before accepting an integration.

## Runtime and content contracts

An optional effect declares required feature bits/formats, input textures and meanings, output dimensions, memory budget, scratch lifetime, reset conditions and supported quality presets. Capability checks happen before selecting it. Missing SDK binaries or unsupported hardware select the maintained fallback with an actionable diagnostic.

Motion vectors describe current-to-previous visible surface movement in an explicitly documented convention. Origin shifts transform both histories consistently; teleports, camera changes, warp transitions, resize and control switches reset or remap history as required. UI/text composition is separated from world reconstruction. Transparent/reactive effects provide supported masks only when the selected algorithm needs them.

RT is an optional renderer capability with its own acceleration-structure build/update/scratch budget. It never becomes a dependency of physics, collision, server authority or asset loading. Compare a narrowly defined effect, such as ship reflections, before attempting a fully ray-traced scene. Maintain non-RT shadows/reflections and report their visual differences.

## M11 acceptance

1. An approved effect-specific ADR identifies the content problem, current baseline, candidate implementation, exact license and supported machines.
2. Fixed images/traces show the improvement and expose artifacts at low frame rates, during warp and through disocclusion. Sean/art review accepts the result using recorded examples.
3. Record native-render and presentation timings separately; meet the target tier's budget. The roadmap's provisional guard is no more than 5% p95 regression in unaffected baseline scenes on the same machine, with at least three warmed runs.
4. Force every optional capability off and rerun the basic path. Asset packages remain loadable and the headless build remains free of graphics SDK dependencies.
5. Driver/SDK upgrades rerun the affected corpus. Retain a feature-disable path and compatible fallback for a bad release.

## Sources

1. [NVIDIA DLSS technologies](https://developer.nvidia.com/rtx/dlss)
2. [Streamline official README](https://github.com/NVIDIA-RTX/Streamline/blob/main/README.md)
3. [AMD FSR SDK](https://gpuopen.com/amd-fsr-sdk/), [SDK 2.4 Vulkan update](https://gpuopen.com/learn/amd-fsr-sdk-2-4-rx-6000-ryzen-apus/)
4. [AMD FidelityFX Super Resolution 3](https://gpuopen.com/fidelityfx-super-resolution-3/)
5. [Detailed AMD SDK license](https://gpuopen.com/manuals/fsr_sdk/license/)
6. [NVIDIA RTXGI](https://github.com/NVIDIA-RTX/RTXGI)
