# Development inspector mockup

Open [m2-inspector.svg](m2-inspector.svg) in a browser. This is an original,
self-contained 1280×720 visual study, not a runtime UI or engine capture.

EVE Online informs the compact panel hierarchy, dark surfaces, tabular readouts
and restrained cyan/amber accents. No EVE assets, branding, fonts or screens were
copied. The ship is an original schematic, not the private supplied model.

The inspector prioritizes scene selection, material factors, render settings,
reload controls and a small diagnostic strip. A clear focus outline and readable
text contrast matter more than decorative effects. Changes queue for the next
frame rather than mutating the displayed immutable version.

All controls and telemetry are disconnected. Material values and queue occupancy
are preview state; timing and memory remain dashes. The fixed-schedule preview
includes planned bloom, not an implementation claim. This is a development
inspector, not a game HUD or evidence of M2 acceptance.

A local preview can be generated without adding a runtime dependency:

```sh
rsvg-convert --output build/m2-inspector-mockup.png docs/mockups/m2-inspector.svg
```

The native queued inspector, disabled-tooling Release graph and final visual
acceptance remain in the [M2 work](../planning/m2-completion.md).
