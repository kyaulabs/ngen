# SDL Wayland tablet proxy leak investigation

On 2026-11-05, the existing [SDL-only fixture](../renderer-startup/sdl-lifetime.cpp) still failed LeakSanitizer with **192 bytes in two allocations**, without NGEN or Vulkan. A separate debugger run tracked two server-created proxies without matching destruction: `zwp_tablet_v2` and `zwp_tablet_pad_v2`. No dependency source, driver, persistent environment or sanitizer policy was changed.

This narrows a current failure; it does not clear or replace the [earlier 480-byte/five-allocation failure](../renderer-startup/asan-gpu-leak.txt). The earlier run did not record each proxy's interface, so those five allocations are not all attributed to tablet/pad objects here. A later repair must pass full renderer startup/calibration as well as the SDL-only fixture with leak detection enabled.

The locked SDL 3.4.16 source has empty `tablet_seat_handle_tablet_added()` and `tablet_seat_handle_pad_added()` callbacks in `src/video/wayland/SDL_waylandevents.c`, at lines 3507 and 3534. They neither retain nor destroy the supplied proxies. `Wayland_SeatDestroyTablet()` cleans tools and the tablet seat, but has no owners for these tablet/pad proxies. Destroying the manager or display is not a substitute for destroying each client proxy. The observed surviving interfaces match those ignored callbacks. Changing NGEN's Vulkan or window teardown cannot recover ownership of objects SDL does not expose.

The SDL-only fixture was already built with ASan/UBSan and `-no-pie`, linked to the locked shared SDL. The repeated check was:

```sh
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  build/render-conan/sdl-lifetime
# Exit 1; sdl-only-leak.txt retains the report.
```

The independent debugger run used an unsanitized build of the same fixture to avoid running LeakSanitizer under ptrace:

```sh
clang++ docs/evidence/M2/renderer-startup/sdl-lifetime.cpp \
  -I /home/kyau/.conan2/p/b/sdl1265989badcaf/p/include \
  -L /home/kyau/.conan2/p/b/sdl1265989badcaf/p/lib \
  -Wl,-rpath,/home/kyau/.conan2/p/b/sdl1265989badcaf/p/lib \
  -lSDL3 -o build/wayland-diagnostic/sdl-lifetime
gdb -nx --batch -x docs/evidence/M2/wayland-tablet-leak/proxy-lifetimes.gdb \
  --args build/wayland-diagnostic/sdl-lifetime
```

`proxy-lifetimes.gdb` records allocations after `wl_proxy_create_for_id()` initializes the proxy, and removes entries on `proxy_destroy()`. It tracks server-created proxies only, not every Wayland object. The inferior exited normally; `proxy-lifetimes.txt` retains the unmatched interface names and count, without addresses or unrelated debugger output. The source-line breakpoint and paths identify this exact local cache; another cache requires corresponding path/line adjustments. `inputs.sha256` identifies the dependency sources, shared SDL and both fixture executables used. `files.sha256` identifies the retained evidence.

A public-source cross-check found [SDL PR #9448](https://github.com/libsdl-org/SDL/pull/9448), which proposed tablet/pad ownership changes in 2024 but was closed **without merging**. It is not an accepted fix for this pinned release and has not been applied. Its diff also contains unrelated refactoring, so blindly importing it is not the smallest repair.

Remaining decision: whether to carry a narrowly scoped, reproducibly pinned SDL source patch and recipe until an upstream release resolves the defect, or retain stock SDL and keep this acceptance gate blocked. A repair must preserve tablet/tool lifetimes, handle removal and shutdown, and account for child objects created by pad/group events. Suppressing LeakSanitizer, disabling tablet support without an explicit feature decision, mutating the global Conan cache in place, or adding compensating cleanup to NGEN is not a demonstrated ownership fix. No patch or passing post-fix result is claimed.
