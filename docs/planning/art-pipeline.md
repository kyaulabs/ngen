# Development art and asset production

Status: proposed workflow, researched 2026-11-02. No generation, subscription purchase, API entitlement or local GPU performance was tested. Art production runs alongside M2–M3 and does not block first light.

## Current OpenAI API choices

Official model pages expose `gpt-image-2.5-sunburst-2026-09-08` and `gpt-image-2.5-flare-2026-09-08`. Pin these snapshots in the evaluation. Provider guidance favors Sunburst for editing precision and Flare for draft generation; measure them on NGEN artwork before adopting a default. [1, 2]

The Images API supports generation/editing; Responses supports iterative context and adds mainline-model charges. Organization verification may be required. Direct Images is the initial workflow candidate. The guide permits **3840 × 2160** output but marks resolutions above **2560 × 1440 experimental**. Keep a lower-resolution fallback and label separately upscaled derivatives. Output dimensions do not establish usable detail or consistent multi-view geometry. [3]

| Standard processing, both 2.5 models | Price per million tokens |
|---|---:|
| Text input | $5.00 |
| Image input | $8.00 |
| Image output | $30.00 |
| Cached text input, where eligible | $1.25 |
| Cached image input, where eligible | $2.00 |

These are published token rates, not a fixed 4K price. Batch rates are separate. [4] Cached-input pricing applies to Responses image generation, not direct Images requests. [3]

For direct requests, estimate dollars as `(5*T_text_in + 8*T_image_in + 30*T_image_out)/1,000,000`, then record actual usage and billed cost. Include orchestration, revisions and rejected outputs in cost per accepted asset. A timed-out request may already incur charges. Preserve its identity and reconcile the outcome before retrying. Set a project budget and request cap before a separately authorized experiment; this plan spends nothing.

## Production pipeline

1. **Specify.** Record original design, role, silhouette, dimensions, faction materials, sockets, camera distance, collision and runtime budgets. Maintain a consistent style sheet.
2. **Draft.** Generate a bounded set of references with exact prompts, input hashes, model/settings, request IDs, usage and costs. Retain rejected drafts and selection reasons.
3. **Approve.** Human review fixes proportions, modular components and materials. Reference images guide a dimensional Blender blockout.
4. **Resolve views.** Prepare front, side, top, rear and perspective views. Compare hardpoints and dimensions; resolve contradictions in the blockout. Generated views alone do not define a consistent object.
5. **Evaluate image-to-3D.** Use identical approved references across candidates. Measure silhouette errors, thin-part loss, hidden surfaces, topology, UVs, material channels and cleanup time against manual modeling.
6. **Refine in Blender.** Set real scale, axes, pivots and sockets. Separate modular/damage parts; author collision proxies. Preserve the source `.blend`, generator output and human edits.
7. **Author materials.** Create appropriate base color, normal, roughness, metalness, emission and opacity maps. Remove baked illumination where runtime lighting would duplicate it. Review under several neutral lighting setups.
8. **Cook.** Build LODs and validate bounds, normals, tangents, UVs, texture formats, counts and sockets. Compile with recorded importer/cooker versions and content hashes.
9. **Accept in engine.** Inspect close flight, distant LOD transitions, shadows and damage states on the minimum hardware. The runtime asset is the acceptance artifact.

Keep generation tools outside shipped clients and servers. Use a restricted development account with environment/secret-manager credentials. Export only approved compiled assets; API keys, SDK dependencies and online generation are absent from shipping targets.

## Image-to-3D evaluation

| Candidate | Verified evidence | Decision |
|---|---|---|
| Meshy 7.1 | September 2026 changelog documents 7.1. API accepts 1–4 images; current prices list 20 credits mesh-only, 30 with 2K/4K textures and 35 with 8K, plus applicable Ultra surcharge. [5–7] | Hosted comparison candidate after paid/private terms and actual dollar-per-credit cost are reviewed. |
| TRELLIS.2 | Official repository states MIT model/code, separate dependency licenses, Linux testing and at least 24 GB NVIDIA memory. Its published timings use H100. [8] | Evaluate on verified suitable hardware; do not assume RTX 5070 compatibility or capacity. |
| TripoSR | Official repository states MIT model/code, about 6 GB default VRAM and optional texture baking. [9] | Lightweight local shape baseline; test hard-surface fidelity. |
| Hunyuan3D 2.1 | Tencent reports 10 GB shape, 21 GB texture and 29 GB combined VRAM. The custom license restricts territory and expressly restricts output use outside it. [10, 11] | Exclude from the default global asset pipeline unless licensing is resolved. |

