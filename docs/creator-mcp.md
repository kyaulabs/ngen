# Read-only creator MCP

The optional Linux companion serves one locally granted starter project over stdio. It does not install with the engine SDK or listen on the network. It cannot edit files, run builds, start playtests, or read arbitrary paths.

```sh
python3 -m venv .venv-mcp
.venv-mcp/bin/pip install -r tools/creator-mcp-requirements.txt
.venv-mcp/bin/python tools/creator_mcp.py --project /absolute/path/to/board
```

Configure the client to launch that command with absolute interpreter and script paths. The launcher selects the workspace; client-supplied roots are not grants. Trust the launcher and the local checkout. This is not host isolation for other programs or a service for untrusted remote clients.

Read `ngen://creator/project-schema` and `ngen://creator/sdk-guide` for the starter manifest and SDK workflow. Call `list_projects` for the opaque handle, then `inspect_project` with `{"handle":"…"}`. The handle lasts only for that process. Inspection returns the validated manifest name and schema, scene revision and entity count, and names and byte counts for referenced assets. It does not return source, asset contents, history, environment variables or absolute paths. With no saved scene, the revision is zero.

Failed inspections return `ok: false` with `unknown_project`, `project_busy` or `invalid_project`. Use `tools/project.py validate` locally for detailed workspace diagnostics. The project document and asset reads use the existing creator limits; the SDK guide is capped at 16 KiB.

The [official Python MCP SDK 2.3.0](https://github.com/modelcontextprotocol/python-sdk/releases/tag/v2.3.0) handles stdio and negotiation. The integration tests use revision `2026-07-28`. A separate SDK-only probe exercised `2025-11-25` over stdio; this is not a claim about any particular MCP host.

```sh
PYTHONDONTWRITEBYTECODE=1 .venv-mcp/bin/python -m unittest tests.creator_mcp -v
```
