# M1 independent review follow-up

The separate Pi review of `888aa10be749c4ca6c414e404765511a5e60617a` is retained in `independent-review.txt`. It used a fresh context with read-only file tools, not this implementation session. It is agent advice, not human approval; it did not rerun checks. The reviewer found no demonstrated engine defect, two CI defects, and the already-open Windows/hosted/acceptance gaps.

Both CI findings were confirmed by inspecting the event and Git semantics:

- `commitlint --from HEAD~1 --to HEAD` selected only one commit, versus three for the actual integration range `fde375b..888aa10`. On a PR merge checkout this can select an ignored merge message. The workflow now uses event base/head SHAs, with the integration merge base only for dispatch/new-branch fallback.
- `git diff --check` ignored committed changes. The workflow now checks the same explicit base/head range. Running that check revealed CRLF lines introduced by the existing setup commit; `.gitignore` was normalized to LF without changing its ignore patterns.

The current configuration also runs format/tidy with the hosted compiler's version and triggers on implementation-branch pushes so the new workflow can execute before merge. These changes supersede the workflow hash in the original `sources.sha256`; engine/app/test hashes and their native test evidence are unchanged.

Executed follow-up checks:

```sh
printf 'invalid subject\n' | NODE_PATH=/tmp/ngen-commit-policy/node_modules /tmp/ngen-commit-policy/node_modules/.bin/commitlint
# exit 1, as expected
NODE_PATH=/tmp/ngen-commit-policy/node_modules /tmp/ngen-commit-policy/node_modules/.bin/commitlint --from fde375b --to 888aa10
git diff --check fde375b
# workflow YAML parsed with PyYAML; each extracted run step passed bash -n
```

A requested isolated Git fixture was blocked by the safety layer before execution. No safety, hook, signature or secret-check bypass was attempted. The negative message check demonstrates the installed validator rejects an invalid message; it is not a claim that a deliberately invalid commit was created or tested in hosted CI.

Windows checks remain for the owner to execute manually. `apollo.kyaulabs` was subsequently authorized for Linux/NVIDIA tests; read-only probing identified a GTX 1660 SUPER with driver 615.78.08. No infrastructure was provisioned. Hosted run outcomes and formal acceptance remain separate from this review.
