# Source releases and reviewed back-merges

NGEN uses `release/X.Y.Z` branches cut from reviewed `develop`, normally merged
into `main`. **Merging that release PR triggers publication.** This is distinct
from merging implementation into `develop`. Keep release branches; never tag or
publish an unmerged release branch or bypass signed-commit/review protections.

The first release was the **0.1.0 source baseline** after M3. Review each
verified checkpoint for a release; a complete, tested feature or useful fix can
ship without waiting for the whole milestone. Use patch releases for compatible
fixes, minor releases for new functionality, and explicitly evaluate breaking
changes against the published compatibility contract and major-zero policy.
Review the actual Conventional Commits and behavior; confirm the proposed bump
with `git-cliff --bumped-version --use-branch-tags`. Do not tag intermediate or
untested work just to increase cadence.

The automation accepts canonical `X.Y.Z` only: no leading zeroes, prerelease
suffixes or additional path components. `CMakeLists.txt` is the version authority;
release branch and source version must agree.

Publication contains exactly `ngen-X.Y.Z-source.tar.gz`, its `.sha256` sidecar and
`ngen-X.Y.Z-SHA256SUMS`. The archive is produced by `git archive` from the exact
clean release merge and carries its commit identity. Only tracked, admitted
regular source files are included; private/ignored scene assets, build directories,
credentials, runtime bundles and recordings are not payloads. Source and archive
entry/byte bounds are enforced. Checksums are integrity checks, not independent
publisher signatures; workflow-created annotated tags are not independently
GPG-signed. Published assets and tags are never moved or overwritten.

## Prepare a release

1. Start `release/X.Y.Z` from current reviewed `develop`, after checking existing
   branches, tags, releases and open PRs. Keep the canonical CMake version aligned.
2. Copy `.github/RELEASE.md` to `docs/releases/X.Y.Z.md`, replace every `X.Y.Z`,
   and tailor highlights/features/limits/validation to actual evidence. Keep the
   supplied theme-aware NGEN logo first and **no text H1 below it**. Getting-started
   commands describe source builds; do not invent downloadable playable bundles.
   Link existing version-pinned guides and label additional user/API docs as coming
   later until published.
3. Generate the tracked changelog with git-cliff, before making a real tag:

   ```sh
   git-cliff --use-branch-tags --output CHANGELOG.md
   python3 tools/release.py validate release/X.Y.Z
   python3 tools/release.py notes release/X.Y.Z > build/release-notes-preview.md
   PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tests.release_automation -v
   ```

   In the tracked changelog, use the new version's linked heading from the
   release-note preview in place of `### Unreleased`. Make sure every version
   heading begins on its own line and all previous versions remain. `--unreleased`
   is for the single-release preview, not the full changelog. Review the NGEN
   commit links and nested `##` / `###` / `####` headings. Retry an already-tagged
   exact checkout with `--current`, never `--latest`.
   The tracked changelog is generated before the preparation commit; the final
   publication notes also include any later release metadata/merge commits.
4. Sign the version/notes/changelog commit normally. Open the templated release PR
   into `main` as `kyaulabs-bot`; review as `kyau` after every Test Plan item and
   applicable workflow passes. Do not infer checks from branch protections alone.
5. `CPU foundation` now runs on `main` as well as the existing integration paths.
   For release promotion only, the owner explicitly selected commit-message lint
   from the release/develop split, preserving previously accepted immutable history.
   This avoids re-linting the old PR #6 generated merge body's line length; no
   message/history is rewritten, no rule disabled. Normal PR ranges are unchanged;
   promotion diff checks and history-wide redacted secret scans remain full.
6. Release preflight checks canonical metadata, real git-cliff notes, automation
   failure/retry contracts and an actual checked source archive. Its separate
   no-checkout credential job checks the inherited bot identity and PR read access.
   Wait for both preflight and the full CPU matrix, then approve as `kyau` with
   `✔️ Approved by: @kyau` and perform an ordinary bot merge pinned to that head.

## Publication and credentials

`.github/workflows/release.yml` accepts only owned `release/` PRs into `main`;
closed-but-unmerged PRs and unrelated branches cannot publish. It checks out the
exact merge, waits at most 75 minutes for that revision's successful main push
CPU workflow, packages source and generates the combined notes before tagging.
It creates/resumes a draft, uploads only the explicit inventory, downloads every
asset, verifies checksums/archive identity and compares bytes with local input.
Only then can it publish. The publisher job has repository contents-write and
Actions-read permissions; ordinary builds receive no bot secret.

Kodometer's `KYAULABS_BOT_TOKEN` is repository-local and is not exposed to NGEN.
NGEN already inherits the organization secret **`BACKMERGE_TOKEN`**. Only the
fixed credential check and back-merge steps receive it. They require the public
identity `kyaulabs-bot`; normal publication uses the scoped `GITHUB_TOKEN` instead.
Secret-name visibility is not proof of a valid token or PR write scope. The
credential check proves identity/read access; actual PR creation verifies write
access. If setup fails, configure the organization's selected-repository access
and token permissions through GitHub's secret/settings UI. Never copy values
from another checkout, put credentials in source/notes, or request them in chat.
No credential values or account scopes are changed by these scripts.

## Back-merge and recovery

After successful publication, the workflow opens or reuses a `main` → `develop`
PR using the bot credential. It validates published inventory metadata, bot
identity, the annotated tag and exact main checkout. It does not claim a second
independent byte audit: publication already performed that verification. The PR
uses the repository's section layout, requests `kyau`, and leaves review/CI Test
Plan tasks open. **It never auto-approves or auto-merges.** Complete those checks,
review as `kyau`, merge normally as the bot and watch exact post-merge CI. Never
delete `main`, `develop` or the retained `release/X.Y.Z` branch.

If only back-merge creation fails, run **Release back-merge recovery** separately:

```sh
gh workflow run back-merge.yml --ref main -f version=X.Y.Z
```

The recovery workflow never tags, uploads, edits or republishes a release. It
reuses an existing open PR or does nothing once `main` is integrated. An older
version whose tag no longer matches current `main` fails closed; inspect the
current release/back-merge rather than creating a mislabelled or downgrade PR.

For interrupted publication, rerun the same merged release workflow. A matching
annotated tag may be retried, never forced or moved. Draft assets may be replaced
only after validation; partial/corrupt downloads cannot publish. A published
release is download-verified but never overwritten, even if a rebuild differs.
Wrong/lightweight tags, dirty or wrong merge checkouts, malformed/missing assets,
failed exact-revision CI, bad notes, or API/auth failures stop the operation.
Investigate a corrupt published inventory instead of automatically repairing it.

A fresh source archive build is a source smoke test, not a new Windows binary
matrix, hardware timing/memory observation, license approval or reproducible-build
attestation. Preserve M3's actual accepted artifacts, revisions and remaining
non-benchmark observations. Detailed build steps live in the release description;
[the executive brief](planning/executive-brief.md) and
[roadmap](planning/milestones.md) distinguish implemented and future capabilities.
