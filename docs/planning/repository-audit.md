# Repository audit

Planning date: 2026-11-02. Status: public-template audit complete; private NGEN comparison blocked by unavailable checkout. This report does not establish that NGEN is empty or that template files are unchanged in NGEN.

## Evidence inspected

| Evidence | What was actually inspected | Limit |
|---|---|---|
| `PRO-HAND-OFF.md` | Complete attached specification | Requirements, not implementation evidence |
| Supplied `AGENTS.md` | Complete 104-line, 4,446-byte file recovered from the user's files | Not verified against the private checkout's current HEAD |
| `distill` | Local Prism copy of `SKILL.md` and full `references/patterns.md` | Used for this plan; not claimed to be the current copy on NOVA |
| Public `kyaulabs/template` | Default `develop` commit `588ff8affa0888d09010bd22bd17e43b9e852eee`, committed 2026-09-03; complete non-truncated tree and text files | This is a comparison reference, not NGEN's proven template ancestor |
| Public `kyaulabs/template-workflows` | `develop` commit `f636ba2d568c3a6251d1a36f68e32b1ea8cc8a42`, committed 2023-06-12; complete tree | Referenced by template README; installation into NGEN not verified |
| Private `kyaulabs/ngen` | No checkout or authenticated repository connection available in this session | Files, history, current branch, dirty state, workflows, rulesets, tests and implementation remain unknown |
| Vulkan profile and hardware | Hardware specifications in the hand-off only | Raw profile absent; no GPU, driver, passthrough or performance test executed |

The supplied `AGENTS.md` and the task's narrower planning scope govern this deliverable. No branches, commits, PRs, infrastructure or engine implementation were created. No project tests were run. Documentation checks performed on this package are not engine tests.

## Public template inventory

The audited tree contains 17 files. It has no `.github/workflows/`, `ruleset-*.json`, root `AGENTS.md`, C++ source, CMake project, Conan recipe, tests or engine documentation. That statement applies only to the public template commit above.

| Area | Observed files | Preserve or adapt after comparison |
|---|---|---|
| Ownership/community | `.github/CODEOWNERS`, `FUNDING.yml`, `CODE_OF_CONDUCT.md`, `SECURITY.md` | Preserve existing project choices; validate CODEOWNERS syntax |
| Contributions | `CONTRIBUTING.md`, Git Flow and GitHub Flow SVGs | Preserve Git Flow; reconcile older instructions with actual NGEN policy |
| Issues and PRs | `.github/ISSUE_TEMPLATE/01_ISSUE.md`, `config.yml`, `.github/PULL_REQUEST_TEMPLATE.md` | Reuse the actual NGEN versions; fill every applicable PR section and Test Plan |
| Local checks | `.github/hooks/pre-commit`, `.github/hooks/commit-msg` | Preserve Gitleaks and commitlint; confirm installation, executable mode and dependency availability |
| Commit/changelog policy | `commitlint.config.js`, `cliff.toml` | Preserve configured types and layout; verify project URLs |
| Documentation/licensing | `README.md`, `LICENSE` | Do not change README layout, logos, main headers or badges; resolve licensing before publication |
| Ignore rules | `.gitignore` | Template covers Visual Studio caches only; compare NGEN before adding generated C++ build/cache paths |

