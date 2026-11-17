# Independent exhaust follow-up at `afb94f7`

Separate read-only agent review of commit `afb94f7da8ea0def13677b2bde00f52674b70a46`, tree `f929ec1be2cf185dbe77a7e5866701d52565b33d`. This is bounded agent advice, not personal owner approval. No private artifact bytes, session logs, builds or benchmarks were accessed.

Original final-report SHA256: `d127345a7a39295dbd33a1488a8e271c11bda075288bc69ff26e13037591c875`. The report below is retained verbatim.

No actionable defects or new advisories found in this bounded review. This does not claim exhaustive correctness.

Confirmed clean, detached HEAD at `afb94f7da8ea0def13677b2bde00f52674b70a46`, tree `f929ec1be2cf185dbe77a7e5866701d52565b33d`. All 55 changed paths are under `docs/`: offline receipts, evidence and summaries. No production C++, shader or dependency changes; `git diff --check` passes.

The derivation, replay and recorded outputs are consistent with the accepted exhaust scope. M2 remains open for the single targeted MSVC Release compiled-UI/accepted-asset run, remaining personal inspector/input verdict and scoped PR integration. Agent review supplies no personal human approval. M3 remains unstarted.

Actual read-only checks:

- Recomputed all **45 exhaust-manifest hashes**, **49 affected closure entries** and **12 source identities**. Original authoring-receipt and palette hashes match embedded creator metadata; canonical/source/final identities agree across records.
- Reviewed replay validation, staging and publication, allowed prefix changes, material/sampler references and relevant native consumers. The sRGB RGB/linear alpha treatment, eight complete mips, alpha-weighted filtering, transparent depth-read-only drawing and shadow exclusion agree.
- Independently reconciled **49 shared vertices / 32 shared triangles × six instances = 192 visible triangles**. Catalog LOD2 is **238,112**; rendered ship/plume **238,272**; total **246,272**, with **195 draw records, 14 materials, five images and 89,522,172 texture bytes**. Added maximum X recomputes to **185.42000007629395**; changed fixture fitting is explicit.
- Recorded replay controls—including Python `-O`—report the exact accepted output. The recorded pixel expectation genuinely fails without plume and passes for the final variant. HD admission and initial catalog-count assertion failures remain correctly classified as setup failures.
- Recomputed every value across **1,200 CSV rows**, frame and CPU/GPU nearest-rank distributions, host ordering, extents, generations and settlement.

| Run, frame ns | P50 | P95 | P99 | Maximum |
|---|---:|---:|---:|---:|
| Steady | 16536045 | 17842376 | 18551985 | 18734107 |
| Mixed | 16539137 | 17938154 | 26152455 | 102601229 |

Mixed resizes occur at frames **20/40**; generations advance at **33/90/147/203** to five. Both last-300-row windows settle at **211,635,648 allocated / 214,399,344 reserved VMA bytes**, **23 live / zero retiring**. Mixed allocation peak is **472,084,240**, below 512 MiB. Allocation-call wording correctly distinguishes steady **0/0** from mixed **2660/2**, and explicitly counts the two temporary exposure/FXAA edits.

I executed no retained scripts, builds, tests or benchmarks and used no remotes. Private model/package/capture/reference/ZIP bytes were neither inspected nor independently rehashed. Native execution, pixel results and ZIP contents were assessed through their public recorded evidence; VMA results do not extend the older WDDM scope.