# Windows validation-layer availability

Owner-executed follow-up using `tools/build-windows.ps1 -GpuInfo` at `dc8eb76f0b9e800098e0d1041a26fd4d0f7a9bd3`. The source revision is unchanged from the [first capture](../windows-nvidia/README.md); the tool and layer inventory changed after the owner's SDK setup.

| Item | Observed value |
|---|---|
| GPU / driver | NVIDIA GeForce RTX 5070 / NVIDIA 617.42 |
| Device API | Vulkan 1.4.351 |
| Instance API | Vulkan 1.4.363 |
| Khronos validation | `VK_LAYER_KHRONOS_validation` 1.4.363 is enumerated |
| Tool | `C:\Windows\System32\vulkaninfo.exe` |
| Tool SHA-256 | `77b0e06229b34a30e574a7cad9f1f1ef3d05da6de8cec2c7127d34f5220ebde5` |

The summary lists 13 layers. The previous registry layer-manifest warning is absent. This clears the **layer-availability prerequisite**, not Windows application validation. No cause for the earlier warning is inferred. Device features remain queried capabilities; no SDL surface, queue presentation support, frame submission or rendered image was tested.

Next: run the [Windows renderer startup check](../windows-checks.md). M2-A remains partial and M2-B through M2-F remain unimplemented.

Original archive SHA-256: `48507643534c0c018226b3e9eed60f61d830740a91f9a59077e2d1461f8f03ad`. The ZIP passed a redacted Gitleaks archive scan before inspection. Selected outputs retain normalized LF/trailing whitespace; `files.sha256` identifies their bytes. The account-bearing transcript was omitted.
