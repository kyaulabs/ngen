# Author a starter scene

C1-C extends the [standalone project CLI](creator-project.md) on Linux. It saves
one small scene of named entities with transforms, optional perspective cameras,
point lights, material overrides and embedded GLB references. It neither runs
project code nor changes the editor or runtime. The authored document is not an
NGAS package, and the starter does not render its camera, lights or overrides yet.

From the NGEN checkout, use a new workspace you own:

```sh
workspace=$(mktemp -d)
python3 tools/project.py new "$workspace/board" --name board-scene
python3 tools/project.py scene show "$workspace/board"
python3 tools/project.py scene edit "$workspace/board" --expect 0 <<'JSON'
{
  "board": {
    "transform": {"position_m": [0,0,0], "rotation_xyzw": [0,0,0,1], "scale": [1,1,1]},
    "asset": "marker.glb",
    "material": {"base_color": [0.9,0.4,0.2,1], "metallic": 0, "roughness": 0.5}
  },
  "view": {
    "transform": {"position_m": [0,2,4], "rotation_xyzw": [0,0,0,1], "scale": [1,1,1]},
    "camera": {"vertical_fov_deg": 55, "near_m": 0.1, "far_m": 100}
  },
  "key": {
    "transform": {"position_m": [3,3,2], "rotation_xyzw": [0,0,0,1], "scale": [1,1,1]},
    "light": {"color": [1,0.8,0.6], "lumens": 500}
  }
}
JSON
python3 tools/project.py scene undo "$workspace/board" --expect 1
python3 tools/project.py scene redo "$workspace/board" --expect 2
python3 tools/project.py scene show "$workspace/board"
python3 tools/project.py validate "$workspace/board"
python3 tools/project.py package "$workspace/board" --output "$workspace/board-source.tar.gz"
```

`edit` takes a JSON map on stdin: each key is a lowercase entity slug of 1–40
characters; each value replaces the whole entity, or is `null` to remove it.
Edits in one map commit together. `show` returns the schema, revision and active
entities, including revision 0 and an empty scene for a new project. On success,
each edit, undo or redo advances the revision once. Reload with `show` before
retrying a stale edit. The saved schema-1 document `ngen-scene.json` keeps at most
eight prior/redo snapshots in total. A new edit after undo discards the redo chain.

| Field | Boundary |
|---|---|
| Entities | At most 64; required `transform`; optional `asset`, `camera`, `light`, `material` |
| Transform | Local positions within ±8,192 m per axis, unit XYZW quaternion (squared-norm tolerance `1e-5`), positive scales in `[0.001, 100]` |
| Camera | Vertical field of view `[1, 179]` degrees; `0.001 ≤ near_m < far_m ≤ 8192` |
| Light | Linear RGB `[0, 1]`; `lumens` in `[0, 100000]` |
| Material | Requires an asset; linear base RGBA, metallic and roughness values in `[0, 1]` |
| Referenced assets | `marker.glb` or `asset-<slug>.glb` at the project root; at most eight active assets totaling at most 16 MiB |
| Edit and save | At most 256 KiB each; schema and collection bounds checked on reload |

To reference another asset, place an embedded GLB v2 at an admitted root name
and set its name in `asset`. Unsafe names, missing/nonregular files, symlinks,
malformed GLB chunks and URI-based buffers/images are rejected. The authoring
check admits the embedded container, not its geometry, images or materials.
Run the existing `assetc inspect` and [offline cooker](creator-project.md) to
check those native inputs separately. `show` can still inspect a scene after an
asset is removed; a mutation or package must resolve every active reference.

Writers to the same directory serialize through a nonblocking Linux directory
lock and compare revisions before publishing. The CLI writes and syncs a new file
then renames it over the old one. Invalid input, a stale revision, competing CLI
writer or a pre-publication write failure leaves the prior snapshot intact. This
is atomic replacement in a local tool, not a power-loss durability or hostile
workspace guarantee. Do not directly edit workspace files concurrently.

Packaging the untouched starter still includes the original five files. An
edited project also contains the active authored document and only its current
admitted asset references. The exported scene retains its revision but not the
local undo/redo history. The resulting archive can reload the scene after
extraction, but does not contain cooked files, an executable, unrelated
workspace files or retired assets.

The native `creator_project` CTest exercises the public CLI and packages and
reloads an authored scene, then inspects, cooks and runs its referenced public
board marker with the installed starter SDK. It verifies observable edit/reload,
undo/redo across processes, stale and invalid input, and non-overwriting package
behavior. Clang 23.1.4 Release and strict Debug ASan/UBSan with leak detection on
Linux passed the 10-test asset/SDK suites. The native consumer reads the cooked
mesh, not the authoring document. Owned play/inspection/reload remains C1-D;
MCP, GUI authoring and independent C1 acceptance are separate.
