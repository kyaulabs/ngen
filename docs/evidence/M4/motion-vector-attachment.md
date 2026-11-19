# GPU motion-vector attachment (M4-G)

[#98](https://github.com/kyaulabs/ngen/issues/98) adds an optional RGBA16F output
to the existing Vulkan mesh pass. XY is previous minus current framebuffer
pixels, Z is reserved zero and W marks valid history. Previous clips behind the
eye or outside the clip volume are invalid. Cleared, first-frame and reset pixels
are zero. Vectors do not perform disocclusion detection or temporal reconstruction.

The opt-in pass writes vectors and color in the same depth-tested draw. Opaque
and alpha-masked geometry share the existing material coverage; blended surfaces
preserve the opaque vectors underneath. There are no separate transparent,
particle or sky vectors. The existing 64-byte material ABI is unchanged. The
attachment must be a distinct live same-context image matching the color extent,
with sampled and color attachment usage, one mip and one layer. It joins the
existing resource budget and submission retirement. At 1280×720 its logical
texel size is 7,372,800 bytes, not a VMA allocation or process-memory estimate.
The extra pipeline and frame storage are created only on the opt-in path.

`flight_demo --motion-vectors` enables the attachment, not an upscaler or another
output file. The application owns previous presented camera/ship and asteroid
transforms. Reset, mode changes, camera target cuts, drawable size changes and
interrupted presentation invalidate them. The default flight path remains single
output; RCS, exhaust, assets, audio and scene limits are unchanged.

Validation-enabled Intel Iris Xe, Mesa 26.2.6-arch1.1, isolated Wayland:

- The production `scene_probe` initially accepted a color alias before admission
  was implemented. The final probe rejected the alias and a mismatched old extent.
- Stationary geometry had valid zero motion within the gate; the known camera
  shift produced `(-7.99609375, +4)` px against `(-8, +4)`, error `0.00390625 px`.
  The first exact half-word comparison caught the rounding difference; the final
  comparison decodes the floats and applies the declared 0.25-pixel limit.
- One real 256 m Jolt rebase across adjacent galactic cells produced motion bytes
  identical to the near-origin 60 Hz control. All 43 valid pixels passed an
  independent screen-projection expectation; maximum error was `0.00557897752 px`.
- Alpha masking, missing-history invalidation and fresh resized attachments passed;
  enabling the attachment did not change the fixture's HDR color bytes. The
  separate particle fixture remained unchanged. No Vulkan validation errors.
- A six-frame validation-enabled opt-in flight replay completed using the approved
  local cooked scene. This is a startup/draw smoke, not an allocation or frame-time
  benchmark. No private input, image or recording was published.

Local Linux x86-64 / Clang 23.1.4 Release and strict ASan/UBSan/LSan demo graphs
passed 27/27 CTests each. The pinned offline shader pack, reflected layout and
Linux PowerShell Windows-script boundary passed; no native Windows GPU run.
Build the `demo-release` targets and run `scene_probe` without arguments to repeat
all GPU checks, or `shader_gpu_probe` for the [standalone stage](motion-vector-shader.md).
M4-C combined presentation/physics/prediction verification remains separate.
