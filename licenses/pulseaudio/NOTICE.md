# PulseAudio client SDK notice

Linux audio builds use PulseAudio 17.0's client API through the existing SDL
backend. The client-only recipe disables the daemon; NGEN does not package a
sound server or change desktop card profiles. PulseAudio's Pulse protocol also
serves PipeWire desktops through their existing compatibility service.

Upstream source: <https://www.freedesktop.org/software/pulseaudio/releases/pulseaudio-17.0.tar.xz>

SHA-256: `053794d6671a3e397d849e478a80b82a63cb9d8ca296bd35b73317bb5ceb87b5`.
`LICENSE` preserves the upstream component/licensing summary and `LGPL` its full
LGPL 2.1 text. Client options exclude D-Bus and optional server/GPL integrations.
Preserve applicable notices and source/relinking obligations when redistributing.
This notice is not a legal attestation.

`recipes/pulseaudio` derives from Conan Center revision
`c0c4e3136fe55847d070d0397582c02f`, retaining its MIT recipe notice. The local
`callback-safe` variant changes only a property destructor's callback signature
and copies the full LGPL notice. It is labelled as modified, not an upstream
release. See the recipe README for the strict native UBSan regression.

The private Linux flight bundle loads the platform `libpulse.so.0` ABI, rather
than copying the SDK client/server into the bundle. SDK sanitizer success does
not imply that the operating-system client contains the local callback repair.
The desktop service and its libraries retain their own distribution notices.
