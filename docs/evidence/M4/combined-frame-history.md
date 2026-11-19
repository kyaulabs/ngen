# Combined frame history (M4-C)

[#13](https://github.com/kyaulabs/ngen/issues/13) checks that a stationary
physics-origin transfer does not show up as motion in the flight view. The
flight snapshot owns both the Jolt body state and the matching frame. The camera
checks those stamps and converts each consecutive pose to the authored scene
basis at its own tick before interpolation. A contradictory or unresolved epoch
cannot supply history. Navigation targets and scene obstacles stay in the authored
basis; the controller converts positional targets to the current Jolt basis.

`flight_demo` uses that basis for the camera, ship-relative draw inputs, and
point selection and approach. Its previous presentation remains available across
compatible origin transfers. A reset replaces the authored basis and discards
that presentation. The bounded demo still does not schedule origin transfers on
its own. This slice does not add a network predictor, warp or streaming.

The native flight scene test applies the same destination command in a near
fixed-origin world and a far world, with 120 live transfers alternating across
adjacent galactic cells. At 1920×1080, the projected marker and its motion each
had 0 px disagreement against the 0.25 px limit; the control reached
0.148483768 px/tick. Shadow and asteroid draw inputs matched. The camera test
rejected two incompatible origins labeled with the same epoch, both with and
without an explicit authored basis. Before the fix the implicit and explicit
cases each returned a view instead of rejecting the relevant conflict.

With the demo graph enabled, the public no-argument `scene_probe` runs a separate
combined GPU check after 12 fixed ticks of real navigation. It presents two flight
snapshots around a 256 m live Jolt rebase, advances an owning particle pool with
the same frame stamp, and changes the camera orbit. The equivalent near control
uses no transfer. Both scenes use the production flight view, mesh/shadow pass,
and GPU motion attachment. On Intel Iris Xe / Mesa 26.2.6-arch1.1, validation
enabled on an isolated Wayland session:

- 256×128 RGBA16F HDR and motion readbacks and 1024×1024 D32 shadow readbacks
  matched byte for byte. The shadow depth contained covered texels.
- The motion attachment had 387 valid pixels with up to 1.65953684 px of
  control motion, so the zero difference did not come from two cleared images.
- Three visible emissive particle pixels had invalid motion, as intended;
  the fixture does not claim separate particle vectors or disocclusion tests.
- The separate M4-F and M4-G GPU controls still passed without Vulkan errors.

Clang 23.1.4 Release and strict ASan/UBSan/LSan demo graphs passed 27/27 CTests
each. The initial hosted Debug and sanitizer checks flagged optional pixel
accesses after Catch assertions under Clang 18. The fixture now copies both
coordinates immediately after its presence check, with no analyzer suppression
or change to the pixel gate. Only the GPU fixture above was run under Vulkan
validation; the sanitizer CTest result is a different check. This is local prediction/interpolation and
translation-only presentation evidence, not cross-platform performance,
rotating frames, arbitrary contact worlds, or integrated M4 acceptance.
