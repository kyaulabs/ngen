# Windows renderer SDK discovery failure

The owner ran `tools/build-windows.ps1 -Renderer` at `85ec7ca5a8763ac2cba0ff7fe74400cbb34b691f` with a clean working tree. Debug dependency preparation completed, including SDL 3.4.16. CMake identified MSVC 19.44.35229.0 and the SDL targets, then failed to find `Vulkan_LIBRARY`. No NGEN renderer compilation, CTest execution, GPU startup or Release check ran.

The owner's subsequent read-only environment/file query returned:

```text
Process VULKAN_SDK=
User VULKAN_SDK=
Machine VULKAN_SDK=C:\VulkanSDK\1.4.363.1
C:\VulkanSDK\1.4.363.1\Lib\vulkan-1.lib
```

The terminal process had not inherited the configured machine SDK. CMake 4.4.4's `FindVulkan.cmake` reads the process `VULKAN_SDK` to form SDK include/library hints; it does not read the persistent Windows machine setting itself. Shader tools or validation layers being discoverable did not establish import-library discovery.

The renderer script now supplies the first nonempty process, user or machine SDK setting to its process environment and records it in `vulkan-sdk.txt`. An explicit process setting remains authoritative; missing settings are not guessed by scanning SDK versions. User/machine values are read through the environment API, never written. No SDK, registry, driver, signing or execution-policy change is made.

The PowerShell regression test first failed because fallback discovery was absent. The corrected helper passes the machine-only SDK path to a real child process. Additional cases verify user precedence, explicit process precedence, paths with spaces and all settings absent. The full existing process/profile/GPU/renderer boundary suite passes (`script-tests.txt`). These are Linux-hosted PowerShell tests, not a successful Windows renderer build. Same-session review traced the selected setting into the existing CMake invocation; The subsequent [MSVC Debug/Release run](../windows-msvc/README.md) at `c730a18` confirms SDK discovery, compilation and clean validation-enabled startup/teardown.

Original archive SHA-256: `0379b7422631d8e4cd999e4f4ad429d6f4c8477af90ead7cf5d1afc2bb113e40`. The ZIP passed a redacted Gitleaks archive scan before inspection. Selected failure/configuration/revision outputs are retained with normalized LF/trailing whitespace; `files.sha256` identifies the retained bytes. Account-bearing Conan logs and the full transcript were omitted.

The [Windows handoff](../windows-checks.md) now requests the remaining clang-cl renderer run. Frame rendering/presentation, the separate Wayland sanitizer leak and full M2 acceptance stay open.
