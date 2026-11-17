# Retained CIM provider diagnostics

The historical source `6a41458` return has no GPU-memory samples. Its first query
fails after 2,049 ms with `CimException`, generic text and HResult `-2146233088`.
Those fields do not establish a provider failure cause or justify changing the
2 s operation timeout. Working set and VMA cannot replace the missing samples.

`Invoke-NgenWorkload` now preserves a bounded PowerShell error identifier and
category, plus the native error name and unsigned status code for an actual
`Microsoft.Management.Infrastructure.CimException`. Non-CIM exceptions leave the
CIM fields null. The regression also checks a provider identifier longer than
512 characters is bounded. It does not serialize error data, command invocation, environment
or whole session payloads. Existing message/HResult and query timing remain.

The public observer regression uses the real child process and an actual managed
CIM exception at the mocked system-query boundary. The baseline fails because
structured fields are missing; the revised observer passes. Its managed fixture
has default native code `Ok` and status 0, but still fails GPU-memory acceptance
with unavailable status, no samples and a null budget result. A code value never
substitutes for a successful observation. Both full PowerShell orchestration
suites pass, including existing missing/zero/malformed/gapped/budget failures.

The exact focused Red/Green reports are retained privately under
`build/m3-flight/windows-cim-exact-{red,green}*`; full suites are
`windows-cim-details-bounded-green.txt` and `windows-cim-details-final-flight-green.txt`.
This is Linux PowerShell/system-boundary verification, not a native Windows CIM
query or a provider repair. No C++ source, asset, timeout, capacity, coverage rule,
GPU/working-set budget or failure gate changes. Same-session review is not
independent review. Run the [current Windows handoff](../../../planning/m3-windows-handoff.md)
and retain any provider failure and the historical ImGui finding until resolved.

The [fresh native return](current-physical/README.md) exercises these fields on
both frontends. MSVC reports `HRESULT 0x40004`, native `Failed`, status 1; this is
WMI's timeout identifier. ClangCl completes sampled coverage. The native timeout's
underlying cause is still unproven; neither code nor observation limits change
on the basis of the generic message alone.

The later [attached-process comparison](provider-comparison/README.md) records
three timeouts across both classes, followed by one successful raw reading on the
same live process. This is neither first-query-only nor limited to the formatted
class. Direct PDH comparison is now available without a rebuild; it remains a
diagnostic, not a replacement for the formal observer or its acceptance rules.
