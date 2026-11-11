# Long replay framing regression

The first2400-frame B1 attempt on packaged `19acb3c` failed honestly:
`flight replay produced no lit world pixels`. Its1800-tick direct-turn sample
remained in mounted first person through the end. After leaving the local scenery,
the final view was empty space. No benchmark timing was accepted and no bad frame
was exempted. Short330-frame walkthroughs did not exercise this late phase.

The fixed-input fixture now returns to tactical mode at tick1980, after the direct
turn sample, keeping the detailed hull in the benchmark tail. This changes only
`--replay` intent: no interactive control, camera math, navigation/physics rule,
asset, rendering pass, shading parameter or lit-world detector changed.

The same2400-frame native Release command now succeeds with lit final HDR,
2400 GPU timing rows, finite traces, empty stderr and zero measured C++/ImGui
allocations. At the transition the entity generation is unchanged, displacement
is bounded and velocity follows the admitted interval force rather than receiving
a reset/impulse. Tactical navigation may accelerate immediately; an initial checker
incorrectly assumed enum order and constant velocity. Correcting that checker did
not change production physics or its tests.

Four21/21 graphs and three complete native application Clang commands pass after
the repair. The long native regression uses the dummy audio device and is not a
listening or timing result. See retained logs/results; the earlier failure remains.
Final delivery and accepted laptop/RTX measurements must use the repaired source.
This is a same-session scoped review, not independent review or M3 acceptance.
