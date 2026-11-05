# Windows verification handoff

Status: not run by the Linux agent. The owner will execute Windows/NVIDIA checks manually. Do not claim Windows support or close M1/M2 from Linux results alone.

Use an x64 Native Tools Command Prompt for the installed Visual Studio toolset, then start PowerShell from that prompt. Work in a clean checkout of `feat/kyau-7d227e-render-foundation`; preserve any unrelated existing work. These commands test the CPU foundation first. Do not enable the renderer until the actual MSVC/clang-cl and Vulkan SDK versions have been reported and its dependency graph is ready.

```powershell
$ErrorActionPreference = 'Stop'
git rev-parse HEAD
git status --short
cl /Bv
# cl without source can return nonzero; retain the compiler version output.
where.exe cl
where.exe clang-cl
# If available:
clang-cl --version

python -m venv "$env:TEMP\ngen-tools"
& "$env:TEMP\ngen-tools\Scripts\Activate.ps1"
python -m pip install conan==2.34.0 cmake==4.4.4 ninja==1.13.2
if ($LASTEXITCODE) { throw 'Tool installation failed' }
conan --version
cmake --version
ninja --version

# Create a dedicated profile only if it does not already exist. Do not overwrite
# a profile or substitute an assumed MSVC version. Return its contents as evidence.
conan profile detect --name ngen-windows-msvc
if ($LASTEXITCODE) { throw 'Profile detection failed (or profile already exists)' }
conan profile show -pr:h ngen-windows-msvc -pr:b ngen-windows-msvc

conan install . -pr:h ngen-windows-msvc -pr:b ngen-windows-msvc -s:h compiler.cppstd=20 -s:b compiler.cppstd=20 -s:h build_type=Debug -s:b build_type=Debug --lockfile conan.lock --build=missing -of build/windows-debug
if ($LASTEXITCODE) { throw 'Conan installation failed' }
$toolchain = (Resolve-Path build/windows-debug/conan_toolchain.cmake).Path
cmake -S . -B build/windows-debug -G Ninja "-DCMAKE_TOOLCHAIN_FILE=$toolchain" -DCMAKE_BUILD_TYPE=Debug -DNGEN_BUILD_RENDERER=OFF
if ($LASTEXITCODE) { throw 'Configure failed' }
cmake --build build/windows-debug -j 2
if ($LASTEXITCODE) { throw 'Build failed' }
ctest --test-dir build/windows-debug --show-only
ctest --test-dir build/windows-debug --output-on-failure --no-tests=error
if ($LASTEXITCODE) { throw 'Tests failed' }
& ./build/windows-debug/sim_harness.exe
if ($LASTEXITCODE) { throw 'Serial replay failed' }
& ./build/windows-debug/sim_harness.exe --workers=2
if ($LASTEXITCODE) { throw 'Worker replay failed' }
```

Return the revision, compiler/profile output and complete configure/build/test output. Four CPU-only CTest entries should execute at this stage. Also report `vulkaninfo --summary` and `vulkaninfo --json` from the Windows SDK when available; retain the generated GPU profile JSON. The eventual M2 Windows gate additionally requires SDL startup, swapchain/resize/lifetime validation, shader output and the actual rendered scene. The CPU commands above do not establish those results or clang-cl compatibility.

No Windows toolset, CRT, shader SDK or driver version has been invented. The verified toolchain/profile values will become the checked-in Windows profile after these results are available. No remote Windows host or account has been accessed.
