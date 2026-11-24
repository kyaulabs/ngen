# Offline creator jobs

The optional MCP companion offers fixed `cook`, `build`, `test`, and `package` jobs for one launcher-granted starter. Launching discovery or granting `--allow-edits` does not grant jobs. Only choose `--allow-jobs` when you intend to run the selected project's code. No command lets a client choose executable paths, CMake flags, output paths, shell text, mounts or a second project.

## Prepare a local profile

The tested profile is non-root x86-64 Linux with a running systemd user manager, cgroup v2 PID/memory/CPU controllers, Bubblewrap with non-root user namespaces, and `libseccomp.so.2`. The system `/usr` must supply Clang, CMake, Ninja, CTest, Python, and `prlimit`. The host crash policy must disable piped crash processing (a non-pipe `core_pattern`, or exactly `|/bin/false`). Jobs refuse to proceed when the worker cannot verify its limits or start the sandbox. There is no unsandboxed fallback.

Prepare a dedicated SDK directory containing the installed headless NGEN SDK with Assets, plus trusted enkiTS 1.12 and ZLIB 1.3.2 CMake packages and libraries. See the [SDK build guide](creator-sdk.md). Install NGEN with `cmake --install <prepared-build> --prefix <profile>/sdk`; stage the two dependency packages below that prefix with CMake configs that resolve there in the sandbox (`/sdk`). Do not mount a package manager cache, a home directory, or a general workspace as the prefix. Put native `assetc`, `ktx`, and `asset_package_probe` (named `probe`) in a separate audited tools directory. Require no dependency downloads from a job. Make the launcher output directory owner-only (`0700`).

```sh
.venv-mcp/bin/python tools/creator_mcp.py --project /absolute/board --allow-jobs \
  --job-sdk /absolute/profile/sdk --job-tools /absolute/profile/tools \
  --job-output /absolute/local-archives
```

`--job-seconds` sets the execution deadline from 1 to 90 seconds (default 90). Cancellation, stopping the scope, staging files and local disk I/O are separate from that execution limit. Use the tested local profile; this is not isolation against kernel bugs, a hostile same-user launcher, or an adversarial OS/SDK. Audit `/usr` and the dedicated SDK/tools as trusted base inputs; do not put secrets or device files in those mounts. Unexpected nested mounts are rejected. The source snapshot, `/usr`, SDK and tools are read-only. The sandbox has private PID, mount, IPC and network namespaces, no inherited credentials, no host output mount, and a filter denying socket creation, io_uring setup, and kernel keyring access. Job memory is capped at 768 MiB with no swap, with 32 tasks, one CPU, 256 MiB temporary work space and 64 MiB `/tmp`. Files are capped at 32 MiB. The launcher controls cross-session disk quotas and must keep the local output private.

## Call the tools

1. Use `list_projects`, then `job_revision` with the returned handle. The revision hashes the exact bounded source and asset files captured under the creator read lock. Other creator commands coordinate through that advisory lock; direct filesystem editors must finish before starting a job if they need a coherent multi-file save.
2. Call `start_job` with `handle`, `kind`, and `expected` set to that revision. One job is active and at most 32 are admitted per launcher session. A later admission retires the previous status. A stale request runs nothing.
3. Poll `job_status` with `handle` and `job`. Call `cancel_job` with the same arguments to request a stop. The result stays running through cleanup, then becomes `succeeded`, `failed`, or `cancelled`. Cleanup checks the owned cgroup is empty or gone. If cleanup cannot be confirmed, the session refuses further jobs.

`cook` verifies the starter marker cook. `build` cooks the marker, configures and builds the starter; `test` repeats that in a fresh job and runs the starter's CTests. These jobs discard child console output and do not export binaries or cooked scenes. Use the native CLI in an owned workspace for detailed diagnostics. `package` uses the existing source-only archive writer on the frozen input, never runs project code, and only publishes a complete `<job>-source.tar.gz` in the granted local directory. Status returns its basename, not a host path or contents. A failed or cancelled job publishes no archive. Local publication is atomic and non-overwriting, not a crash-durability or public release grant.

Run the real stdio suite with a prepared profile (unsupported hosts do not run these isolated tests):

```sh
NGEN_JOBS_FIXTURE=/absolute/profile PYTHONDONTWRITEBYTECODE=1 \
  .venv-mcp/bin/python -m unittest tests.creator_mcp tests.creator_jobs -v
```
