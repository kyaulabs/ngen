# Windows active developer-shell reuse

The first current MSVC archive declares clean `ef85f4c`, MSVC194/x64/C++20 and
correct Debug/Release CRT contexts. Dependency graph resolution succeeds. Jolt
then fails before CMake/compiler execution: Conan re-enters `conanvcvars.bat`
inside the script's already-active developer shell and cmd.exe reports “The input
line is too long” / “The syntax of the command is incorrect.” This is a native
setup failure, not a physical/gameplay behavioral Red or a compiler defect.

The trusted script already initializes/verifies the x64 Visual Studio shell and
selects the actual compiler. Its scoped Conan host/build invocation now sets
`tools.microsoft.msbuild:installation_path=`. Conan2.34's installed `VCVars`
generator explicitly treats that empty value as reuse/no additional activation.
It does not change system/user profiles, shorten the owner's PATH, remove compiler
flags, weaken warnings/checks or alter Jolt/upstream source and recipe locks.
Manual Conan use outside this active-shell workflow keeps normal activation.

Both PowerShell boundary suites pass. A real Conan CMakeToolchain generation
fixture shows redundant activation generated without reuse, then no activation
script with the scoped setting; C++20 and Debug dynamic CRT remain in the toolchain.
The placeholder SDK batch is never executed. This checks generator behavior on
Linux, not actual Windows compilation. A current owner rerun is still required.

The16-entry return is bounded/preflighted for names, types, sizes, compression,
encryption and duplicates. Selected diagnostics pass a redacted secret scan.
The specific dependency-build failure log is inspected separately; full session
and bootstrap payloads remain unread/unextracted. No returned script/binary runs.
The initial broad preflight hit a safety guard; only transparent metadata and
explicit diagnostic selections were used afterward, without changing the guard.
