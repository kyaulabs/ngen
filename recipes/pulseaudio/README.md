# Pinned PulseAudio client callback repair

Linux audio uses `pulseaudio/17.0@ngen/callback-safe`, derived from Conan Center
recipe revision `c0c4e3136fe55847d070d0397582c02f`. The upstream source checksum is
`053794d6671a3e397d849e478a80b82a63cb9d8ca296bd35b73317bb5ceb87b5`.
The MIT recipe notice and upstream LGPL/component notices are preserved.
No cached vendor recipe or original source archive is edited.

A real SDL desktop open/play/close run with full Clang ASan/UBSan diagnosed
`property_free(struct property *)` being called through `void (*)(void *)` by the
property hashmap. The local patch accepts `void *` and converts inside the
function, removes the incompatible function-pointer cast and preserves both
normal destruction and direct callers. This is not a sanitizer suppression or an
upstream release claim. The same native fixture passes with the patch.

The inherited recipe builds the client only (`daemon=false`). NGEN disables
optional X11/OpenSSL/D-Bus integrations and unnecessary libsndfile programs and
external/MPEG codecs. `tools/prepare.sh` passes `-DENABLE_MPEG=OFF` on the actual
libsndfile configure command because that upstream option is declared before
`project()` loads its Conan toolchain. Otherwise ambient MPEG detection produced
a static client with undeclared unresolved codec imports. Pulse's optional ambient
systemd and asynchronous-DNS integrations are disabled through native Meson
configuration. Normal linker checks stay strict.

`profiles/linux-asan` adds the Pulse-only Meson native file that permits Clang
sanitizer DSO imports to resolve in the instrumented executable. ASan/UBSan,
fail-fast checks and leak detection remain enabled; real client tests also use
`LD_BIND_NOW=1`. Normal builds retain Meson's `--no-undefined`. No server service,
profile, volume or mixer setting is changed to obtain a passing result.

The SDL client is selected through the root dependency's declared namespaced
requirement and pinned lock override. The public SDK driver-list test rejects
direct ALSA and other undeclared backends. The hidden `[desktop]` Output test runs
real desktop playback/close without a dummy driver; it requires an existing
PulseAudio API service, including PipeWire's compatibility service.

The private Linux bundle declares the platform `libpulse.so.0` ABI rather than
shipping this SDK library or daemon. Sanitizer evidence covers the instrumented
SDK client, not every distro's client implementation. See
[desktop routing evidence](../../docs/evidence/M3/audio-desktop/README.md).
