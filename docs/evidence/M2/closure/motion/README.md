# Explicit exhaust flow and accepted final art

The owner accepts the turbulent recessed droplet exhaust, charcoal-gray hull
finish and six recessed bridge windows. The final private package is
`a02d5438ad32d9c8fb917ce847d5a822688ca5b51d69125f6da71cf2ba2116ed`.
These are visual verdicts, not M2 completion. A [separate read-only review](../independent-659d570.md)
finds no demonstrated P1/P2 defect and recomputes all public measurement rows;
its stale-progress wording advisory is corrected. Scoped Windows execution of
the new renderer, exact-head CI and normal PR integration remain open. M3 has
not started. An [earlier final-package Windows return](windows-source-mismatch.md)
passes asset/native checks with old `5a2f841`, not the flow renderer. The subsequent
[clean `062b4b9` return](windows-wrap-failure/README.md) builds and passes10/10
CTests but fails the native RTX phase-wrap continuity probe before workloads run.
The shader and detector are unchanged; [bounded diagnostics](windows-diagnostic.md)
are ready to identify the failing values. This is not a claimed repair or waived
gate. Use the focused probe before repeating a full Windows workflow.

`identities.json` binds the tested source files, actual compile databases,
Release executables and final package. Its parent revision identifies the
working-tree base, not an invented commit for uncommitted code. Native logs and
CSV retain their bytes. Only CTest selection JSON trailing whitespace is normalized;
`originals.json` records original/retained hashes and that transform. Private model,
reference, package and capture bytes are not copied into the repository.

## Runtime contract

`SceneView::material_flows` is an optional complete material-indexed table of
finite phase `[0,1)` and amount `[0,1]`. The renderer validates the whole table
before submission and copies encoded values; it never retains caller storage.
Empty/zero entries restore static rendering. Nonzero flow requires a blend
material. Material constants remain 64 bytes with unchanged bindings: word 3
packs phase/65536 in low16 and amount/65535 in high16; word 2 high16 carries a
stable source-node seed. Phase-only words, reserved bits and seeds without flow
are rejected. Normal/facing low bits keep their original meaning.

Only the workload resolves its authored plume material at admission/replacement.
The renderer contains no asset-name animation policy. A steady visual clock
traverses a 512-cell cycle in 16 seconds, independently of simulation state;
three cells span the plume, with four trailing samples across 18.75 ms. Seeded
anisotropic noise supplies rapid irregular longitudinal streaks, not sine bands.
UV0 guides motion aft; normalized guide coordinates are bounded before lattice
integer conversion. Density thins the outer skin and tip, while the hot base
stays anchored. An analytical tail envelope survives coarse texture mips.
No fluid solver, new pass/resource owner, runtime dependency or schema change is
introduced. This is authored presentation, not CFD or M3 propulsion.

## Verification

Native Reds cover admission/table domains, copied scene state, visible phase
changes, wrong-direction mutation, hard finite-card termination, rejected
mechanical bands, independent seeds, solid skin and unbounded guide behavior.
The wrap check decodes half floats and compares linear HDR changes rather than
half-bit distance. Its deliberately discontinuous phase mutation still fails.
The first large-finite-UV finiteness check passed on this GPU; the actual Red is
bounded-guide equivalence, not an invented NaN or crash. Parser/compile/setup
failures are not counted as behavioral Reds.

Debug, Release, ASan/UBSan and GCC full graphs pass 12/12 CTest cases each.
Shipping, shader-disabled and TSan graphs pass 11/11, 10/10 and 6/6. Eight native
resource/inspector runs pass with empty stderr. Two later C++ lint corrections
(unsigned promotion in half decoding and standard nearest-integer quantization)
were followed by affected builds/tests/native runs. Those CTest reruns exclude
only unchanged shader-offline work already passed with the exact shader source.

