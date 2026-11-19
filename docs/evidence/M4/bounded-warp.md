# Bounded warp travel (M4-D)

[#14](https://github.com/kyaulabs/ngen/issues/14) implements one headless,
owner-thread journey between authored berths. `game/travel/` uses the existing
flight owner for local controls and Jolt contacts. Transit owns a logical vessel
and a split-coordinate trajectory, not a fast-moving rigid body. The interactive
flight demo does not expose a warp command. ADR 0004 remains Proposed.

`Journey::create` copies an authorized route and vessel definition. Jump
capability, route permission and security permission all default to deny. Pilot
commands cannot set those facts. The command names the session-local vessel,
exact barrier tick and increasing sequence. Entry requires an empty command
queue and a stationary tactical vessel at the aligned source berth. `cancel`
keeps the same local body until the next `step` commits and retires it. Commitment
clears local flight publication; transit samples the linear path on explicit
60 Hz ticks. The path is bounded to 128 cells per axis and 1–216,000 transit ticks.
Integer quotient/remainder splitting retains large cell indices without forming
an absolute double. The bound is separate from local physics limits.

At the last transit tick, the vessel waits at the destination anchor without a
local body. Exit requires the active trip, the exact destination and the
collision revision supplied by the same owner. Null data, a stale identity or
revision, malformed collision boxes and a blocked berth cannot publish a body.
A proxy box must be separated from the arrival hull by more than 50 mm on at
least one axis. The new world is prepared before it is published. The logical
vessel, mass and session tick persist; the native body and frame belong to the
new domain. Once published, replays cannot replace the arrival. An explicitly
prepared empty site is valid. A missing site is not.

`ctest -R '^travel$'` exercises five cases. Behavioral Reds observed a missing
astronomical midpoint, three unauthorized entries, a rejected pre-commit cancel,
and an unavailable exit even when legitimate collision data was supplied. The
final native path crosses 36 cells plus 1.5 m (154,618,822,657.5 m, about 1.03 AU)
near a galactic-scale origin. It also samples at signed cell limits. Arrival
preserves a changed 1,200 kg mass and starts from matching Flight/World/body tick
7. A stale retry and a fresh duplicate request preserve the same arrived body.
The arrived ship subsequently moves under its ordinary flight controller toward
x = 20 m, but a copied static box stops its center within 1.9–2.1 m after 240
local steps. This checks native destination contact, not just a readiness flag.

Linux x86-64 with the locked double-position Jolt 5.6.0: Clang Release, GCC Debug
and strict ASan/UBSan/LSan headless CTests each passed 9/9. The full Release demo
graph passed 28/28. The travel test ELF has no graphics or window dependency.
No Windows runtime, graphical warp, streaming, live permission service, moving
entry or arbitrary contact-group transfer is claimed. M4-E owns preparation and
streaming integration; integrated M4 acceptance remains open.
