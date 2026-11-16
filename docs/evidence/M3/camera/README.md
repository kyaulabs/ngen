# Presentation camera contracts

`game/flight/camera.hpp/.cpp` is a value-based presentation rig, separate from
simulation. It composes each ship's authored local mount with interpolated ship
pose for first person; no universal offset, camera steering command or mutable
simulation reference is substituted. Translation uses double positions and
attitude uses normalized shortest-path Jolt quaternion interpolation. Only
consecutive snapshots in one entity generation interpolate; reset/gap samples
snap. Sampling is const and has no hidden time-dependent lag.

Tactical orbit follows the interpolated ship or a copied target sphere with
world-up orientation, bounded event-angle deltas and wheel zoom. Mounted free
look changes only orientation around the mount; explicit centering resets that
look. Input adapters must still implement actual focus-loss neutralization and
pointer capture/release. These numerical tests do not prove that SDL integration.

Tactical collision handling tries at most eight booms over at most128 external
sphere proxies and the controlled hull when focusing another object. It shortens
before an expanded obstacle, tries bounded alternative angles at contact, and
returns `busy` rather than an unsafe eye if none works. The selected focus sphere
is skipped as the anchor object; its radius sets minimum orbit distance. An
actual regression placed a focused camera at the controlled hull center; the
internal hull proxy now prevents it. First person is never displaced away from
its authored mount. This is bounded proxy clearance, not exact triangle or
arbitrary moving-scene visibility proof.

Thirteen public cases cover two distinct authored mounts, rotating/translated
composition,180-degree and equivalent-quaternion interpolation, tactical world-up
focus, drag/zoom limits, copied target focus, mount-only free look/centering,
obstacle shortening/contact alternatives, invalid/bounded/busy admission,
reset/gap snaps, cadence-independent sampling, admitted mount range and focused
own-hull clearance. `red/` retains ten actual runtime failures. The initial
incorrect native method spelling (`Slerp`, actually `SLERP`) is a compilation
setup failure, not a Red. A trivial optional `std::move` lint finding is fixed by
ordinary copying, not suppression.

Four complete headless graphs pass7/7 each in Clang debug/release/ASan+UBSan and
GCC. The final affected camera is rebuilt/rerun in all four after review fixes,
including strict sanitizer/leak policy. Six actual-command Clang23 analyses cover
both new TUs and camera-header consumers; all pass at the reviewed source. GCC
native checks pass, while its unchanged unsupported module flags remain outside
Clang AST claims. The camera ELF graph has no SDL/Vulkan/display dependency.
Native text has trailing-whitespace normalization; ignored originals are intact.

The preceding controller `aae5f8a` passes hosted CI4/4, retained separately from
this later camera source. This commit extends hosted Clang18/dependency checks
to the camera; exact `c3805b6` subsequently passes
[CI42946061436](https://github.com/kyaulabs/ngen/actions/runs/42946061436)4/4.
Same-session Core/
C++ review checks copies/borrows, normalized transforms, range/epoch rules,
bounded proxy iteration and errors before output. No remaining demonstrated
P1/P2 defect was found in this numerical slice; this is not independent review.

Actual input/capture/focus loss, playable presentation/contacts, force plots,
audio, offline packaging, Windows compatibility, warmed hardware benchmarks,
recording and owner control-feel feedback remain. M3 is not complete. No private
asset, pixel, shader/material ABI, render pass or resource owner changes here.