Clang analysis passes 111 actual compile invocations, including consumers of the
changed headers and disabled configurations. Nine affected invocations were
rerun after the two corrections; their original diagnostics remain. No compiler
flags were removed. GCC executes successfully, but clang-tidy rejects its actual
`-fmodules-ts`, mapper and dependency-format flags; it is not a clean GCC AST pass.
The TSan CPU graph has no affected renderer translation-unit/header consumer.
Existing asset-vendor advisories and historical recook limitations remain scoped
in earlier evidence; this work does not erase them.

A post-acceptance numeric-guide guard does not retune art parameters. Fixed views
before/after that guard were not byte-identical: 118/223/44 changed pixels among
2,073,600 per view, maximum channel differences 4/15/3. Both comparisons remain
in `analysis/`. Subsequent hull and bridge previews used the guarded shader.
Bridge-only refinements preserve the accepted aft/exhaust views byte-for-byte,
which is a separate, retained comparison rather than a claim about the guard.

## Private art and acceptance

The original GLB, canonical package and accepted static derivative remain
immutable. Six original author scripts replay exactly on the current host from
static base `067407fb…` through white-core texture, recessed droplet geometry,
selective panel coating, bridge-light localization, charcoal/translucent
materials and final recessed glazing. The replay preserves original/final
hashes. NumPy 2.5.4 is existing authoring tooling, not a runtime dependency;
texture generation uses the pinned KTX executable identified in the receipts.
No reference image pixels or logos are used.

The hull finish is accepted at package `27de456e…`: only three coating factors
are darkened, leaving wear/normal/ORM maps and gray/red design intact. The prior
cyan bridge wash was local light, not a reactor or one giant window. Its six
original exterior proxy lights had 9 m reach. Their placement/reach and glazing
emission were localized without changing global exposure/bloom. A lower-power
candidate activated a different roof lamp under the bounded light tier and was
not selected. The chosen correction preserves the selected light set; middle
ordering can change, while the drive prefix and native aft pixels remain fixed.

Material-only glazing still showed solid hull. The owner explicitly authorized
six local openings with shallow dark backing. The final derivative cuts only the
raked/chamfered citadel patches at all three LODs, replaces solid window boxes
with single translucent panes, and reuses the original black material for
frames/interiors. All accepted materials/maps, unrelated primitive attributes
and topology, original instance prefix and exhaust local geometry remain exact.
Only six existing bridge-light proxies move deeper inside to avoid washing the
visible recess. There is no full cockpit, refraction shader or added light.

All 18 window/LOD sightlines fail for the original opaque backing and pass for
the 0.9 m recesses. Independent checks cover local removed area, nondegenerate
float32 cut triangles, unrelated geometry and the unchanged accepted art.
Native validation and ASan/UBSan package admission pass. All three aft views
remain byte-identical. The final workload has **247,542 visible triangles**, 211
draw records, 17 materials, nine images and 106,343,076 texture bytes, below the
unchanged limits. Final preview verdicts are retained in `acceptance/`; camera
orbits are visual inspection, not benchmark evidence.

## Clean current-host measurements

Both runs use the final package, Release, 1280×720, LOD2, 100 asteroids,
1024 shadow, exposure1/FXAAon/bloom.04 and 60 warmup/600 measured frames. Vulkan
validation is off for timing. `verify-measurements.py` recomputes all 1,200 rows,
hashes, phase domains/progression, extents, generations, original budgets and
tracked last300-frame settlement. No resize/reload frame is excluded.

| Run | P50 / P95 / P99 / max ms |
|---|---|
| Steady inspector | 16.502 / 18.286 / 18.580 / 19.272 |
| Four reloads, two resizes | 16.459 / 18.419 / 39.152 / 130.110 |

Steady warm C++/ImGui allocation calls are zero. Mixed edits/reloads allocate and
are not steady-state proof. Scripted frames1–2 deliberately use exposure.75 and
FXAAoff, restored at3; all rows remain counted. Both runs pass original timing
budgets and include the new flow-phase column. VMA/resource records are bounded
and settle; they are not a new WDDM observation, process working-set measurement,
continuous/global peak or indefinite leak proof. Prior native Windows memory and
static-package results keep their historical scope and do not prove this shader.
