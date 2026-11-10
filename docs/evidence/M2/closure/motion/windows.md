# Targeted Windows flow verification

**Pending.** All personal visual gates are accepted. This is one fresh MSVC
Release execution of the new renderer/final package, not another approval,
compiler matrix, capability capture or WDDM collection. The historical compiled
UI/bounds and static-package results are [complete](../windows-final/README.md).
Their old binary predates explicit flow and cannot verify this change.

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
git fetch origin
git switch feat/kyau-ade38c-m2-acceptance
git pull --ff-only
git status --short
```

Stop if source edits are present; do not delete/reset them. The branch must
contain signed renderer `659d570`; later evidence-only commits do not change that
renderer. The native return records its actual build revision. Confirm input
identity, then run the existing workflow once:

```powershell
Get-FileHash .\build\m2-final-visual-input\scene.ngas -Algorithm SHA256
.\tools\build-windows.ps1 -Compiler MSVC -Configuration Release -Inspector -ScenePackage .\build\m2-final-visual-input\scene.ngas
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
