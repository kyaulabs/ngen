# Shared Context overlay frontend

`Gui` extracts the existing ImGui/SDL context, bitmap atlas, bounded CPU draw
storage, allocation accounting and overlay conversion from `InspectorUi`.
Inspector widgets delegate to that same implementation; the replaced ownership/
conversion code is removed. Custom flight HUD drawing can use begin/end and the
existing Context post/overlay path. There is no ImGui Vulkan backend, second GPU
resource owner, new dependency/pass or changed inspector layout/style/font.

The actual native Red rejects generic custom frontend creation. After extraction,
custom fullscreen red geometry reaches native pixels; original inspector
material/overlay/bounds/resize controls remain green before and after delegation.
A second active ImGui owner rejects without breaking the first; sequential
inspector/custom contexts cleanly reuse the SDL window. GUI objects and atlas
owners end before explicit Context/window close. Allocation callbacks restore the
previous allocator, and returned bounded overlay spans retain their original
call-lifetime contract. Initial missing-header and64-pixel-vs-RGB assertion shape
compilation failures are setup errors, not Reds.

Six complete scene graphs pass69 CTests, including shipping/shader-disabled
consumers. Four serial native Clang debug/release/strict ASan+UBSan+leaks and GCC
runs pass with empty stderr and custom/exclusive/sequential/inspector markers.
Nine actual native Clang commands cover the new header's three consumers in
three configurations; all pass with flags intact. GCC native passes; unchanged
GCC AST limitations remain. Same-session Core/C++ review checks ownership,
callback restoration, borrowed geometry, bounds, extraction parity and explicit
window-close ordering. No remaining demonstrated P1/P2 was found in this shared
frontend; not independent review, arbitrary misuse or allocation-failure proof.

Input `c87d1e1` passes hosted CI4/4, retained separately from this later GUI tree.
This source needs its own exact-head CI. Text records normalize trailing
whitespace; ignored originals remain. The actual flight scene, SDL live routing,
force/velocity HUD/traces, audio, offline package, current Windows, warmed
measurements, recording and owner control-feel acceptance remain. M3 is not
complete and accepted M2 art is not reopened.
