# Creator MCP

The optional Linux companion serves one locally granted starter project over stdio. It is read-only by default and does not install with the engine SDK or listen on the network. It cannot read arbitrary paths. Separate launcher grants enable bounded editing, offline jobs, or an owned board playtest on a supported Linux host.

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

## Opt-in offline jobs

See the [creator jobs guide](creator-jobs.md) for the separate `--allow-jobs` grant, host prerequisites, fixed job operations, and cancellation. `--allow-edits` does not grant execution, and a job grant does not grant edits or playtest control.

## Opt-in board playtest

Use the existing [headless board playtest](creator-play.md) tools and Linux Bubblewrap prerequisites. Add `--allow-play` and provide trusted, absolute executable paths outside the granted project:

```sh
.venv-mcp/bin/python tools/creator_mcp.py --project /absolute/path/to/board \
  --allow-play --play-assetc /absolute/path/to/assetc \
  --play-ktx /absolute/path/to/ktx --play-probe /absolute/path/to/asset_package_probe \
  --play-player /absolute/path/to/creator_player
```

The tools are not exposed without this grant. Neither `--allow-edits` nor `--allow-jobs` grants play; `--allow-play` does not grant those operations. The launcher must trust the checkout, executable inputs and host. The client cannot select a path, PID, binary, script, flight session or server. The board runtime uses staged scene data, not the project's C++ or CMake code. There is no unsandboxed fallback.

Send each tool's arguments in `request`, using the project `handle` from `list_projects`:

| Tool | Other request fields | Effect |
|---|---|---|
| `start_playtest` | `revision` | Start one board at the specified saved scene revision. Return an opaque `session` and generation 1. |
| `inspect_playtest` | `session`, `generation` | Read a snapshot of the current board without applying saved edits. |
| `reload_playtest` | `session`, `generation`, `revision` | Run the same checked board tick on a newer saved revision. Rejected scene data leaves the accepted snapshot active. |
| `stop_playtest` | `session`, `generation` | Stop and reap the owned playtest. The handle cannot be reused. |

Revisions and generations are JSON integers. A successful snapshot contains `generation`, `revision`, `tick`, `marker_mm`, `instances` and `asset_cooks`. The client sends the current generation back for each operation. Unknown projects/sessions, a stale generation and a second active start are denied. Invalid scene or cook inputs report `play_start_failed` or `play_rejected`; use the local CLI for detailed diagnostics. The MCP response does not forward worker logs, paths or project content.

The companion admits one operation at a time and rejects overlap as `play_busy`. It reuses the CLI's 60-second cook deadline, 10-second native reply deadline, 300-second idle limit and 128-command budget. Closing stdio requests normal shutdown after any admitted operation. A failed retirement is not reported as stopped. The native sandbox has a cleared environment and private network/process namespaces. This is not a general untrusted-code service or a wall-clock guarantee for blocked host I/O. No signals are addressed by client input.

To exercise the real play tools along with the default stdio suite, set `NGEN_MCP_PLAY_BUILD` to the absolute asset build directory and `NGEN_MCP_PLAY_KTX` to its configured KTX executable. Without those inputs the optional native play cases are skipped.

The [official Python MCP SDK 2.3.0](https://github.com/modelcontextprotocol/python-sdk/releases/tag/v2.3.0) handles stdio and negotiation. The integration tests use revision `2026-07-28`. A separate SDK-only probe exercised `2025-11-25` over stdio; this is not a claim about any particular MCP host.

```sh
PYTHONDONTWRITEBYTECODE=1 .venv-mcp/bin/python -m unittest tests.creator_mcp -v
```