Source: [pinned template tree](https://github.com/kyaulabs/template/tree/588ff8affa0888d09010bd22bd17e43b9e852eee). API tree and downloaded files were inspected directly; the web page renderer could not retrieve this repository.

## Findings that matter for NGEN

1. **Policy is present; enforcement must be verified.** Hook files exist, but their presence does not prove installation, executable permissions, available tools or CI enforcement. Do not treat README badges as evidence that checks run.
2. **There is no C++ workflow to inherit from the audited template.** The separate workflow repository contains artifact cleanup, changelog, Lua, PowerShell, shell and Unity templates. Its tree contains no CMake, Conan or Vulkan workflow. Read any selected workflow before adapting it; do not copy obsolete action pins or privileges wholesale.
3. **Branch instructions conflict.** The template contribution guide uses a different hash length and an `openssl rand hex 2` example. The supplied NGEN policy requires `<type>/kyau-(openssl rand -hex 3)-name-of-branch`. The NGEN instruction wins. Template README examples of direct pushes to `main` or `develop` are not authorization to do that in NGEN.
4. **The commit-type list needs reconciliation.** Template `commitlint.config.js` accepts `feat`, `patch`, `fix`, `docs`, `perf`, `refactor`, `revert`, `style`, `test`, `ci`, `chore`, and `ignore`. README also mentions `build`, which that configuration does not accept. Use the actual NGEN configuration. Until inspected, proposed foundation tasks use `chore` or `ci`, not an assumed `build` type.
5. **CODEOWNERS may not express the intended owner.** The template contains `* kyau`. GitHub's documented username syntax uses `@username`. Validate NGEN's file and GitHub's reported parse errors before proposing a narrow correction. This session did not verify active code-owner enforcement. [GitHub CODEOWNERS documentation](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/about-code-owners)
6. **Review the hook implementation, without bypassing it.** The commit-msg hook invokes `commitlint --edit ${1}` without quoting the path. The pre-commit hook invokes `gitleaks detect --source . -v --no-color --max-target-megabytes 5`. Confirm the installed Gitleaks version and the intended staged/history coverage; do not infer complete scanning from this command alone.
7. **Changelog links still reference the template.** Check whether NGEN already replaced these links before proposing edits. Preserve the configured format.
8. **The template declares AGPL-3.0.** That is not proof of NGEN's current license or a final conclusion about derivative-work obligations. Inventory the actual license, copied material, copyright ownership, contributor grants and third-party notices. Prefer a permissive engine license such as Apache-2.0 if Sean has the rights and wants proprietary games to use it. A folder boundary alone is not a licensing exception. No license was changed. [Template license](https://github.com/kyaulabs/template/blob/588ff8affa0888d09010bd22bd17e43b9e852eee/LICENSE), [AGPL text](https://www.gnu.org/licenses/agpl-3.0.en.html)

## Private-checkout comparison procedure

Run only when the actual NGEN checkout is supplied. This is M0-A and does not require a new feature branch for read-only inspection.

1. Read root and applicable nested `AGENTS.md` before repository operations. Record the checkout path, HEAD, branch, remotes, dirty files and submodules without modifying them. Never reset or clean user work.
2. Inventory tracked files and existing build, engine, test, asset, documentation and workflow areas. Search for CMake/Conan configuration, renderer initialization, command/tick types, server entry points, ADRs and existing adapters. Read the relevant implementations before deciding to replace anything.
3. Identify the actual template-derived commit or copy history if possible. Compare each relevant file against that ancestor and against the audited public reference. Classify it as unchanged, customized, new, deleted or unknown; attach hashes or a focused diff.
4. Read all local `ruleset-*.json`, actual hooks, PR template, commitlint configuration, branch conventions and CI definitions. Compare local rule intent with remote active protection separately. A private-repository plan or inactive JSON file must not be treated as enforced remote protection.
5. Record the smallest gaps needed for M1. Retain working code and project patterns. Replace no implementation merely because this proposal names a different library or directory.
6. Produce an evidence table listing each conclusion, path, exact revision and verification command/output. Do not mark the audit complete until a second reviewer can reproduce the classification.

## M0 closure conditions

- Actual NGEN `AGENTS.md`, tree and implementation inventory are read.
- Template ancestry/customizations and license ownership are documented.
- Existing checks and missing checks are distinguished.
- Private ruleset and workflow requirements are recorded.
- Raw Vulkan capability capture and runner hardware inventory are attached as evidence, or the affected hardware gate remains explicitly blocked.
- Proposed source paths and dependency additions are reconciled with existing code.

The public audit above is useful now. It does not close M0 and does not authorize M1 implementation.
