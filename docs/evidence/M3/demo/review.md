# Same-session playable bridge review

Reviewed the new application, preset/presentation helpers, public tests, combined
lock/presets and CI consumers against M3 and existing Context/Flight/Gui contracts.
This is assistant-operated Core/C++ review, not independent human review.

Fixed demonstrated findings:

- `preset.cpp`: a smaller/mirrored/sheared/projective authored root could satisfy
  the loose hull enclosure test. Proper-rigid/finite admission now precedes bound
  transformation; the public fixture Red passes without renderer changes.
- `app.cpp` focus routing: F always reapplied selected focus. The second native
  toggle moved the eye0m instead of returning to the ship. Track copied focus
  intent and clear it on reset/clearance fallback; both toggles now move only
  presentation, never the physical hull.
- `app.cpp` drive mapping: the world wrench describes the completed tick's
  interval, while the body attitude is its end state. Using that attitude leaked
  lateral steering into main-drive power. Use the owning previous attitude;
  lateral native rows now remain below1e-6 drive power without changing the
  accepted material/effect or generic renderer policy.
- `app.cpp` reset routing: the GUI button replaced the domain after camera/root
  preparation, mixing an old rendered pose with new physical state by
  0.880427279742m. SDL retry callbacks could do the same. All event/widget resets
  now request replacement at the unfrozen frame boundary. Native keyboard and
  SDL-to-ImGui button resets each advance generation once; all330 fixed-tick
  rendered poses match physical snapshots.
- `app.cpp` diagnostics: sanitizer-disabled C++ accounting printed an ambiguous
  zero. `allocation_probe=0` now distinguishes unavailable interposition from an
  observed count; validation status is explicit and HUD GPU values are pass sums.

An initial native checker expected braking to reach zero within0.5s. The existing
bounded0.35s proportional response correctly reduced3m/s to0.753778815269m/s.
The check now verifies opposing bounded force, monotonic deceleration and coasting
on release; no production physics change was made for that false premise.

Verified owning pins/snapshots/presets, copied extras/overlay submission, lifetime
of atlas/frontend before Context close, retired scene pins, finite SI/viewport/root
admission, command capacity/failure, off-focus release, fixed60Hz stepping,
pre-tick wrench interpretation and mode/reset generation semantics. Existing M1
simulation, accepted M2 pixels/settings, shader/material ABI and native resource
ownership are unchanged. Strict sanitizers cover exercised paths, not every
allocation failure, lifetime or interleaving.

Four18/18 graphs, two optional-demo-disabled graphs and four330-frame native
runs pass; eighteen actual Clang commands plus three affected implementation
reruns pass. Existing GCC AST flag limitations and historical asset-vendor
advisories/recook limitations are not waived or relabeled. No remaining
demonstrated P1/P2 was found in this playable baseline. That does not close
M3: audio/offline delivery, current Windows, warmed hardware measurements,
recording and physical owner feedback remain. Local numeric admission is not
an astronomical frame or arbitrary device-loss/global-memory guarantee.