These are provider claims, not NGEN measurements. Pin repository commits and weight hashes before experiments. Review dependencies separately from top-level licenses.

Meshy's terms distinguish free-plan attribution from paid-plan ownership/private options; community publication changes rights. Ordinary API assets are retained for at most three days, so persist successful results promptly. [12, 13] Obtain the actual account terms before sending private references.

The template's AGPL-3.0 declaration is addressed in [repository audit](repository-audit.md). Engine-license selection, tool licenses, model/output terms and individual asset rights require separate review. An engine folder boundary or permissive engine license does not grant rights to third-party imagery or restricted generated outputs. Keep proprietary game assets separable from reusable engine examples.

## Provenance and task gates

Each asset manifest records asset/version ID, parent hashes, raw source files, prompt/settings, dated model, provider task IDs, rights/terms snapshot, human approvals, tool versions, costs, runtime budgets, cooker version and final hashes. Record a seed only if exposed by the provider. Preserve generated bytes; do not promise identical regeneration.

| Stage | Atomic tasks | Completion evidence |
|---|---|---|
| M2 | Define asset schema/style sheet; create one Blender-authored test asset; add provenance and cooker checks; design bounded image comparison. | Asset loads reproducibly; source/rights/cost fields are present; no credentials in outputs. |
| M3 | Produce approved flight-demo hull, component, asteroid and environment references; compare manual and generated workflows; finalize materials/LODs/collision. | Human approval, recorded cleanup hours, passing runtime budgets and minimum-hardware presentation. |
| M11 | Extend budgets for fleet counts and advanced materials; validate LOD/texture streaming; reassess changed model/tool versions. | Fixed-scene traces, memory/LOD evidence and completed license/provenance audit for additions. |
| M12 | Review final asset rights, service terms and retained provenance alongside release-readiness investigation. | Every shipped asset has a documented rights basis; missing or incompatible rights block that asset. |

Proposed first comparison uses four objects: starter hull, fighter, industrial component and asteroid, with two candidate outputs per object. Measure median artist-hours to an approved runtime asset, including rejected results. Adopt a tool only if it reduces work without failing silhouette, modularity, collision, performance or rights criteria. All counts and gates are proposals; no success rate is claimed.

## Sources

1. [OpenAI Sunburst model](https://developers.openai.com/api/docs/models/gpt-image-2.5-sunburst)
2. [OpenAI Flare model](https://developers.openai.com/api/docs/models/gpt-image-2.5-flare)
3. [OpenAI image-generation guide](https://developers.openai.com/api/docs/guides/image-generation)
4. [OpenAI API pricing](https://developers.openai.com/api/docs/pricing)
5. [Meshy changelog](https://docs.meshy.ai/en/api/changelog)
6. [Meshy multi-image API](https://docs.meshy.ai/en/api/multi-image-to-3d)
7. [Meshy API pricing](https://docs.meshy.ai/en/api/pricing)
8. [Microsoft TRELLIS.2](https://github.com/microsoft/TRELLIS.2)
9. [TripoSR](https://github.com/VAST-AI-Research/TripoSR)
10. [Tencent Hunyuan3D 2.1](https://github.com/Tencent-Hunyuan/Hunyuan3D-2.1)
11. [Hunyuan3D 2.1 license, release June 13, 2025](https://github.com/Tencent-Hunyuan/Hunyuan3D-2.1/blob/main/LICENSE)
12. [Meshy terms](https://www.meshy.ai/terms-of-use)
13. [Meshy asset retention](https://docs.meshy.ai/en/api/asset-retention)
