# Accepted blue-white exhaust presentation

The owner accepted this exact wider, longer version: **“that looks perfect.”**
This closes the exhaust-appearance decision, not the remaining inspector/input,
Windows execution or PR gates. M2 stays open; M3 has not started. The original
GLB/canonical package and unrelated untracked `brand/` remain untouched/private.

The separate accepted package is `scene.ngas`, 175,904,891 bytes, SHA256
`067407fb5fa2527771212cc7280a345b02ad3dd2a823ae8ba81b3553689ec52b`.
Its original canonical input remains
`e0ee6a023900c78b2c313f493f2a0e78c9639c39b6d830a1eaf3a3f6c0a29b6d`;
the GLB remains `e68cd409c479036556446ed1753b287c0e7a40260109b3cd442abe7f533941fe`.
No private model, package, capture, reference-photograph or brand bytes are retained
here. Owner development input is not public redistribution approval.

The owner first approved the blue-white palette but rejected the overexposed,
“on fire” housings. A zero-light diagnostic and a low-power candidate were not
selected for delivery: the latter lost all six core lights from the forward tier.
The localized version keeps core powers unchanged, confines their ranges to
2.5 m, and reduces collar/coil powers to .01 with .75/4 m ranges. All 612 positive
light definitions and their instance transforms remain; only 60 drive definitions
change color/power/range. An independent selection-rank mirror still selects
16 point instances including all six core lights; this is not GPU readback.
The ordinary exposure, FXAA and bloom defaults were not used to hide the spill.

The owner called the live localized result a **“MAJOR improvment”** and explicitly
requested a plume, superseding the earlier color/glow-only scope. They then asked
for a longer tail and a larger core/plume radius closer to the ring. The final
variant preserves the white base position, doubles the tail length to roughly
22–26 m, and uses .8 of nominal bell radius with soft texture edges. The blue-white
color/glow and bridge/navigation definitions are preserved. `F01_Window` is the
cyan bridge glazing/instrumentation, not a reactor. Authored material names such
as `F01_DriveRed` stay stable; unrelated red indicators are not recolored.

This is a **static presentation asset**, not a flight/thrust system, particle
framework or physical volume simulation. It adds one shared 49-vertex/32-triangle
primitive, six instances, one alpha/emissive material, one sampler/binding and an
original procedural 128x64 sRGB texture with eight complete mips. The existing
Context-owned PBR transparent path, depth-read-only blending, post and ownership
are used; there is no new renderer/backend, shader, runtime dependency or point
light. Blend geometry stays out of the directional shadow caster pass.

`package-verification.json` independently checks every original geometry, LOD,
texture, sampler, name and mesh-instance prefix; every unrelated material/light
byte; and the allowed factor changes in the two drive materials. Their authored
emissive strengths/maps/flags remain unchanged. Added geometry contributes
**192 visible triangles**, not 32: the catalog counts the shared primitive once,
whereas drawing counts all six instances. The normal scene has **238,272 ship/plume
+ 8,000 asteroid = 246,272 triangles**, 195 draw records, 100 asteroids, 14 materials,
5 images and 89,522,172 mip bytes. Original LOD0 geometry is preserved.

Bounds now include the added exhaust, extending maximum X from 160 to about
185.42. Consequently the normal fixture's camera, shadow projection and asteroid
placement reflect the new bounds. Its fixture CRC is `0ceb739b`, package CRC
`091e9782`; it is not the old byte-identical scene. Resolution, LOD2, asteroid
count, 1024 shadow map and original budgets are unchanged. The full-ship UI box
uses the complete package union, independent of selected part/material.

The fixed-camera native 1920x1080 side/quarter/rear captures use production scene,
post and tracked resource owners. They are not enlarged crops, and are not the
asteroid/UI benchmark. A temporary, exactly bounded 16 MiB capture encoder handles
this inspection size without weakening production capture admission. The first
HD attempt failed production encoding-storage admission and remains in the logs;
it is not a renderer defect. `inspection/` retains the helper and actual argument
arrays with repository paths normalized. `captures.json` identifies private pixels.

