# Saved native voyage checkpoint

`quest_voyage` is a Linux headless fixture built with `NGEN_BUILD_PHYSICS=ON`
and `NGEN_BUILD_QUEST_SCRIPT=ON` (Jolt and Lua 5.5.1). It runs the Surveyors
campaign with a native crossing from the protected starter to the lawless
frontier and back. It does not use the flight demo window or private assets.

```sh
mkdir -m 700 voyage-save
build/flight-release/quest_voyage voyage-save status
build/flight-release/quest_voyage voyage-save step 0 law
build/flight-release/quest_voyage voyage-save step 1 join
build/flight-release/quest_voyage voyage-save step 2 venture
```

Use the revision and `next:` actions printed after each command. New legs
require a prepared source berth. The fixture CLI supplies the two known-empty
collision profiles. The native journey must reach its exit and admit the
prepared destination before the quest commits the action. Missing, stale or
occupied contacts do not charge the outbound fee or publish the return sale.
While a leg is open, other quest commands (including replays) return busy.

The last committed campaign stage determines the checkpoint system after
restart. Transit itself is not a durable state: closing before arrival leaves
the source checkpoint, and a failed pre-rename save requires another trip.
An exact retry after publication returns the original receipt without crossing
or paying again. Voyage saves use a separate file and magic from `quest_survey`.
The campaign vessel generation persists across legs and reload, and changes on
recovery. Native Entity and body IDs are local to the journey; the live tick is
not saved.

Local Linux x86-64 verification used Clang 23.1.5 with Jolt 5.6.0 and Lua
5.5.1. The native arrival test first failed because the action committed without
starting a journey; the CLI test first failed because status had no committed
system. The combined Release preset passed 20 CTests. Travel, native voyage
and the voyage CLI also passed in a fresh network namespace with no display
environment. Five affected CTests passed under ASan, UBSan and leak detection
with instrumented Lua. Review led to requiring prepared departure contacts,
withdrawing stale native snapshots on save failure, and making campaign identity
explicit on native frames.

This is not B4 acceptance. No live flight controls, scene streaming, one-hour
resource or trade-latency evidence, independent gameplay/economy review, or
recorded owner playthrough is established by this checkpoint.
