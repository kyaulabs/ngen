# Pinned offline Slang compiler

`slangc/2026.21@ngen/tools` packages the upstream Linux/Windows x86_64 binary release as a Conan **build-profile application**, not an NGEN runtime library. Conan Center had no `slang` or `shader-slang` recipe when this slice was prepared. The compiler gate is still provisional pending Windows/NVIDIA execution.

`conandata.yml` records the upstream release URLs and SHA-256 digests. Both archives were downloaded and matched those digests; the Windows archive was inspected, not executed on Linux. Linux uses the glibc-2.28 distribution, whose executable has `$ORIGIN/../lib:$ORIGIN` RUNPATH. The compilation process supplies only its package-local library path and a small environment allowlist, rather than inheriting another Slang installation's settings. Windows loads the archive's private DLLs beside `slangc.exe`. Python 3.11 or newer drives the offline tool.

The package retains upstream `LICENSE`, every `LICENSES/` file and `third-party-notices/`. Slang's top-level license is **Apache-2.0 WITH LLVM-exception**; bundled components have their own retained notices. Nothing here changes NGEN's license. SPIR-V Tools 1.4.357.0 (Apache-2.0) and its matching headers (MIT-KhronosGroup plus the retained file-specific notices) are separately pinned build dependencies. The CPU lock and all existing renderer pins are unchanged.

Enable `renderer=True,shaders=True` through the normal preparation helper. Build tools use a Release build profile independently of the engine configuration. `tools/shaderc/compile.py` hashes the executable, validator and bundled Slang libraries/modules, emits compiler reflection, validates SPIR-V for Vulkan 1.3 and admits only the finite shader set's baseline capabilities. Host system-library identity is recorded separately in Linux evidence; the manifest's `libraries` list describes bundled payloads, not every system DLL or DSO loaded by the operating system.

See [shader evidence and limitations](../../docs/evidence/M2/shader-interface/README.md). No runtime shader compiler, second production shader language, persistent environment change or system SDK installation is introduced.
