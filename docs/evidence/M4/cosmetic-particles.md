# Bounded cosmetic particles (M4-F)

[#97](https://github.com/kyaulabs/ngen/issues/97) adds a headless 60 Hz cosmetic
pool and one synthetic rendered fixture. The pool admits at most 256 particles,
with lifetimes of 1–240 ticks and per-axis speeds up to 500 m/s. Emissions carry
the current frame stamp. Invalid or excess input does not replace a live particle.
Age, position and expiry advance in stable order. A failed step or rebase leaves
the earlier frame and all particles unchanged. The frame and active particle
values belong to the returned snapshot, not to the scene renderer or Jolt.

The local tests cover expiry, motion, immutable earlier snapshots, malformed and
stale input, capacity and atomic rejection. The particle state follows ten actual
Jolt origin changes in the paired physics case, retaining its age and trajectory
relative to the unchanged hull. Release and strict ASan/UBSan/LSan each passed
all 27 demo-graph CTests on Linux x86-64/Clang 23.1.4. The lifecycle and origin
conversion tests failed before those behaviors were implemented.

Run the validation-enabled `scene_probe` with no arguments in an owned supported
Wayland session. Its public textured triangle scene renders one moving cosmetic
particle through the existing instance path. The probe compares the full
256×128 RGBA16F readback against a no-transfer run after a 256 m live particle
frame change across adjacent galactic cells. It also compares default no-particle
draws before and after the fixture. Intel Iris Xe (TGL GT2), Mesa 26.2.6-arch1.1,
isolated virtual Wayland, yielded 43 lit particle pixels, a changed image after
motion, identical fixed/translated readbacks and identical default readbacks. No
Vulkan validation error was reported. Portals lacking a graphical backend in the
isolated session logged warnings; no portal was needed by the fixture.

This is a synthetic rendered fixture, not a new effect in the accepted flight
preset, a particle-performance benchmark or a GPU simulation. Mesh RCS/exhaust,
scene counts, resources and the audio/pulse authority are unchanged. M4-G GPU
motion vectors and M4-C combined history verification remain separate.
