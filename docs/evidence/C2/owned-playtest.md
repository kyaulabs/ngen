# C2-D owned board playtest

The owner authorized MCP control of the existing sandboxed headless board only. The [separate launcher grant](../../creator-mcp.md#opt-in-board-playtest) starts a fixed helper using the opened project directory, not a request-selected path or PID. The helper reuses the C1-D play protocol, cooker and native player. The project remains data; no CMake or project executable runs.

A real Python MCP SDK 2.3.0 client exercised stdio revision `2026-07-28` on Linux x86-64 with Python 3.14.10 and Bubblewrap 0.13.1. The native asset build used Clang 23.1.5, CMake 4.5.1 and the locked Conan inputs. All 10 MCP and four provisioned job tests passed, as did all 15 Release asset/SDK CTests with the native owned board cases enabled.

- Start loaded revision 1 and reported `marker_mm=1042`. A source file containing `#error` did not affect play. An invalid initial revision did not retain a session; a second active start was denied.
- Saving a new scene did not change the active snapshot. An invalid reload left it unchanged; reloading revision 2 advanced generation to 2, reported `marker_mm=3042`, and reused the one cook. Old generations were rejected.
- Renaming and replacing the original workspace path did not change the granted directory. A request could not choose another root, PID, production target or binary.
- Two separate MCP clients could only inspect and stop their respective sessions. Stop retired one staging directory without disrupting the other client; stdio close removed the remaining owned staging directories. An old handle did not select a restarted playtest.

Each new public operation was first exercised against the absent grant/tool and failed as expected before implementation. The existing CLI and separate edit/job controls stayed available. Local testing establishes the trusted board profile, not general project execution, resource quotas for arbitrary executables, or hosted/multi-user isolation. C2-E integrated acceptance and MCP host usability remain separate.