The public rendered-pixel expectation beyond the unchanged nozzle exit genuinely
fails on the localized no-plume asset: zero bright/blue/white pixels. The accepted
plume passes through the same fixed native camera and production renderer.
Ordinary validation/teardown stderr is empty. The first plume author's temporary
assertion confused catalog and instanced triangle counts; native admission itself
had succeeded. That setup failure is retained, not presented as behavioral Red.

| Clean Iris Xe run, 60 warmup / 600 measured | P50 / P95 / P99 / max ms |
|---|---|
| Steady, 1280x720 | 16.536 / 17.842 / 18.552 / 18.734 |
| Mixed, two resizes / four reloads | 16.539 / 17.938 / 26.152 / 102.601 |

`verify-measurements.py` recomputes every row, nearest-rank distributions, native
clock order, actual extents, generations and tracked settlement. It also works on
the retained `measurements/` without private BMPs, explicitly marking capture
hashes as not recomputed in that case. No resize/reload frame is exempted. Both runs pass the original timing limits, with finite HDR,
LOD2, exposure 1/FXAA on as default/restored settings and bloom .04. The mixed
walkthrough deliberately renders frames 1–2 at exposure .75/FXAA off, restoring at
3; all frames remain included. These validation-off runs have no external observer
or interposer; built-in GPU timings/allocation diagnostics remain enabled.

Steady execution records **zero warm C++ and ImGui allocation calls**. Mixed
edit/resize/reload activity records 2,660/2 respectively; it is not a zero-allocation
steady-frame test. Mixed generation reaches 5, settles for the last 300 frames at
211,635,648 allocated / 214,399,344 reserved VMA bytes, 23 live/zero retiring records;
observed VMA allocation peak is 472,084,240, below the unchanged 512 MiB Context cap.
This is not a new process/driver memory observation or indefinite-duration proof.
Earlier sampled WDDM and host-condition evidence keeps its original revision/asset
scope. No further WDDM run or completed compiler matrix repeat is requested.

`author-palette.py` and `author-plume.py` are exact original authoring receipts:
their hashes match embedded provenance. Their historical paths and intermediate
“No plume” palette wording are not a general entry point or a statement about the
final combined package. `provenance.json` preserves the accepted creators' exact
metadata plus one storage LF. The clean replay below removes only that LF, reuses
the existing assembler/KTX CLI, admits through the native package probe and must
produce the **exact accepted SHA256** before publication. It does not need the
reference photograph or execute either historical authoring receipt.

```sh
python docs/evidence/M2/closure/exhaust/reproduce.py \
  --base build/cooked-candidate/frigate-canonical-first/scene.ngas \
  --source planning/models/frigate/frigate-01.glb \
  --ktx /path/to/the/recorded/pinned/ktx \
  --probe build/inspector-release/asset_package_probe \
  --output build/cooked-candidate/frigate-exhaust-replayed
```

This is a Linux/tool-identity-specific offline receipt using trusted local build
inputs, not an OS/filesystem sandbox. `check-replay.py` exercises the public CLI
with actual tools, exact byte replay, wrong-source rejection and existing
file/directory/link preservation, including `python -O`. No original owner output
is replaced. `originals.sha256` records pre-normalization hashes; `files.sha256`
covers retained bytes. No dependency or system configuration changes are required.

The private owner handoff is `build/ngen-m2-exhaust-input.zip`, SHA256
`1cd8b19aa17634129510be66aca4e4d7b2becee21d76bb9ea30aa60e0ee968a5`,
47,526,154 bytes: only `scene.ngas`, its manifest and a short data-only README.
Follow [the one targeted Windows check](../windows.md). The compiled bounds/native
asset execution and remaining overall inspector/input verdict stay separate from
the already accepted exhaust appearance. Same-session review is not independent
or personal owner approval.
