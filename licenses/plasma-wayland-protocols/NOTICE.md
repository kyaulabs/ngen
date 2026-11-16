# KDE screencast protocol

`tools/record_flight/screencast.xml` derives its version-6 wire definitions from
KDE plasma-wayland-protocols, `src/protocols/zkde-screencast-unstable-v1.xml`:
https://invent.kde.org/libraries/plasma-wayland-protocols/-/raw/master/src/protocols/zkde-screencast-unstable-v1.xml

Copyright 2020–2021 Aleix Pol Gonzalez. SPDX: LGPL-2.1-or-later.
Descriptions are condensed; request/event order, argument types and versions are
preserved. The LGPL 2.1 text is included beside this notice. Generated client
sources stay in the recorder's build directory.

Upstream labels this protocol an unstable desktop implementation detail, not a
regular-client API. It is used only by the optional, host-specific development
recorder, requires the inspected version 6, and fails closed when unavailable.
It is not an NGEN renderer/runtime dependency or a portable capture guarantee.
KPipeWire, Qt and Wayland remain system SDK/library dependencies of that optional
tool; the NGEN offline executable is not relinked to them.
