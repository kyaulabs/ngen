# C2-C owned creator jobs

The [offline job guide](../../creator-jobs.md) documents the separate launcher grant, one-project revision, fixed job arguments, resource limits and local artifact policy. Discovery and authoring alone do not grant execution. The build/test worker runs against a staged read-only snapshot in Bubblewrap, after the supervisor verifies the systemd scope's effective CPU, PID and memory limits. Jobs neither download dependencies nor publish publicly.

The opt-in suite used Linux x86-64 (`7.2.15-hardened1-3-hardened`), systemd 262, Bubblewrap 0.13.1, Clang 23.1.5, CMake 4.5.1, Ninja 1.13.2 and the preinstalled local Assets SDK with enkiTS/ZLIB. Python 3.14.10 ran the pinned MCP SDK 2.3.0 in stdio mode `2026-07-28`. The SDK and dependency packages were staged in a dedicated profile and resolved inside `/sdk`; no host home directory or Conan cache was visible to project code.

```sh
NGEN_JOBS_FIXTURE="$PWD/build/c2-jobs/profile" PYTHONDONTWRITEBYTECODE=1 \
  build/c2-mcp/venv/bin/python -m unittest tests.creator_mcp tests.creator_jobs tests.release_automation -v
NGEN_PLAY_SANDBOX_TEST=1 ctest --test-dir build/c1-project/fixed-release \
  -R '^creator_project$' --output-on-failure --no-tests=error
ctest --preset release --output-on-failure --no-tests=error
```

The Python invocation passed all 28 tests (five MCP discovery/authoring, four owned-job and 19 release). The full creator CLI target and all nine headless CTests also passed. The new contract initially failed because the job grant was absent. The native suite then verified real configure/build/test, embedded-marker cook, a source-only archive, and separate failures for cook, build and test. It rejected ungranted, stale, foreign and overlapping requests. A test executable could not see the launcher sentinel, open an unrelated file, write the staged source, reuse the host network namespace or create a socket. It also checked denial of io_uring and kernel keyring syscalls. Child output did not enter the MCP response. A mismatched cgroup limit prevented the worker's ready handshake.

The cancellation test saw both a test process and its child in the owned scope, requested cancellation, then checked the cgroup was empty or gone. The deadline rejected a separate long job. Neither job published anything. Source review led to a verified ready/permit handshake, a seccomp filter, private output staging and bounded process waits. Local owner I/O and kernel hangs are not covered by the execution deadline; transport frames still use the SDK's local-peer boundary. Hosted CI covers the default MCP path; the isolation results above are from the explicitly provisioned local profile, not a hosted or cross-platform claim. This is not full C2 acceptance.
