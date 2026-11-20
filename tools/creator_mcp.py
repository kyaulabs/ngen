"""Serve read-only NGEN creator discovery over local stdio."""

import argparse
import fcntl
import json
import os
from pathlib import Path
import secrets
from typing import TypedDict

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


def denied(code: str) -> Inspection:
    return {"ok": False, "project": None, "scene": None, "error": code}


def serve(directory: int) -> None:
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
            fcntl.flock(directory, fcntl.LOCK_SH | fcntl.LOCK_NB)
        except BlockingIOError:
            return denied("project_busy")
        except OSError:
            return denied("invalid_project")
        try:
            manifest = project.validate(project.project_files(directory))
            document = creator_scene.load(directory)
            entities = document["entities"]
            assets = creator_scene.assets(directory, entities)
            return {"ok": True, "project": manifest, "scene": {
                "revision": document["revision"], "entities": len(entities),
                "assets": [{"name": name, "bytes": len(data)} for name, data in assets.items()]},
                "error": None}
        except (OSError, ValueError):
            return denied("invalid_project")
        finally:
            fcntl.flock(directory, fcntl.LOCK_UN)

    server.run(transport="stdio")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project", type=Path, required=True, help="one absolute, explicitly granted starter root")
    args = parser.parse_args()
    if not args.project.is_absolute():
        parser.error("--project requires an absolute directory")
    try:
        directory = os.open(args.project, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    except OSError:
        parser.error("could not open the granted project directory")
    try:
        serve(directory)
    finally:
        os.close(directory)


if __name__ == "__main__":
    main()
