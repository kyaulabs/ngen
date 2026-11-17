# Preserve absent Vulkan environment settings

The `f360260` ClangCl Debug return links and passes all 19 CTests. Its validation
probe then reports a missing Khronos layer. Independent `vulkaninfo` diagnostics
also find no explicit layers and warn that `VK_INSTANCE_LAYERS` is defined with
an empty value. This is not a compiler-specific enumeration failure.

The offline bundle smoke test saves and restores four process environment values.
Passing a saved `$null` to .NET's string parameter creates an empty variable on
current PowerShell/.NET instead of restoring absence. An empty `VK_LAYER_PATH`
prevents normal registry-based explicit-layer discovery. A local native-process
reproduction confirms the unset-to-empty change; `[NullString]::Value` removes the
variable as intended.

The public flight orchestration regression now checks both presence and value
for absent, empty and nonempty layer settings on successful and failed offline
startup. It fails on the original cleanup and passes with typed-null restoration.
Existing path, SDK, location, native failure and owner-file tests remain enabled;
both PowerShell boundary suites pass. Empty values remain empty where the host
supports them, and explicit owner settings remain unchanged.

The fix changes only process-environment restoration: no SDK installation,
registry edit, validation suppression, compiler option or game/render behavior.
Already-affected sessions need a one-time removal of empty `VK_LAYER_PATH` and
`VK_INSTANCE_LAYERS` values before the next run. Nonempty settings are not removed.
A fresh native ClangCl validation/replay/package run is still required.

Same-session source/security review finds no further actionable defect. This is
not independent review or native Windows success. Returned archives are bounded,
selected diagnostics pass redacted secret scans, session payloads remain unread
and no returned code executes. The current RTX 5070 identity reports driver 621.14
and device UUID `2af4ad0b-3769-1f01-f43e-8e1fd9a3bb7e`.
