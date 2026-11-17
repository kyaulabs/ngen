# M1 CPU foundation evidence

Status: implemented Linux slice; **M1 is not complete or formally accepted**. This record describes the initial `888aa10` evidence. [Independent review and corrections](review-followup.md) and [current startup/status evidence](../M2/README.md) supersede its originally outstanding review/CI entries. [Windows MSVC Debug/Release verification](windows-msvc/README.md) passed at `d49b23c`, and [clang-cl Debug/Release](windows-clang-cl/README.md) passed at `54a7a63`. Formal acceptance remains open.
Recorded 2026-11-04 against base `f6ccd18e47fa066b9e5c8623324d3141d63f0f4a`, on `feat/kyau-ed4dbe-cpu-foundation`. The initial working tree was clean. `sources.sha256` identifies the reviewed implementation/configuration bytes; the commit containing this record supplies the resulting source revision. Subsequent edits require affected checks to be rerun.

| Task | Actual status | Remaining gate |
|---|---|---|
| M1-A | Conan lock resolved; Linux and Windows MSVC/clang-cl builds/tests passed; verified profiles recorded | Independent acceptance |
| M1-B | Strong values, checked arithmetic, 16 fixed entity slots and stale-handle rejection tested | Independent acceptance |
| M1-C | enkiTS adapter, 64-job bound, serial path, dependencies, cancellation/drain/shutdown tested; TSan passed | Independent ownership/concurrency review |
| M1-D | Ordered commands, atomic fixed-tick batches and immutable snapshot copies tested | Independent acceptance; no broader determinism claim |
| M1-E | Headless replay and offline network-namespace execution passed; link graph inspected | Clean no-SDK hosted environment execution and independent acceptance |
| M1-F | Local warnings/format/tidy/tests/ASan+UBSan/TSan/coverage executed; CPU workflow configured | Hosted workflow execution, required Windows checks, independent review and PR delivery |

## Reconciliation

- The checkout contains no prior engine, build targets, tests or workflows. `fde375b` is the local initial commit; `dc52445` adds root policy/ignores; `f6ccd18` imports the tracked supporting plans. Existing `.github/` policy files are unchanged from the initial commit, not repaired speculatively. The remote public-template pin is not treated as the actual ancestor.
- `planning/NGEN-PLAN.md` is the sole master plan found. It and the extracted PDF/manifest are ignored by `/planning/`; supporting authoritative plans are tracked in `docs/planning/`, and the actual ADR index is `docs/adr/INDEX.md`. No duplicate named handoff/audit/milestone/technology documents were found. No extracted file was moved, force-added or regenerated. The only ignore addition covers native build outputs and personal CMake presets.
- Actual root `AGENTS.md`, contribution instructions, hooks, commit types, PR template and ADRs were read. No nested policy files or local ruleset JSON files exist. Root AGENTS overrides the old contribution guide's branch example. The work branch preserves the imported planning HEAD rather than dropping it to the older `develop` baseline.
- Remote rules were read only: main/develop require signatures and one PR approval; feature branches require signatures and reject non-fast-forward updates/deletion. No settings were changed. There were no stale task branches to delete. Remote ownership remains separate from local enforcement.
- `.github/CODEOWNERS` still contains `* kyau` (missing `@`); both hook files are non-executable mode 100644, no corresponding `.git/hooks/` hooks or `core.hooksPath` are installed. These pre-existing policy gaps are reported, not silently repaired or called enforced. Explicit local secret/commit checks supplement them.
- Current LICENSE is AGPL-3.0. Existing notices and license were retained. enkiTS is zlib-licensed and Catch2 is BSL-1.0; Conan retains their packaged license texts. No redistribution package was produced and no future proprietary-game licensing decision was accepted. ADR 0016 remains proposed.
- Pi 1.5.0; Prism Core and C++ modules 1.2.2. Actual `cpp-stack`, `cpp-build`, `tdd-cpp`, `tdd`, `distill`, `adr`, `cpp-review`, `code-review` and commit guidance were used. No Prism upgrade was attempted.
- CPU: Intel i7-1185G7, x86_64, 4 cores/8 logical CPUs, approximately 15 GiB RAM; Linux `7.2.10-hardened1-3-hardened`. No GPU capability or Windows machine was available to this task. M0 is not closed: independent audit acceptance and GPU/WSI captures remain open; graphics probes belong to M2.

## Build and dependency identity

Primary: Clang 23.1.4, libstdc++ from GCC 16.2.1, C++20 without extensions, CMake 4.4.5, Ninja 1.13.2, Conan 2.34.0. Compatibility: GCC 16.2.1. Conan was already installed at `/tmp/prism-conan-venv/bin/conan`; add that directory to PATH locally, or install Conan 2.34.0 in a virtual environment. This temporary tool location is not required by project files.

