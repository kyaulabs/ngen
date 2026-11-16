NGEN VISUAL IDENTITY 1.4
KYAU Labs

NGEN is a C++20 game engine for space simulators, with a focus on space MMOs.
Balanced is the approved mark: a stepped N with paired violet wakes.
Revision 1.4 adopts Prism's dark and light reference palettes and typography.

START HERE

  guide/ngen-brand-guide.pdf
      Eleven-page identity, clear-space, color, type and implementation guide.
      Editable HTML is alongside the PDF; page PNG/SVG exports are in pages/.

  identity/
      Combined identity board and dark/light variants, in SVG and PNG.

  logos/svg/ and logos/png/
      Outlined wordmark and descriptor signature. Use dark files on dark
      surfaces and light files on light surfaces. Monochrome white/ink assets
      are unchanged. Transparent PNGs are 640 and/or 1280 px wide.

  icons/
      Transparent marks, opaque app tiles and favicons. App sizes: 32, 64,
      128, 256, 512 and 1024 px. Favicons include fixed-theme SVG/PNG,
      a theme-adaptive SVG, and 16/32/48 px ICO frames for each mode.

  social/
      Open Graph 1200 x 630, banners 1500 x 500 and avatars 1024 x 1024.
      Use PNG for publishing; SVG for editing. Select a matching theme.

  graphics/
      Transparent orbital schematics, in dark/light SVG and PNG.

  web/
      Canonical ngen-tokens.json, reference ngen-theme.css, palette preview,
      PNG/SVG swatches, sharing-metadata example and self-hosted fonts.
      CSS follows system preferences unless html data-theme is dark/light.
      Base colors use --ngen-*; material aliases are --bg-0, --panel,
      --panel-2, --line and --line-strong. Fonts are Neo Sans Pro and Operator
      Mono supplied from the user's Prism checkout; embedded notices and
      applicable font permissions remain separate from the website license.

ASSET USE

  Keep the custom N inside the wordmark and preserve one upright of clear
  space, measured from visible artwork. Minimums: wordmark 144 px,
  signature 220 px, standalone N 24 px canvas, favicon 16 px tile.
  Keep logos proportional, horizontal and unfiltered on quiet fields.
  Use monochrome for single-color applications. SVG labels require the
  authoring fonts; PNG/PDF exports remain usable without installed fonts.

  Dark materials: #04060C page, 66% #0A101D panel, 55% #10182A header,
  14% / 30% #7D9BD2 borders, #22D3EE / #A78BFA / #F472B6 accents.
  Light materials: #EDF0F8 page, 78% white panel, 85% #F4F7FD header,
  12% / 26% #102A54 borders, #0891B2 / #7C3AED / #DB2777 accents.
  Alpha affects surfaces, not text. Borders and subtle pigments are
  decorative; retain readable labels, selected-state cues and bright focus.
  Use dark ink on light cyan actions and higher-contrast light status text.
  Guide ratios composite over reference fields, not arbitrary imagery,
  and are not a full accessibility-conformance claim.

  The adaptive favicon follows system preferences. Select a fixed-theme
  favicon when needed. Publish Open Graph PNGs at absolute URLs.

REVISION AND EXPORTS

  Both palettes, colored masters, typography references, swatches, previews,
  guide and matching exports are refreshed. All original paths, logo
  geometry and monochrome artwork are preserved. The original supplied
  Graphite 1.2 archive is untouched. Legacy asset filenames are retained.
  Website, engine and brand versions are independent; NGEN's active website
  remains dark-only. Its approved nebula and natural star field are separate
  from this brand recoloring. UI icons use attributed Font Awesome Free.

  In the website checkout, edit JSON, affected SVG masters and guide HTML;
  run npm run build:brand. It rebuilds reference CSS, both swatch SVGs,
  colored PNG/ICO files at existing dimensions, PDF and guide page exports.
  Monochrome files are not regenerated. Required tools: Playwright/Chromium,
  rsvg-convert, pdftocairo and ImageMagick (magick). Provided fonts are used
  through isolated Fontconfig/cache files; no global font installation is
  performed. Port 4173 must be free. Ordinary deployments never run this
  optional command and require no production Node.js server.
