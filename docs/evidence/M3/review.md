# M3 independent pre-integration review

A separate, fresh read-only `gpt-6.1-sol` session reviewed source `8d4d89c`
against accepted M2 integration `4b554cc`. Coverage included physics/flight/RCS,
cameras/input, GUI/renderer integration, telemetry, audio and dependency patches,
Windows PDH/build orchestration, packaging, brand generation and CMake. It found
two P2 defects, both reproduced and repaired:

- Right Alt did not own the existing free-look intent. An owned-SDL direct-mode
  drag produced no camera look; after mapping both Alt keys, releasing either
  while the other remained held reproduced the same failure. Both scancodes now
  map to free look, and key-up retains ownership when SDL's remaining modifier
  state contains Alt. No modifier cache, domain API or binding was added.
- Packaging selected the current source revision independently of the executable.
  A clean newer revision could therefore label an older binary. Native links now
  depend on a bounded source snapshot; a successful link records that snapshot
  and the executable hash. Delivery rejects missing/stale/changed source or
  artifacts, archives the captured revision and rechecks correspondence before
  publishing. Unchanged snapshots do not relink. Extracted source without Git
  metadata remains buildable but cannot claim a versioned delivery record. This
  protects against stale artifacts; it is not reproducible-build attestation.

A second fresh read-only review of the repair diff found no remaining production
defect in that scope. It found a P3 regression in two package tests: new identity
validation could fail before their intended path/rollback boundaries. The tests
now use consistent Git fixtures, require the path-specific error, and inject and
verify failure specifically at `patchelf`. Same-session review checked this final
test-only correction; the production repair did not change afterward.

Verification: 104 CTests across Clang Debug/Release/strict ASan+UBSan and GCC;
19 native UI cases (18 in the main run, then the validation-layer-dependent case
with its required layer path); four strict sanitizer-rendered UI cases; both
2,400-frame real plot-hover fixtures with zero warm C++/ImGui allocation calls;
three actual-command Clang analyses of `app.cpp`; and 12 package boundary cases.
All four native flight targets configured and linked with the identity actions.
The no-change Release build preserved its linked artifact. A validated 330-frame
SDL/audio walkthrough retains all 27 physical fields of the prior accepted UI
control trace. Formatting and diff checks pass. The initial missing-validation-layer setup failure is not counted
as a behavioral failure or sanitizer result.

Raw review reports and commands remain in ignored `build/m3-flight/` under
`m3-independent-review*`, `m3-review-followup*` and `review-*`. The independent
sessions performed source/compile checks, not native runtime or hardware tests.
This review does not establish fresh Windows execution, final laptop/RTX timing,
recording/listening/delivery acceptance or completed M3 integration. The owner has
accepted the UI starting point and deferred polish; direct-desktop Windows
verification is now available and remains the next hardware gate.
