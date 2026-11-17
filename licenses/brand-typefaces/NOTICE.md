# Supplied NGEN typefaces

The owner supplied the Visual Identity 1.4 font files under `brand/web/fonts/`
and explicitly confirmed redistribution rights for including them in this
repository and offline delivery. They were copied byte-for-byte from
`kyaulabs/ngen-web/brand/`; that pack identifies the user's Prism fonts as its
source. All embedded notices and the supplied fonts README remain intact.

These commercial font programs are **not relicensed by NGEN's AGPL license**.
Applicable embedding, redistribution and modification permissions remain
separate. The owner's confirmation is not independent legal approval or a grant
of rights beyond the applicable font licenses.

Embedded notices identify:

- Neo Sans Pro: "Digitised data (c) 2004 Monotype Imaging. All Rights Reserved.
  Neo Sans is designed by Sebastian Lester."
  License reference: http://www.agfamonotype.com/html/type/license.html.
- Operator Mono Lig / SSm Lig Nerd Font variants: "Copyright (C) 2016 Hoefler & Co.
  http://www.typography.com". Embedded name records state that the end-user license
  agreement is available at http://www.typography.com/support/eula.html.

The native GUI embeds only `NeoSansPro-Regular.ttf` and
`OperatorMonoLigNerdFont-Regular.otf` through its existing ImGui rasterizer and
atlas. The provided pack retains the other supplied weights for identity/source
correspondence. No fonts are installed globally, no host font files are copied,
and no new font or renderer dependency is introduced. Typeface geometry, byte
contents and notices are unmodified; OpenType ligature/shaping support is not
newly implemented.
