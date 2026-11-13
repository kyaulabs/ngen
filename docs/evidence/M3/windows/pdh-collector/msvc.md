# MSVC native collector return and timing cadence

Archive `f25d56b4d76b352fd28e8f1fbcfe902a145cce4ff0ec019670ad1fda5d825e7d`
is the MSVC sibling at signed source `7afeefe`. Bounded preflight admitted
84 entries: 1,149,018 compressed bytes and 28,170,933 uncompressed bytes.
Selection retained 36 diagnostic/build/test/replay text files (3,056,989 bytes);
their redacted secret scan passed. Sessions, bootstrap/dependency payloads and
returned code were neither read nor executed. Eight capture/report hashes match
the archive bytes. Private captures remain outside Git. The only working-tree
entry is the owner's untracked `output.csv`, neither read nor removed.

MSVC 19.44.35229.0 builds and links the native PDH helper in Debug and Release.
Both configured graphs use C++20, extensions off and the dynamic CRT; each passes
22 CTests. The detector baseline's `compiler.cppstd=14` is not the configured
build standard. Two validation-enabled 330-frame walkthroughs pass physical
controls, camera/focus/reset, geometry and reported audio lifecycle checks.
Both have three F12 toggles, 260 drawn panel frames, available system CPU/RAM
and selected-device dedicated VRAM, without collector failure. Whole-device VRAM
readings are separate from the process budget below.

Independent QPC/frame correlation, row accounting, byte sums and gap recomputation
confirm 76 observations in 77 attempts over the 40.0003941 s measured window.
One startup-empty attempt is outside that window; no measured query is missing.
Maximum gap is 565.041 ms, below the unchanged 1 s limit. Maximum query time is
215 ms including initialization; measured queries take at most 2 ms. The process
GPU dedicated + shared peak is 362,328,064 bytes; separate committed peak is
362,393,600 bytes. Working-set high-water is 432,386,048 bytes. These scoped
observations meet existing budgets without relaxed coverage or fallback.

Clean and memory replays each retain 2,400 GPU timing rows and zero warm C++/ImGui
allocation calls. All 27 physical fields match each other and the historical
physical MSVC replay. Geometry verification and SDK-path-isolated app-local
startup pass. Explicit native live-panel hovered-tooltip allocation evidence,
complete delivery-manifest and real disconnected-network acceptance remain open.

The clean replay misses the unchanged 16.7 ms RTX P95 target: median 14.60655 ms,
P95 21.8387 ms, P99 22.1768 ms and maximum 22.4197 ms. The [ClangCl sibling](clangcl.md)
also misses it at 21.8084 ms. Neither run drops frames or enables F12/process-memory
sampling during clean timing. This is not a compiler-specific result or evidence
that the PDH observer slowed the clean run.

Existing CSV clocks show a useful next diagnostic, not a proven cause:

| Clean replay | Completion rate | Measured wall window |
| --- | ---: | ---: |
| Current MSVC | 60.008014 Hz | 39.9992308 s |
| Current ClangCl | 59.995653 Hz | 39.9990198 s |
| Historical ClangCl at `5ea9e3c` | 143.928031 Hz | 16.6737851 s |

The renderer requires FIFO presentation. These are **observed frame-completion
rates, not queried monitor refresh rates**. Confirm the current display refresh
and any frame cap before attributing the difference to code. Do not change
presentation mode, timing definitions, services or budgets to turn this into a
pass. No unchanged full rebuild is requested.

Both compiler returns now cover native PDH sampled memory and F12 functionality
(88 CTests combined). Final three-run/post-UI timing, the required UI refinement,
recording, independent review and owner final acceptance remain open.
