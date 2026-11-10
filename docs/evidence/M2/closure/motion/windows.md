# Targeted Windows flow verification

**Complete:** [current-source MSVC Release execution](windows-verified/README.md)
passes at `c6b82da`, including all native controls and2,400 measurement rows.
No rerun or new personal approval is requested. The historical handoff below is
one fresh Release execution of the new renderer/final package, not another approval,
compiler matrix, capability capture or WDDM collection. The historical compiled
UI/bounds and static-package results are [complete](../windows-final/README.md).
Their old binary predates explicit flow and cannot verify this change. An earlier
[final-package return](windows-source-mismatch.md) reports clean old `5a2f841`.
The subsequent [correct-source return](windows-wrap-failure/README.md) builds
`062b4b9` and passes10/10 CTests but fails native phase-wrap continuity on RTX.
A fresh `b6ea825` build now supplies [numeric diagnostics and a local fix](lattice-continuity/README.md).
Use the corrected [fresh single-Release handoff](windows-diagnostic.md) next.
It avoids stale executables and the earlier pasted-command binding/redirection
errors. Its subsequent verified return closes continuity/final-asset Windows
execution; keep the accepted appearance and finish normal PR delivery.

The private input is `build/m2-final-visual-input.zip`, 48,245,561 bytes, SHA256
`bbc551a3a2a2dd412891805e09a00e75724d52b736ba7e584f4d4fb2fa8e6edb`.
It has exactly three flat members: `scene.ngas`, `manifest.json`, `README.txt`.
The package is 194,096,492 bytes, SHA256
`a02d5438ad32d9c8fb917ce847d5a822688ca5b51d69125f6da71cf2ba2116ed`.
No private asset/capture bytes are committed or publicly uploaded.

Extract to the Windows checkout's ignored `build\m2-final-visual-input\` folder,
preserving old inputs/evidence. In PowerShell7 at the repository root, update the
existing work branch without resetting local edits:

```powershell
$tree = @(git status --porcelain)
if ($LASTEXITCODE -ne 0 -or $tree.Count -ne 0) { throw 'Stop: preserve local edits or fix the repository path.' }
git fetch origin
if ($LASTEXITCODE -ne 0) { throw 'Fetch failed; do not build old source.' }
git switch feat/kyau-ade38c-m2-acceptance
if ($LASTEXITCODE -ne 0) { throw 'Branch switch failed; do not build.' }
git pull --ff-only origin feat/kyau-ade38c-m2-acceptance
if ($LASTEXITCODE -ne 0) { throw 'Fast-forward failed; preserve local changes and stop.' }
git merge-base --is-ancestor 659d570f61e4d9a5ab8fb5e505cc03ab127775fc HEAD
if ($LASTEXITCODE -ne 0) { throw 'New flow renderer is missing; do not build.' }
git rev-parse HEAD
```

Do not delete/reset owner edits. The ancestry check must succeed before building;
later evidence-only commits are fine. The native return records its actual build
revision. Confirm the already-correct input identity, then run the existing
workflow once:

```powershell
$options = @{
    Compiler      = 'MSVC'
    Configuration = 'Release'
    Inspector     = $true
    ScenePackage  = '.\build\m2-final-visual-input\scene.ngas'
}
& .\tools\build-windows.ps1 @options
```

Use the existing SDK/tools/cache; do not select `All`, `ClangCl`, `-GpuInfo` or
`-MeasureWorkload` for this follow-up. The workflow builds/tests the current graph,
executes native resource/flow and inspector/copy controls, renders the exact asset,
and records validated inspection plus clean steady/fixed-stream CSVs. These are
actual Windows/RTX checks, separate from the local Linux evidence. Keep compiler,
package, binary, revision and phase-report identities explicit; no cross-platform
floating-fixture CRC or dynamic-phase pixel equality is required.

Return the new `evidence.zip` path printed by the workflow. Preflight bounded
names/types/counts/sizes/compression/encryption/duplicates before selective
extraction and redacted scanning. Setup/session logs remain unread/unextracted;
never execute returned scripts/binaries or include credentials. Inspect all native
rows, phase domains/progression, actual extents/generations and settlement without
omitting scripted frames1–2. Old sampled memory proofs retain their scope.

M2 stays open until this execution, exact-head CI and normal reviewed PR integration
are complete. M3 must not start first.
