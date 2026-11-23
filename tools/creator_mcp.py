"""Serve NGEN creator discovery and opt-in bounded edits over local stdio."""

import argparse
import fcntl
import json
import os
from pathlib import Path
import secrets
from typing import Literal, TypedDict

from pydantic import BaseModel, ConfigDict, Field

from mcp.server.mcpserver import MCPServer
from mcp.types import ToolAnnotations

import creator_scene
import project


class ProjectInfo(TypedDict):
    name: str
    schema: int


class AssetInfo(TypedDict):
    name: str
    bytes: int


class SceneInfo(TypedDict):
    revision: int
    entities: int
    assets: list[AssetInfo]


class Inspection(TypedDict):
    ok: bool
    project: ProjectInfo | None
    scene: SceneInfo | None
    error: str | None


class Projects(TypedDict):
    handles: list[str]


class EditRequest(BaseModel):
    model_config = ConfigDict(extra="forbid", strict=True)
    handle: str = Field(max_length=32)
    operation: Literal["rename_project", "replace_source", "edit_scene"]
    expected: str = Field(max_length=64)
    change: str = Field(max_length=creator_scene.MAX_BYTES)
    apply: bool = False


class EditResponse(TypedDict):
    ok: bool
    applied: bool
    before: str | None
    after: str | None
    diff: dict[str, list[str] | list[int]] | None
    error: str | None


class Revisions(TypedDict):
    ok: bool
    revisions: dict[str, str] | None
    error: str | None


def edit_denied(code: str) -> EditResponse:
    return {"ok": False, "applied": False, "before": None, "after": None, "diff": None, "error": code}


def denied(code: str) -> Inspection:
    return {"ok": False, "project": None, "scene": None, "error": code}


def serve(directory: int, allow_edits: bool) -> None:
    granted_handle = secrets.token_hex(16)
    server = MCPServer("ngen-creator", subscriptions=False)
    read_only = ToolAnnotations(readOnlyHint=True, destructiveHint=False, openWorldHint=False)

    @server.resource("ngen://creator/project-schema", mime_type="application/schema+json")
    def project_schema() -> str:
        """Schema for the starter's ngen-project.json manifest."""
        return json.dumps({"$schema": "https://json-schema.org/draft/2020-12/schema",
                           "type": "object", "required": ["schema", "name"],
                           "additionalProperties": False,
                           "properties": {"schema": {"type": "integer", "const": 1},
                                          "name": {"type": "string", "pattern": project.PROJECT_NAME}}})

    @server.resource("ngen://creator/sdk-guide", mime_type="text/markdown")
    def sdk_guide() -> str:
        """Installed SDK targets and the supported consumer build workflow."""
        with (project.ROOT / "docs/creator-sdk.md").open("rb") as guide:
            data = guide.read(16385)
        if len(data) > 16384:
            raise ValueError("SDK guide exceeds its resource budget")
        return data.decode("utf-8")

    @server.tool(annotations=read_only)
    def list_projects() -> Projects:
        """List handles for projects granted by the local launcher."""
        return {"handles": [granted_handle]}

    @server.tool(annotations=read_only)
    def inspect_project(handle: str) -> Inspection:
        """Summarize the validated starter project and its active saved scene."""
        if handle != granted_handle:
            return denied("unknown_project")
        try:
            with creator_scene.locked(directory, fcntl.LOCK_SH) as fd:
                manifest = project.validate(project.project_files(fd))
                document = creator_scene.load(fd)
                entities = document["entities"]
                assets = creator_scene.assets(fd, entities)
                return {"ok": True, "project": manifest, "scene": {
                    "revision": document["revision"], "entities": len(entities),
                    "assets": [{"name": name, "bytes": len(data)} for name, data in assets.items()]},
                    "error": None}
        except creator_scene.Busy:
            return denied("project_busy")
        except (OSError, ValueError):
            return denied("invalid_project")

    if allow_edits:
        @server.tool(annotations=read_only)
        def edit_revisions(handle: str) -> Revisions:
            """Return revision strings for the supported edits without returning file contents."""
            if handle != granted_handle:
                return {"ok": False, "revisions": None, "error": "unknown_project"}
            try:
                return {"ok": True, "revisions": project.edit_revisions(directory), "error": None}
            except creator_scene.Busy:
                return {"ok": False, "revisions": None, "error": "project_busy"}
            except (OSError, ValueError):
                return {"ok": False, "revisions": None, "error": "invalid_project"}

        @server.tool(annotations=ToolAnnotations(readOnlyHint=False, destructiveHint=True, openWorldHint=False))
        def edit_project(request: EditRequest) -> EditResponse:
            """Preview or apply a bounded edit to the granted project at an expected revision."""
            if request.handle != granted_handle:
                return edit_denied("unknown_project")
            try:
                return {"ok": True, **project.edit(directory, request.operation, request.expected,
                                                  request.change.encode("utf-8"), apply=request.apply),
                        "error": None}
            except creator_scene.Busy:
                return edit_denied("project_busy")
            except creator_scene.StaleRevision:
                return edit_denied("stale_revision")
            except (OSError, ValueError):
                return edit_denied("invalid_edit")

    server.run(transport="stdio")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project", type=Path, required=True, help="one absolute, explicitly granted starter root")
    parser.add_argument("--allow-edits", action="store_true", help="grant bounded local edits")
    args = parser.parse_args()
    if not args.project.is_absolute():
        parser.error("--project requires an absolute directory")
    try:
        directory = os.open(args.project, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    except OSError:
        parser.error("could not open the granted project directory")
    try:
        serve(directory, args.allow_edits)
    finally:
        os.close(directory)


if __name__ == "__main__":
    main()
