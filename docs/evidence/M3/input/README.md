# Bounded live input value

`game/flight/input.hpp/.cpp` owns held controls and emits game intent/camera
events without a window, physics mutation, network or wall clock. Direct forward
is local-Z negative; opposing keys cancel. Explicit brake works in either mode.
Captured direct mouse motion controls a bounded virtual stick, held until release
or recentering rather than multiplied by render cadence. Tactical drag/wheel
produces consume-once camera deltas. Free look clears the prior steering stick
and changes camera orientation only; releasing it centers without resuming stale
turn input. Keyboard control remains independent of mouse free look.

Focus loss and explicit release clear keys, pointer request, stick and pending
camera motion, requesting look centering. Focus return does not restore them.
Changing view mode releases live input, while confirming the current mode is a
no-op. Invalid keys/mode/pointer/wheel data is rejected without changing state;
unfocused input cannot queue a hidden restart. Camera deltas stay inside the
separate rig's admitted range under a128-event flood. Apply centering before
new camera deltas. The value requests capture; SDL must still apply or reject it.

Eight public cases pass in four headless configurations: Clang debug/release,
strict ASan+UBSan+leaks and GCC. Four complete graphs pass8/8 each, followed by
final affected input reruns. Six actual-command Clang23 implementation/header
consumer invocations pass after three test-only reruns. Clang23's opt-in enum
checker diagnoses Catch2's combined false-assertion flags; `REQUIRE(!predicate)`
retains the same negative assertions without that vendor flag combination. No
check, warning or assertion is disabled. Initial diagnostic and six real runtime
Reds remain separate. GCC AST limitations remain unchanged; flags are not stripped.
Headless links exclude SDL/Vulkan/display libraries. Native text normalizes
trailing whitespace, with ignored originals intact.

Same-session Core/C++ review covers enum indices, finite bounds, state release,
held versus consume-once semantics, mode/free-look transitions and copies. No
remaining demonstrated P1/P2 defect was found in this value-only slice; it is not
independent review, concurrency proof or actual SDL/device evidence. Parent root
`0fd447b` passes hosted CI4/4, retained separately; this input source requires its
own exact-head check. Live event routing, focus/capture behavior, playable contact
preset/renderer, traces, audio, offline package, Windows, warmed measurements,
recording and owner control-feel acceptance remain. M3 is not complete.
