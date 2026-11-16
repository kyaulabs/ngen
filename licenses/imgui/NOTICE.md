# Dear ImGui

The optional development inspector uses Dear ImGui 1.92.9b, licensed under MIT.
The unchanged license is in `LICENSE.txt`. NGEN remains AGPL-3.0.

- Conan recipe: `imgui/1.92.9b#b0eee8a52c7f9d1cae639b440b4bb234`.
- Upstream source archive SHA-256:
  `21d8a0a565e85dce943e375db00812c2f3f0ab21f3f0f7964e364a63422d7f99`.
- The packaged, unmodified `imgui_impl_sdl3.cpp` provides platform input and
  cursors against NGEN's locked SDL. NGEN does not use ImGui's Vulkan backend.
- Technical data uses the embedded ProggyClean bitmap font: MIT license,
  copyright (c) 2004, 2005 Tristan Grimmer.
- Body/readout text uses an available installed system sans font without copying
  or redistributing that font. If unavailable, the embedded
  ProggyForever-Regular-minimal is selected: MIT license, copyright (c) 2026
  Disco Hello; copyright (c) 2019, 2023 Tristan Grimmer.

The MIT permission and disclaimer in `LICENSE.txt` also apply to these embedded
fonts with the respective copyright notices above. Font provenance is retained
in the packaged `res/src/imgui_draw.cpp` default-font sections. No EVE Online
assets, fonts, branding or screen artwork are included.
