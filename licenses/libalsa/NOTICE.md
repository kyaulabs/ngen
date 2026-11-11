# ALSA client library

M3's Linux-only SDL audio graph adds the shared ALSA client1.2.16.1, not a sound
server or another audio framework. Ordinary renderer and headless graphs keep
SDL audio disabled. Windows uses SDL's native platform backend without ALSA.

Upstream: https://www.alsa-project.org/files/pub/lib/alsa-lib-1.2.16.1.tar.bz2.
Archive SHA256: `f740db7f488255944ffd4428416ee3390a96742856916433df468c281436480e`.
Conan recipe revision: `f1f84174d706f61048a3e55eb5c2daac`.

`COPYING` preserves the upstream LGPL-2.1-or-later wording (trailing whitespace
normalized) and must accompany redistribution, with the required source/build
and relinking provisions.
Upstream source is unchanged. The Clang sanitizer profile permits its shared
instrumentation imports to resolve in the final instrumented executable, as
with SDL; ordinary links remain strict and eager-binding consumer tests verify
the imported runtime. No sanitizer or fail-fast detector is disabled.

NGEN remains AGPL-3.0. These notices do not authorize redistribution of the
owner's private ship assets. The procedural mixer/cues are original NGEN code.
