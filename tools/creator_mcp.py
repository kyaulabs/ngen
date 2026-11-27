"""Serve NGEN creator discovery and separately granted local operations over stdio."""

import argparse
import fcntl
import json
import os
from pathlib import Path
import secrets
from typing import Annotated, Literal, TypedDict

from pydantic import BaseModel, ConfigDict, Field

from mcp.server.mcpserver import MCPServer
from mcp.types import ToolAnnotations

import creator_jobs
import creator_play_session
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


class JobRevision(TypedDict):
    revision: str | None
    error: str | None


Token = Annotated[str, Field(max_length=32)]
Revision = Annotated[str, Field(max_length=64)]


SceneRevision = Annotated[int, Field(ge=1, le=creator_scene.MAX_REVISION)]


class StartPlaytest(BaseModel):
    model_config = ConfigDict(extra="forbid", strict=True)
    handle: Token
    revision: SceneRevision


class PlaytestRequest(BaseModel):
    model_config = ConfigDict(extra="forbid", strict=True)
    handle: Token
    session: Token
    generation: int = Field(ge=1, le=creator_scene.MAX_REVISION)


class ReloadPlaytest(PlaytestRequest):
    revision: SceneRevision


def serve(directory: int, allow_edits: bool, jobs: creator_jobs.Runner | None,
          play: creator_play_session.Runner | None) -> None:
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

    if jobs is not None:
        job_write = ToolAnnotations(readOnlyHint=False, destructiveHint=True, openWorldHint=False)

        @server.tool(annotations=read_only)
        def job_revision(handle: Token) -> JobRevision:
            """Fingerprint the saved source and assets eligible for a job."""
            if handle != granted_handle:
                return {"revision": None, "error": "unknown_project"}
            try:
                revision, _ = creator_jobs.snapshot(directory)
                return {"revision": revision, "error": None}
            except creator_scene.Busy:
                return {"revision": None, "error": "project_busy"}
            except (OSError, ValueError):
                return {"revision": None, "error": "invalid_project"}

        @server.tool(annotations=job_write)
        def start_job(handle: Token, kind: creator_jobs.JobKind, expected: Revision) -> creator_jobs.JobReply:
            """Run a fixed offline creator job at the expected saved revision."""
            if handle != granted_handle:
                return creator_jobs.denied("unknown_project")
            return jobs.start(directory, kind, expected)

        @server.tool(annotations=read_only)
        def job_status(handle: Token, job: Token) -> creator_jobs.JobReply:
            """Report status without logging or returning project-controlled output."""
            if handle != granted_handle:
                return creator_jobs.denied("unknown_project")
            return jobs.status(job)

        @server.tool(annotations=job_write)
        def cancel_job(handle: Token, job: Token) -> creator_jobs.JobReply:
            """Request cancellation of the one owned job; poll for its terminal outcome."""
            if handle != granted_handle:
                return creator_jobs.denied("unknown_project")
            return jobs.status(job, cancel=True)

    if play is not None:
        play_write = ToolAnnotations(readOnlyHint=False, destructiveHint=False, openWorldHint=False)

        @server.tool(annotations=play_write)
        def start_playtest(request: StartPlaytest) -> creator_play_session.Reply:
            """Start the one granted board playtest at a saved scene revision."""
            if request.handle != granted_handle:
                return creator_play_session.denied("unknown_project")
            return play.start(request.revision)

        @server.tool(annotations=read_only)
        def inspect_playtest(request: PlaytestRequest) -> creator_play_session.Reply:
            """Inspect the active generation of this process's owned board playtest."""
            if request.handle != granted_handle:
                return creator_play_session.denied("unknown_project")
            return play.command(request.session, request.generation, "inspect")

        @server.tool(annotations=play_write)
        def reload_playtest(request: ReloadPlaytest) -> creator_play_session.Reply:
            """Advance the owned playtest to a newer saved board revision."""
            if request.handle != granted_handle:
                return creator_play_session.denied("unknown_project")
            return play.command(request.session, request.generation, "reload", request.revision)

        @server.tool(annotations=play_write)
        def stop_playtest(request: PlaytestRequest) -> creator_play_session.Reply:
            """Stop and reap this process's owned board playtest."""
            if request.handle != granted_handle:
                return creator_play_session.denied("unknown_project")
            return play.command(request.session, request.generation, "stop")

    server.run(transport="stdio")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project", type=Path, required=True, help="one absolute, explicitly granted starter root")
    parser.add_argument("--allow-edits", action="store_true", help="grant bounded local edits")
    parser.add_argument("--allow-jobs", action="store_true", help="grant bounded isolated jobs")
    parser.add_argument("--job-sdk", type=Path, help="audited, relocatable local SDK/dependency prefix")
    parser.add_argument("--job-tools", type=Path, help="audited directory with assetc, ktx and probe")
    parser.add_argument("--job-output", type=Path, help="existing directory for local source archives")
    parser.add_argument("--job-seconds", type=int, help="job deadline, 1–90 seconds (default: 90)")
    parser.add_argument("--allow-play", action="store_true", help="grant one isolated board playtest")
    for name in creator_play_session.TOOLS:
        parser.add_argument(f"--play-{name}", type=Path, help=f"trusted absolute board tool path: {name}")
    args = parser.parse_args()
    if not args.project.is_absolute():
        parser.error("--project requires an absolute directory")
    try:
        directory = os.open(args.project, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    except OSError:
        parser.error("could not open the granted project directory")
    jobs = play = None
    try:
        profile = (args.job_sdk, args.job_tools, args.job_output)
        if args.allow_jobs:
            if not all(profile):
                parser.error("--allow-jobs requires --job-sdk, --job-tools and --job-output")
            try:
                jobs = creator_jobs.Runner(sdk=args.job_sdk, tools=args.job_tools, output=args.job_output,
                                           seconds=90 if args.job_seconds is None else args.job_seconds)
            except (OSError, ValueError):
                parser.error("could not establish the local creator job profile")
        elif any(profile) or args.job_seconds is not None:
            parser.error("job profile options require --allow-jobs")
        tools = {name: getattr(args, f"play_{name}") for name in creator_play_session.TOOLS}
        if args.allow_play:
            if not all(tools.values()):
                parser.error("--allow-play requires all four --play-* tool paths")
            try:
                play = creator_play_session.Runner(directory, tools)
            except (OSError, ValueError):
                parser.error("could not establish the local board playtest profile")
        elif any(tools.values()):
            parser.error("board playtest tool paths require --allow-play")
        serve(directory, args.allow_edits, jobs, play)
    finally:
        try:
            if play is not None:
                play.close()
        finally:
            try:
                if jobs is not None:
                    jobs.close()
            finally:
                os.close(directory)


if __name__ == "__main__":
    main()
