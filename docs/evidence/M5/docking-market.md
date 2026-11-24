# M5-E docking-gated market

A serialized game owner holds one `docking::Berth`, one assigned vessel and the existing M5-A local market. The same scripted local-frame approach is exercised at a station and at a planetary orbital hub. The [M3 flight baseline](../M3/mode-handoff/README.md) remains unchanged. This fixture supplies trusted relative samples; it does not drive the controller, resolve native contacts, move cargo between world instances, or model planetary surfaces.

An available berth admits one approach and issues a ticket bound to the vessel, facility kind and attempt. Capture requires each signed axis offset within 500 mm and relative speed at most 200 mm/s. Unavailability blocks new approaches and capture but does not revoke a completed docking. The owner can abort an approach even if the berth becomes unavailable. Departure closes the market immediately; the reservation remains until at least one axis reaches 2,000 mm from the berth. Abort and clearance retire the ticket. Tickets are local fixture identifiers, not cross-session tokens or credentials.

Only the current docked ticket reaches the market, even for replay. A blocked trade does not use a market journal slot. The scripted buy adds three units to the vessel; after departure and re-docking, the same vessel still owns the purchased cargo and the old trade ID returns its original receipt without settling again. A subsequent sale leaves the two accounts with 13 units and 120 credit subunits in total. No database, wallet, live docking or multiplayer authority is involved.

```sh
cmake --build --preset release && ctest --preset release --output-on-failure --no-tests=error
cmake --build --preset asan && ctest --preset asan --output-on-failure --no-tests=error
cmake --build --preset tsan && ctest --preset tsan --output-on-failure --no-tests=error
```

Linux x86-64, Clang 23.1.5/libstdc++, CMake 4.5.1, Ninja 1.13.2 and the existing Conan graph: all nine headless CTests passed in Release, strict ASan/UBSan/LSan and TSan. The final docking target passed 83 assertions across four cases in Release and was rerun under both sanitizer presets. Admission, abort, departure and foreign-ticket assertions failed before their implementations. Scoped clang-format, clang-tidy and workflow lint passed. An independent source review found no blocker for the serialized fixture. This is not integrated M5 acceptance or a performance measurement.
