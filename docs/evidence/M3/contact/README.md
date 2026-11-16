# Copied local contact preset

`Flight::create` copies at most127 static box proxies into the controlled hull's
existing128-body Jolt domain. Native ownership and60Hz stepping stay unchanged;
no second world or worker pool is used while playing. Reset prepares a replacement
of the same copied preset before swapping. Invalid/dynamic/oversized input fails
without publishing a Flight; partial native preparation is RAII-cleaned.

The actual Red passes through a wall to27.150021467m instead of stopping before
5.25m. The fixed controller stops against the native static proxy in two runs,
including reset after caller storage is changed. Eighteen controller cases now
include this regression and capacity/late-invalid preparation controls. Four
complete headless graphs pass8/8 in Clang debug/release/strict ASan+UBSan+leaks and
GCC. Native dependency metadata maps the changed public header to18 complete
Clang invocations, all passing with flags intact. Native GCC passes; unchanged
GCC AST limitations remain disclosed. Same-session Core/C++ review checks copied
spans, shared contact ownership, partial failure and old-preset lifetime during
replacement. No remaining demonstrated P1/P2 was found; not independent review.

These are conservative box proxies, not exact mesh collision or large-capital/
warp validation. The actual100-asteroid preset, live SDL/render integration,
traces, audio, offline package, current Windows, warmed hardware measurements,
recording and owner control-feel acceptance remain. M3 is not complete. Native
text normalizes trailing whitespace; ignored originals stay intact. No private
asset, accepted appearance or renderer ABI changes here.
