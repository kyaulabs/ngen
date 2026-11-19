# Motion-vector shader fixture (M4-G)

This is partial evidence for [#98](https://github.com/kyaulabs/ngen/issues/98).
The offline Slang stages take two clip-from-local matrices, a pixel viewport
and an explicit history-valid flag. The RGBA result holds current-to-previous
pixel displacement in XY, reserved zero in Z, and validity in W. Cleared and
invalid pixels are zero. Prior clips behind the eye or outside Vulkan's clip
volume are rejected; this is not a disocclusion detector.

The reflected 144-byte history uniform matches the C++ offsets and alignment;
`MaterialConstants` remains 64 bytes. The native offline suite passed all 15
cases with pinned Slang and `spirv-val`. Before implementation, the public
package regression reported both motion stages missing. The Linux PowerShell
Windows-script boundary fixture passed with Conan on PATH; no native Windows
build was run.

The existing `shader_gpu_probe` draws the stages into its 64×64 float target.
On Intel Iris Xe / Mesa 26.2.6-arch1.1 under isolated Wayland with Vulkan
validation, actual readback matched stationary `(0,0,0,1)`, a known XY transfer
`(-8,+4,0,1)`, and zero for cleared, reset and behind-eye samples within
0.001 pixel. The existing PBR fixtures still passed. The Release and strict
ASan/UBSan/LSan demo graphs each passed all 27 CTests.

This probe does not own a shipping attachment, test depth/alpha-mask coverage,
or establish frame-history continuity in the live flight renderer. The next
slice must admit, own and retire a bounded attachment on the production path,
feed it aligned histories and verify real live rebase, resize and reset
readbacks. No GPU motion-vector coverage is claimed for transparent effects,
particles or sky.