`profiles/linux-clang` and `profiles/linux-gcc` are explicit x86_64 host profiles. The preparation script uses the Clang build profile; the sanitizer profiles instrument dependencies as well as owned code and encode the sanitizer in Conan package IDs. `dependencies.json` retains the primary resolved refs, package IDs/revisions, settings and options. Recipe revisions are locked in `conan.lock`: enkiTS 1.12 and Catch2 3.16.0 both built from source successfully. Catch2 is test-only. The lock also records the recipes' conditional CMake 4.4.4 tool requirement; this graph used installed CMake 4.4.5 instead. The lock is not a byte-reproducible binary guarantee or a toolchain image lock.

`ngen_core` uses only the standard library. `ngen_jobs` adds enkiTS and threads; `ngen_simulation` adds no dependency; `sim_harness` links simulation. No EnTT, GLM, Jolt, renderer, database, transport or benchmark framework was added. Existing CMake/Conan facilities and fixed standard containers cover this fixture.

```sh
export PATH=/tmp/prism-conan-venv/bin:$PATH  # local installation only
sh tools/prepare.sh debug
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --show-only
ctest --preset debug

# Repeat preparation/configuration/build/test for release, gcc, asan, tsan.
# Host-specific ASan PIE/shadow collision required this local configure override:
cmake --preset asan -DCMAKE_EXE_LINKER_FLAGS=-no-pie
```

The initial ASan execution aborted before tests because this hardened host mapped PIE into ASan shadow memory. The local `-no-pie` executable-link override resolved it; instrumentation, leak checking and fail-fast UBSan stayed enabled. This is not a required distributed-binary setting. TSan initially exposed conflicting allocation-probe operator replacements at link time; allocation instrumentation is now disabled in sanitizer executables (reported as `allocation_probe=0`), leaving sanitizer interceptors intact. TSan then passed. No failing diagnostic was suppressed.

Fresh build-directory reproduction (uses the existing verified Conan cache, not proof of a cold network download):

```sh
conan install . -pr:h profiles/linux-clang -pr:b profiles/linux-clang --lockfile conan.lock --build=missing -of build/clean
cmake --preset debug -B build/clean -DCMAKE_TOOLCHAIN_FILE="$PWD/build/clean/conan_toolchain.cmake"
cmake --build build/clean -j2
ctest --test-dir build/clean --output-on-failure --no-tests=error
```

## Behavior and verification

Each configured test selection contains **3 CTest entries**, all executed, none disabled or skipped. `ngen_tests` runs **10 Catch2 cases / 281 assertions**. All five local configurations and the fresh-directory build passed. `checks.txt` records actual final outputs. Sanitizer test environments enable halt-on-error; ASan leak detection and UBSan stack traces are enabled. TSan is a separate fully instrumented build, not mixed with ASan. Passing covers exercised paths, not every interleaving.

Observed Red→Green slices included: metres conversion returning 1 instead of 1250; tick overflow returning a wrapped value rather than an error; entity allocation rejecting a valid create; job submission rejecting the first valid job; snapshot position staying zero instead of 7; wrong-tick command admission succeeding. Each failed through the public interface before its implementation was corrected. Additional boundary tests cover nonfinite input, signed overflow, generation exhaustion, stale reuse, capacity, invalid worker/dependency input, dependency-ordered updates, active cancellation, borrowed callback lifetime, and failed-batch atomicity. Tests use explicit assertions that remain active in Release.

```sh
clang-format --dry-run --Werror engine/core/*pp engine/jobs/*pp engine/simulation/*pp tests/*.cpp apps/sim_harness/*.cpp
clang-tidy -p build/debug engine/core/values.cpp engine/core/entities.cpp engine/jobs/jobs.cpp engine/simulation/simulation.cpp tests/core.cpp tests/jobs.cpp tests/simulation.cpp apps/sim_harness/main.cpp
git diff --check
```

Clang-format/tidy 23.1.4 passed on all eight owned translation units and their headers using freshly generated actual compile commands. Curated Clang/GCC warnings are errors on all owned targets. Tidy reported suppressed external-library diagnostics, not owned-code findings. A first tidy attempt used stale pre-reconfiguration metadata and failed on a missing define; only the regenerated-database rerun is accepted.

Coverage used native LLVM instrumentation, not sanitizer timing:

