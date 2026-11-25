# B4 one-hour technical soak

Use the Linux opt-in `demo-release` target and pinned Lua from the
[windowed voyage guide](windowed-voyage.md). The reference run uses the X1
Carbon's i7-1185G7 / Iris Xe (TGL GT2), Wayland, 1280×720, muted audio, and the
same checked private frigate and public landmark as the M4 flight fixture.
Measure timings in optimized unsanitized code. Run Vulkan validation and
sanitizers separately on a shorter correctness run.

```sh
work=$(mktemp -d)
timeout -k 10s 3660s unshare --user --map-root-user --net -- env SDL_VIDEO_DRIVER=wayland \
  build/demo-release/flight_demo "$frigate" --destination="$marker" \
  --survey="$work/charters" --soak-seconds=3600 --width=1280 --height=720 \
  > "$work/soak.log" 2>&1 && python3 tools/b4_report.py "$work/soak.log"
```

The scratch root must not exist and its parent must be owned and not writable
by others. The application creates the root exclusively, keeps its descriptor
and retains the charter saves. Do not point the workload at a real save. A
short `--soak-seconds=90` run exercises the loop but cannot pass the one-hour
trace checker. Do not minimize or close the window during measurement. The
external timeout bounds hangs outside the game's frame loop. Keep Wayland
local display IPC distinct from game networking: the run has no game server,
account, database, or IP network access and initializes no audio device.

The automatic pilot leaves and returns to the berth on every sortie. The
same window and Vulkan context stay open for one actual hour. Each charter
alternates loss/recovery and victory, sells all 12 units across four sorties,
and rejects another departure once exhausted. Every saved action checks 12 ore,
130 credits, and the expected vessel identity; loss, salvage and replacement
also check their cargo ownership. After each sale the workload reopens the save,
compares the state and system, retries both travel receipts and rejects a changed
retry without changing the revision or crossing again. The first completed
finite charter ends with 66 credits, all 12 ore at the station, and vessel 3.

The report requires 100 or more admitted sales, every sale reopened, at least
one of each encounter result, full elapsed time, and no sale gap over 45 seconds.
The market timer wraps the actual berth-gated trade during the arrival commit
attempt. Persistence separately measures encoding/publication and directory
sync; it includes installing the staged state. Neither timer includes the
arrival, preview, reopen or historical retries. Both report nearest-rank
p50/p95/p99 and max. Market p95 must not exceed 20 ms; persistence has no B4
latency limit. A failed commit attempt is logged and stops the workload.

Sample process RSS and the renderer's tracked GPU allocator and resource
records about once per elapsed second. Require at least 3,565 samples, with
no gap over five seconds and coverage within 1.5 seconds of the end. Use the
existing reference ceilings of 6 GiB RSS and 1 GiB tracked GPU reservation;
do not add them on an integrated GPU. Compare settled starter samples with
no pending upload or retiring records at minutes 5–10 and 55–60, at least 120
in each window. Late p95 may grow by at most 32 MiB RSS and 8 MiB each in tracked
GPU allocation/reservation, with no growth in p95 live records. These are
fixture-specific drift budgets, not proof that no leak is possible.

Keep the raw log, checker JSON, exit status, source/executable/dependency and
asset identities, device/driver, OS/CPU/memory, power/thermal observations,
and the standalone invalid-save and invalid-script checks. A technical pass
supports M5 but does not replace the independent gameplay/economy review or
recorded owner playthrough.
