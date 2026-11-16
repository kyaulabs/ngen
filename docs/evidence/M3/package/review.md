# Packaging and handoff review

Same-session review of the packaging/CMake/Conan boundary, flight timing CSV,
Windows caller graph, process/output/environment handling and task-owned tests.
This is not independent human review or current Windows execution.

Concrete findings repaired and verified through public-boundary runtime Reds:

- Dirty source could be archived as an apparently corresponding HEAD snapshot.
  Delivery now rejects changed/untracked allowlisted source and rolls back.
- A list-valued platform escaped declared malformed-input rejection through a
  Python `TypeError`. Platform shape now rejects before output reservation.
- Prefix matching admitted SDL3.4.160 as3.4.16. Version delimiters are checked.
- A copied SDL library could be delivered without its required notice. That
  delivery now rejects and rolls back; unredistributed system wrappers need not
  invent license files.
- Flight lacked the monotonic end-of-frame clock needed by the existing scoped
  memory observer. Every measured row now publishes the same steady-clock epoch
  convention as M2, with one shared end sample for duration/absolute timestamp.
- The existing32-column ceiling rejected the valid60-column flight trace. A
  qualified native-child runtime Red proves the trigger. The64-column bound
  retains2048-byte headers,8MiB reports,3600 rows and all clock/coverage/budget
  failure checks; no measured frame or missing active query is exempted.

The new Windows path preserves existing CPU/renderer/scene/inspector selections,
adds the locked physics/audio graph and avoids duplicate M2 appearance runs.
Typed argument arrays retain asset paths with spaces. Correctness, clean timing,
sampled WDDM memory and SDK-path-free bundle startup remain distinct. Existing
output freshness is admitted before native launch. Failure paths restore Conan,
PATH/SDK/layer variables and working directory. The private bundle is outside the
returned evidence ZIP.

Passed: four21-test graphs, two PowerShell suites, eight package boundary cases,
three complete application Clang analyses, four native three-frame clock/HDR/GPU
checks, pinned nonmutating C++ formatting and Python syntax checks. Shipping and
shader-disabled graphs remain11/11 and10/10. No unresolved task-related defect was
found in this review after repairs. This is not exhaustive safety proof.

Remaining: regenerate delivery after the packaging source commit; test real
Windows compilers/DLL loading and disconnected-network startup; benchmark current
hardware; record original audio/video; obtain physical control/listening feedback.
Linux delivery targets compatible build-host platform ABIs, not arbitrary clean
distributions. Project source and locked recipes are not a vendor source mirror;
external redistribution/source/relinking rights still require their applicable
license review. No license, asset, GPU owner, rendering pass or material ABI changed.