```sh
cmake --preset debug -B build/coverage -DCMAKE_TOOLCHAIN_FILE="$PWD/build/debug/conan_toolchain.cmake" -DCMAKE_CXX_FLAGS='-fprofile-instr-generate -fcoverage-mapping' -DCMAKE_EXE_LINKER_FLAGS=-fprofile-instr-generate
cmake --build build/coverage -j2
LLVM_PROFILE_FILE="$PWD/build/coverage/unit.profraw" build/coverage/ngen_tests
llvm-profdata merge -sparse build/coverage/unit.profraw -o build/coverage/unit.profdata
llvm-cov report build/coverage/ngen_tests -instr-profile=build/coverage/unit.profdata -ignore-filename-regex='(tests/|.conan2)'
```

`coverage.txt` is diagnostic owned-code coverage, excluding test code and third-party libraries. An earlier combined-executable report warned of a mismatched function; it was replaced by the isolated unit-executable report above. Untested branches include defensive private scheduler-failure paths and complete generation-space exhaustion of an actual slot. The generation transition itself is tested at its maximum; no claim of running 2^64 churn cycles is made.

The fixture has 8 entities, 1,000 ticks and 32,000 commands, seed `0x4e47454e`. Expected final positions are `[7303, 4415, -5166, 425, -3267, 2214, 9433, -3427]`, sum 11930. Every serial/two-worker replay checks all positions. Published snapshots own their data and expose const spans; the caller must retain the snapshot while borrowing its span. Simulation and scheduler control methods are owner-thread-only; this is not a multithreaded simulation mutation API. The scheduler executes one extraction job per fixture tick; commands remain sequential. Failed batches do not advance the clock. No wall-clock pacing affects rules.

`timing.txt` records three Release runs per worker mode. Serial P50 was about 0.5 microseconds/tick; two-worker P50 about 3–5 microseconds/tick. These include submission, scheduling, drain and publication, not isolated scheduler latency. The task is too small to benefit from workers; the serial reference is retained. Measurements are exploratory on a shared laptop with unpinned power/frequency and are **not fleet-capacity evidence**. Storage is fixed (16 entities, 64 commands, 64 task slots, at most 7 workers). Ordinary C++ allocations and enkiTS allocator callbacks both recorded zero after warmup; this is not an interception of every libc/OS allocation or aligned-new path.

```sh
build/release/sim_harness
build/release/sim_harness --workers=2
unshare -Urn -- env -u DISPLAY -u WAYLAND_DISPLAY python -c 'import socket,subprocess; s=socket.socket(); code=s.connect_ex(("192.0.2.1",9)); print("network probe errno:",code,flush=True); assert code==101; subprocess.run(["build/release/sim_harness"],check=True); subprocess.run(["build/release/sim_harness","--workers=2"],check=True)'
ldd build/release/sim_harness
```

`offline.txt` proves network-unreachable errno 101 and successful replay without display variables. `link.txt` shows only standard runtime/math libraries, no Vulkan/SDL/service libraries. Build-time dependency downloads are separate from this offline runtime proof. A fully clean no-SDK hosted image is configured but not yet executed.

## Review and delivery limits

Same-session C++ review covered all source, header consumers, actual target graphs and configurations. Fixed findings: stale analysis metadata, TSan interceptor conflict, explicit error enum representation, and preservation of original ignore-file bytes around new entries. No unresolved demonstrated source defect was found in that pass; this is not independent review.

**Independent review is outstanding.** No reviewer approval, human approval, successful hosted check or PR Test Plan is fabricated. `.github/workflows/cpu.yml` configures an Ubuntu/Clang 18 compatibility CPU matrix with locked dependencies, strict sanitizer execution, offline smoke, secret and commit checks. It has not run. It is not a claim of primary Clang 23 CI, GCC CI, pinned runner-image reproducibility, or Windows support. Local format/tidy 23 checks are not presented as hosted enforcement.

Local explicit Gitleaks history and staged-diff scans passed (redacted output). ShellCheck passed for `tools/prepare.sh`. The global commitlint installation initially lacked its conventional config; installing CLI/config 19.8.1 into `/tmp/ngen-commit-policy` allowed the actual repository config and intended commit message to pass using `NODE_PATH=/tmp/ngen-commit-policy/node_modules /tmp/ngen-commit-policy/node_modules/.bin/commitlint --edit /tmp/ngen-m1-commit.txt`. These checks supplement the currently uninstalled repository hooks. Signed commit/PR outcomes must be checked in Git and reported at handoff. No signing configuration, credentials, rulesets, runners or hosted settings were changed. PR creation/merge still requires the configured bot identity, complete Test Plan, independent review and authorized formal review. M2 is the next milestone only after M1 closes and its GPU prerequisites are met; no renderer, game, service or infrastructure work started here.
