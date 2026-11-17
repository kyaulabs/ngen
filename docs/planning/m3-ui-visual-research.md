# M3 HUD visual reference study

The first layout proposal followed the three article texts and the NGEN guide,
but had not yet inspected the EVE images. After the owner identified that gap,
implementation paused and all 27 relevant UI images linked by those articles
were opened and visually reviewed. Workshop photographs were excluded. Source
images are cached only under ignored `build/m3-flight/eve-ui-reference-images/`;
none are product assets or committed artwork.

Sources:

1. [Photon UI – Iterating and Improving](https://www.eveonline.com/news/view/photon-ui-iterating-and-improving):
   the Normal / Compact Mode comparison.
2. [EVE Evolved: A New Horizon for the HUD](https://www.eveonline.com/news/view/eve-evolved-a-new-horizon-for-the-hud):
   three historical in-game layouts, the isolated command board and four-panel
   earlier HUD-concept montage (five images).
3. [Evolution, not revolution](https://www.eveonline.com/news/view/evolution-not-revolution):
   all 21 UI figures, including current/new composites, medallion, utilities,
   speed, health, capacitor, energy change, rack heat, security, module states,
   overload controls/state/racks, three map views, consumables, booster bay and
   component customization.

The historical/current screenshots, exploratory montage and explicitly
work-in-progress proposals are different evidence. The article's cyan/magenta
figure ribbons, diagonal presentation background and WORK IN PROGRESS labels
are not game HUD styling. The images do not prove that proposed interactions
have shipped or are usable in NGEN.

Visual observations and their implementation consequences:

- **Photon's compact comparison:** compact windows preserve recognizable tabs,
  row alignment and selection while removing redundant headings/navigation
  space. Apply this to target actions and optional flight data, not by shrinking
  every font or hiding state behind unexplained symbols.
- **Historical in-game layouts:** the ship and surrounding space stay central;
  overview/selection information sits at an edge and repeated piloting actions
  stay low. After reviewing the first mockup the owner requested fewer panels:
  keep only a minimal bottom readout and world-anchored target information by
  default. Remove the tall telemetry slab, target card and boxed binding banner.
- **Current medallion versus separate speed figure:** the current circular
  instrument mixes unrelated quantities; the proposal gives speed its own
  numbered readout, scale and marker. NGEN gets a dedicated, labelled actual-speed
  instrument, visually separated from actuation and navigation. It is read-only:
  do not imply a clickable velocity setter or add EVE's max-speed controls.
- **Health/capacitor/heat figures:** different geometry and placement distinguish
  related systems without making everything a window. Use that grouping
  discipline, not fake shields, hull HP, capacitor, heat or a copied medallion.
- **Module-state strip:** idle, hover, reload, cycling and overload differ through
  outlines, sweep/position, numeric state and localized colour, not only a light.
  NGEN must distinguish active navigation/input from actual main-drive and RCS
  firing. Valve-off intervals must remain visibly off even if guidance persists.
  Any actuation indication comes from the published physics snapshot, never a
  separate decorative pulse clock.
- **Overload/rack figures:** group-level interaction highlights the related
  controls instead of presenting a tray of unrelated utilities. Keep target
  identity/range and approach/orbit/focus together; separate view/reset/help/audio
  utilities from flight instruments. Existing actions retain visible shortcuts.
- **New composite and component customization:** the command board is a set of
  compact instrument groups, not a large opaque rectangular dialog. Use sparse
  frame segments, restrained surfaces and legible value/unit hierarchy. NGEN's
  stepped/chamfered language, cyan focus/actions and violet secondary accents
  supply its identity; do not simply recolour stock ImGui windows.
- **Map/booster figures:** these communicate substantial gameplay that NGEN does
  not have. Do not add a fake minimap, modules or inventory slots to resemble the
  reference. Optional plots and F12 hardware data stay clearly separate.

The owner's latest immersive/no-persistent-panels direction supersedes the
initial boxed deck/right-card proposal; shared-brand scope stays approved. Use the supplied proportional NGEN wordmark with its clear
space, a quiet field and the guide's minimum sizes. Preserve the accepted ship,
physical handling and audio. No EVE image, icon, font or sound enters the runtime.

The original [immersive NGEN mockup](../mockups/m3-flight-immersive.svg) captures
that revision; the earlier [panel-heavy mockup](../mockups/m3-flight-deck.svg) is
retained as rejected design history, not an alternate runtime mode. Then
verify the real rendered UI, responsive sizing, operational-state truthfulness,
keyboard/pointer behaviour and warm allocations. A mockup and this visual study
are not implemented UI, usability proof or final acceptance.
