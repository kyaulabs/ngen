# Creator MCP

The optional Linux companion serves one locally granted starter project over stdio. It is read-only by default and does not install with the engine SDK or listen on the network. It cannot run builds, start playtests, or read arbitrary paths.

```sh
python3 -m venv .venv-mcp
.venv-mcp/bin/pip install -r tools/creator-mcp-requirements.txt
.venv-mcp/bin/python tools/creator_mcp.py --project /absolute/path/to/board
```

Configure the client to launch that command with absolute interpreter and script paths. The launcher selects the workspace; client-supplied roots are not grants. Trust the launcher and the local checkout. This is not host isolation for other programs or a service for untrusted remote clients.

Read `ngen://creator/project-schema` and `ngen://creator/sdk-guide` for the starter manifest and SDK workflow. Call `list_projects` for the opaque handle, then `inspect_project` with `{"handle":"…"}`. The handle lasts only for that process. Inspection returns the validated manifest name and schema, scene revision and entity count, and names and byte counts for referenced assets. It does not return source, asset contents, history, environment variables or absolute paths. With no saved scene, the revision is zero.

Failed inspections return `ok: false` with `unknown_project`, `project_busy` or `invalid_project`. Use `tools/project.py validate` locally for detailed workspace diagnostics. The project document and asset reads use the existing creator limits; the SDK guide is capped at 16 KiB.

## Opt-in authoring

Add `--allow-edits` to the launcher command to expose `edit_revisions` and `edit_project`. This grants the client preview **and commit** authority for the operations below, only in that project. Omit the flag for discovery alone. A tool call cannot add a grant or change the project root.

Call `edit_revisions` with the handle. Pass the returned revision for the selected operation as `expected` in `edit_project`:

```json
{"request":{"handle":"…","operation":"rename_project","expected":"<rename_project revision>","change":"new-board-name"}}
```

| Operation | `change` | Resulting diff |
|---|---|---|
| `rename_project` | A valid project slug; only `ngen-project.json` changes | Old and new names |
| `replace_source` | Nonempty UTF-8 text, at most 256 KiB; replaces only `main.cpp` | `line_range`: 1-based start, old line count, new line count for the changed span |
| `edit_scene` | A JSON string containing the [existing entity edit map](creator-scene.md), at most 256 KiB | Added, changed and removed entity IDs |

Preview is the default and does not reserve a revision. To save, send the same request with `apply: true`; the operation validates the input and rechecks `expected` under the write lock. File revisions are SHA-256 hashes of exact bytes; the scene revision is a decimal string so large values survive JSON clients. Failed execution returns `unknown_project`, `project_busy`, `stale_revision` or `invalid_edit`; malformed input may be rejected by the tool schema. No source or asset text is included in successful diffs.

The CLI `tools/project.py edit-revisions` and `tools/project.py edit --operation … --expect …` use the same operations. The CLI reads the change from stdin and saves by default; use `--preview` for its diff alone. Scene edit, undo and redo keep the same revision/history model. The lock coordinates the creator tools, not arbitrary filesystem editors. Saves replace a single file atomically with an owner-only file; they are not a power-loss durability or multi-file transaction guarantee. The tools reject symlinked or hard-linked project inputs. The launcher and direct workspace users must be trusted.

The adapter does not edit CMake files, write assets or active snapshots, or compile, run or validate the semantics of user source code. GLB metadata is data, not instructions. The per-operation limits apply after SDK framing; the local SDK transport has no NGEN pre-parse frame cap. Do not expose this stdio service to untrusted processes or remote callers.

The [official Python MCP SDK 2.3.0](https://github.com/modelcontextprotocol/python-sdk/releases/tag/v2.3.0) handles stdio and negotiation. The integration tests use revision `2026-07-28`. A separate SDK-only probe exercised `2025-11-25` over stdio; this is not a claim about any particular MCP host.

```sh
PYTHONDONTWRITEBYTECODE=1 .venv-mcp/bin/python -m unittest tests.creator_mcp -v
```
