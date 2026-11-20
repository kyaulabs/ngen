# Headless board playtest

The Linux starter can play, inspect and reload one saved board in an owned
foreground session. It does not compile or run project code. The playtest
loads checked cooked geometry and runs the starter's 42 mm move on one real
simulation tick per accepted revision. It is not a graphical editor or a
continuously running game.

Prepare the optional [asset-tool build](creator-project.md) with `assetc`,
`asset_package_probe` and `creator_player`. Supply the `ktx` executable from
that build's locked KTX dependency. Play requires `/usr/bin/bwrap`, Linux user
and network namespaces, and `/usr/bin/python3`. There is no unsandboxed fallback.
Choose trusted executable inputs explicitly; project files cannot choose them.

```sh
python3 tools/project.py play /absolute/path/to/board --expect 1 \
  --assetc /absolute/path/to/assetc \
  --ktx /absolute/path/to/ktx \
  --probe /absolute/path/to/asset_package_probe \
  --player /absolute/path/to/creator_player
```

The saved scene must have exactly one entity with an embedded asset and a
transform with only X translation, identity rotation and unit scale. The
position converts to millimetres by truncation. A camera, light, material
override, multiple entities or an unsaved scene is rejected, not ignored.

Send newline-terminated JSON on the session's stdin. The startup reply
includes the active `generation`, saved `revision`, simulated `tick`,
`marker_mm`, checked instance count and the number of attempted `asset_cooks`.
Save edits or undo in another shell through the [scene commands](creator-scene.md).

```json
{"op":"inspect"}
{"op":"reload","expect":1,"revision":2}
{"op":"stop"}
```

`expect` is the active play generation; `revision` is the newer saved scene
revision. All file and asset reads use the existing scene validators under a
shared workspace lock. Cook outside the lock, using only the captured bytes.
Identical asset bytes reuse the session's checked package, but the new board
still runs a native tick. Failed cooks, invalid edits and stale reloads leave
both the last admitted play snapshot and the saved project intact. A successful
undo saves a new revision and can be loaded normally. The play session never
saves back to the project. Stop or stdin EOF closes the owned worker.

The native worker sees a read-only staging directory. The isolated cook sees
only its separate writable staging directory, approved tools and read-only
system files. Neither mounts the project or home directory; children use a
cleared environment and private network namespace. The foreground command
bounds request size/count, idle time and child deadlines. This is a scoped
local tool for trusted binaries, not a sandbox for arbitrary project executables,
a remote execution service or an MCP grant.

## Verification

For the same four saved board positions with an unchanged marker, the manual
workflow performed four native cook-and-verify passes (87–105 ms each on the
local host). Owned play and reload performed one cook; all four native snapshots
reported the expected X position plus 42 mm. Disabling the reuse check brought
the count back to four and failed the regression. Those counts measure the
avoided work, not an end-to-end latency or performance guarantee.

The Linux bubblewrap 0.13.1 fixture exercised play/stop, stale generation,
failed geometry cook, native package rejection, undo/save and invalid edits.
A controlled sandbox check observed a cleared environment, hidden outside
file, private network namespace and read-only runtime stage. Run the real
sandbox cases with `NGEN_PLAY_SANDBOX_TEST=1 ctest -R '^creator_project$'` in
the configured optional asset build with SDK installation enabled. The native
worker corruption/reload test also runs in `asset_cook_cli` without bubblewrap.
No interactive renderer or Windows playtest is claimed.
