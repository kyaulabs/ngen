# Presentation first-light implementation slice

Status: Linux clear-only presentation and two verified resizes now pass in four native configurations; see [first-light evidence](../evidence/M2/presentation-first-light/README.md). Owner [Windows MSVC/clang-cl Debug/Release execution](../evidence/M2/windows-first-light/README.md) now passes the first-light/resize/minimize scenario at `b7f0334`; milestone acceptance remains pending. This slice follows [ADR 0007](../adr/0007-renderer-organization.md) and does not close M2 or introduce assets/PBR.

Build one explicit presentation profile for the verified Intel/Wayland and Windows/NVIDIA targets. Both retained device profiles advertise swapchain maintenance and present-wait support; both instance summaries advertise surface maintenance and `VK_KHR_get_surface_capabilities2`. Use the EXT maintenance names/types supported by the pinned Vulkan 1.3-era CI headers, rather than assuming recent KHR aliases exist everywhere. Query and enable every required extension/feature. The existing startup-only profile remains available and does not silently claim presentation support.

The context owns a private presentation implementation, which borrows its device/queue/surface only while that context is alive. It owns two frame contexts and at most sixteen swapchain-image records. Swapchain images themselves remain WSI-owned. Each image record owns its view, rendering-complete semaphore and present-retirement fence. Partial initialization must unwind only successfully created objects.

Acquire with a fence and retain pending acquisition state across nonblocking polls. Do not reuse an acquisition fence until its operation has completed. Wait for acquisition completion before recording/submitting access to that image. Keep command pools/buffers and submission fences per frame context; keep presentation semaphores/fences per image. Every queued presentation supplies a maintenance present fence. Submission completion is not presentation retirement.

A clear-only dynamic-rendering command establishes first light. It transitions an acquired image to color-attachment layout, clears it and transitions to presentation layout. It uses synchronization2 and a rendering-complete semaphore for presentation. No shader, source importer, VMA allocation or asset dependency is needed for this first result.

Resize/out-of-date handling stops new submissions to the old swapchain, completes or releases any pending acquisitions, and polls both submission and presentation completion before destroying old objects. Do not idle the device on ordinary frames or resize. A zero drawable or unfinished operation returns a skipped frame rather than spinning or constructing an invalid swapchain. Controlled shutdown may wait; query/allocation/device errors must remain observable and must not trigger premature destruction of in-use objects.

Verification sequence:

1. Make the real public probe's clear-frame scenario fail for missing presentation behavior.
2. Enable the explicit maintenance profile and create a minimally owned swapchain/frame set.
3. Queue clear frames under validation; verify present-fence retirement and repeated close.
4. Extend the scenario to resize/recreate during queued work and exercise partial-allocation failures through Vulkan boundary injection.
5. Run Debug/Release and instrumented Linux scenarios separately; retain diagnostics and actual frame/recreation counts, not performance claims.
6. Extend the one-command Windows helper and request actual owner execution once first light is ready. Do not mark current Windows startup evidence as frame evidence.

The existing M2 gates remain: independent acceptance, resource upload/retirement, offline shaders, cooked spacecraft, PBR/pass schedule, inspector, captures and measured performance. Native minimization semantics and surface-loss recovery require explicit evidence; a hidden-window startup is not a substitute.
