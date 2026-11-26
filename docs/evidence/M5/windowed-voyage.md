# Windowed Surveyors checkpoint

On Linux, build `demo-release` with `NGEN_BUILD_QUEST_SCRIPT=ON` and the
[pinned Lua 5.5.1 dependency](saved-survey.md). Supply a compatible cooked
frigate and a separate, checked frontier landmark. Neither ships in the source
archive. The existing [M4 fixture](../M4/flight-acceptance.md) describes the
local frigate input and how to cook the public marker.

```sh
mkdir -m 700 charter
build/demo-release/flight_demo "$frigate" --survey=charter --destination="$marker" --mute
```

Use the **Surveyors charter** panel to learn, enlist and take a paid sortie.
The normal flight controls steer the same contact body that starts the crossing.
In tactical mode, **Approach berth** commands a return to the local anchor; let
the ship settle before choosing the sortie or return. Departures reject a
queued pilot command, an incorrect frame, or an active approach outside the
capture envelope. Rejected departure and entry cancellation preserve the
current body and pose. The envelope is ±0.5 m per axis, at most 0.2 m/s and
0.05 rad/s, with force equivalent to at most 0.25 m/s² and torque at most 5%
of the weakest authored axis limit. The fixture aligns the hull for transit
only after capture.

On arrival, the window keeps the authored asteroid contacts and displays the
streamed landmark for the frontier. The landmark is decorative; it does not
supply colliders. A new outbound leg waits for both the prepared scene and an
accepted native berth before publishing the fee. During transit and after ship
loss, the window holds the camera and scenery without drawing a live ship or
reporting its stale telemetry. The window resumes an already committed frontier
berth even if the decorative landmark is unavailable. Use **Retry landmark**
after repairing the file. The return uses the resident starter scene.

Mining, encounters and salvage are bounded charter actions, not live-aimed
weapons or a spatial mining prototype. The loss route replaces the vessel with
an empty starter; salvage moves the same wreck cargo. The station sells at the
committed arrival and rejects replays that change the historical action. Reopen
the same directory to continue from the last committed system and berth. In-flight
pose and pending travel are not saved.

## Record the owner playthrough

On KDE Wayland, the [private window recorder](../../../tools/record_flight/README.md)
can launch the charter and capture its window automatically. It does not pilot
or select charter actions for you.

```sh
python3 tools/record_survey.py --flight-demo=build/demo-release/flight_demo \
  --frigate="$frigate" --landmark="$marker"
```

Wait for `RECORDING` before taking control. The launcher creates a private
charter and captures only the launched game window, with no microphone or
system audio. Close the window to finalize `recording.webm`. At the default
20-minute limit the recorder finalizes the clip and closes the launched game;
your committed save is retained. Run the same command with `--save` set to the
printed charter directory to record a separate reload clip. Both clips stay
under `build/`. Recording alone does not close the milestone.

## Verification

For a bounded physical replay, use a fresh scratch directory. An interrupted
replay leaves actual checkpoints; open that directory interactively to inspect
or continue it, or use a new scratch directory to rerun the replay.

```sh
scratch=$(mktemp -d)
build/demo-release/flight_demo "$frigate" --survey="$scratch" --destination="$marker" \
  --replay --warmup=0 --frames=680 --width=640 --height=360 --mute --validation --motion-vectors
```

The replay submitted the same tick-queued charter actions after flying away,
steering back, crossing both directions, losing the ship, recovering and
salvaging. Tests ran on Linux x86-64 / Clang 23.1.5 with Iris Xe (TGL GT2),
Vulkan 1.4 driver `109064193`, Wayland and a fresh network namespace. Wayland
uses a local display connection; no gameplay network, account or database was
used. Audio device initialization was skipped in muted Surveyors runs.

| Run | Observed result |
|---|---|
| 680-frame validated replay | Revision 8, both native arrivals, 13 queued landmark frames and vessel 2. The player had 27 credits and no cargo; 3 ore reached the station and 9 remained in the deposit. |
| Corrupt landmark, no retry | Stopped at held exit with no local body or landmark. A separate reopen found revision 2 and 10 credits, so no departure fee was committed. |
| Corrupt landmark, then valid retry | `--destination-retry="$marker"` retried once; eight exit polls were recorded in the ASan/UBSan/LSan run. Final revision, cargo, credits and vessel matched the healthy run. |
| Reopen committed checkpoints | The starter reopened at revision 8 without crossing or paying again. The frontier reopened at revision 3 with 8 credits and a live body despite a corrupt landmark. |
| Public `scene_probe` | A GPU readback confirmed that the held view removed the package while retaining the extra geometry. |

All 39 CTests passed with the opt-in Release demo graph. Five targeted headless
and four opt-in demo ASan/UBSan/LSan CTests passed. The default demo still
rejects `--survey` before native startup. Clang 18 formatting, local clang-tidy
and actionlint passed. Red tests caught a replacement contact body, a powered
berth departure, an undrawn held-view flag and a decorative asset blocking an
already committed frontier checkpoint. Review also moved charter actions to the
tick boundary and checked that an approach rejects visibly without changing mode.

This is not B4 acceptance. The runs establish neither one-hour memory stability
nor market p95 separate from save latency. Independent gameplay and economy
review and a recorded owner playthrough are still required.
