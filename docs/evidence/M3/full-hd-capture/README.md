# Bounded Full-HD capture and warmed flight HUD

The clean `41e71a8` MSVC return passes19/19 CTests in both Debug and Release.
Its1920×1080 replay then fails with “scene capture extent exceeds bounded encoding
storage”: the shared helper still reserves4 MiB, below an RGBA Full-HD image.
The first native regression reproduces that exact runtime rejection on Iris Xe.

Use one8 MiB capacity for admission and encoding, matching the existing Windows
artifact bound. Retain the1024-byte header margin, extent limits, finite HDR checks,
exclusive writes and error handling. Full-HD BMP output is8294538bytes;2048×1080
still rejects without creating a capture or CSV. Existing720p capture, concurrent
owner-file preservation and path-admission cases remain green. Capture allocation
and readback remain cold evidence work, outside measured frame counters.

A full validation-free replay also exposed one warm ImGui allocation. GDB traces
it to a2106-byte header index-buffer growth when direct-mode text becomes longer.
A separate public330-frame regression fails on `imgui_calls=1`. Reserve the existing
bounded overlay index capacity in that application-owned header during warmup.
The regression then passes without changing labels, appearance, ownership, counters,
warmup length or input/physics. No renderer pass/backend/resource owner is added.

Final verification: ten graphs/153 CTests,11 complete actual Clang commands,
pinned nonmutating formatting and24 opt-in physical-device cases pass, including
strict ASan/UBSan/leak paths. The final2400-frame1920×1080 Release replay writes
both artifacts, has finite/lit HDR,2400 valid GPU rows, monotonic clocks, empty
stderr and0 measured C++/ImGui allocations after the unchanged60-frame warmup.
This is correctness/allocation evidence on Iris Xe, not RTX/B1 timing or process
memory acceptance. A previous validation-enabled run has43396885 counted C++
allocations and is retained separately, not treated as clean allocation evidence.

The initial capacity expression triggered the widening-multiplication analyzer;
performing the multiplication in `size_t` fixes it with the detector still enabled.
Same-session source/security/C++ review finds no further actionable defect; this
is not independent review. The new native Windows replay/package and ClangCl run
remain required. Broad archive selection hit the safety guard; only five named,
preflighted/scanned diagnostics were read. Other returned payloads and all private
image/media originals remain unread/untouched. No returned code runs.
