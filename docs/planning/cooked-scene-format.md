# Checked scene package version 1

Implemented candidate for M2-D under proposed ADR 0008. This is a local, bounded static-scene format, not authenticated remote-asset transport. Canonical cooking uses the pinned offline native importer/cooker, the KTX command-line tool and Python's standard SHA256/CRC32 implementations. Runtime links only the checked asset loader and Zlib, not glTF, PNG, meshoptimizer, KTX, Python or a shader compiler.

All numeric fields are little-endian; floats are finite IEEE float32. C++ object representations are never serialized directly. Maximum file size is 256 MiB. The 64-byte header is:

| Offset | Field |
|---:|---|
| 0 | Eight bytes `NGASSET` followed by NUL |
| 8 | uint32 schema version, exactly 1 |
| 12 | uint32 header length, exactly 64 |
| 16 | uint64 exact file length |
| 24 | uint32 IEEE CRC32 of bytes 64 through EOF |
| 28 | uint32 section count, exactly 13 |
| 32–63 | Reserved zero bytes |

The directory starts at 64 and has thirteen 32-byte entries, ordered by kind. Each entry is `uint32 kind, stride, count, reserved_zero; uint64 offset, byte_length`. Length must equal count×stride. Payloads follow the directory at 8-byte boundaries, in kind order, with zero padding and no overlap or trailing unclaimed bytes. Unknown versions, fields, kinds and strides reject rather than guessing compatibility. CRC32 detects accidental corruption; it does not authenticate a package or provide a collision-resistant asset identity. The offline manifest supplies SHA256 source/tool/output identities separately.

| Kind | Stride | Contents |
|---:|---:|---|
| 1 | 56 | Vertex: position3, normal3, UV0 pair, UV1 pair, tangent4 floats |
| 2 | 4 | uint32 primitive-local indices, including each derived LOD |
| 3 | 80 | Primitive: first vertex/count/material/LOD count uint32; local min3/max3 floats; three first-index/count/error LOD records; reserved zero uint32 |
| 4 | 128 | Material record described below |
| 5 | 8 | Texture binding: image and sampler uint32 indices |
| 6 | 16 | Sampler: min/mag filter and wrap-S/wrap-T uint32 glTF enumerants; zero filtering means engine default |
| 7 | 32 | Image: blob-relative uint64 offset/length; uint32 width/height/mip count/format |
| 8 | 1 | Embedded KTX2 image files, each aligned to 8 bytes, without aliases or unclaimed bytes |
| 9 | 72 | Instance: primitive/source-node uint32 indices; column-major transform16 floats |
| 10 | 32 | Light: type uint32; RGB3/intensity/range/inner-angle/outer-angle floats |
| 11 | 72 | Light instance: light/source-node uint32 indices; column-major transform16 floats |
| 12 | 1 | Pooled material-name bytes |
| 13 | 1 | Bounded opaque provenance text; not interpreted as paths, code or runtime JSON |

Material offsets: base-color4 at 0; emissive3 at 16; emissive strength at 28; metallic/roughness/normal-scale/occlusion-strength/alpha-cutoff at 32–48; alpha-mode uint32 at 52 (opaque/mask/blend = 0/1/2); double-sided uint32 at 56 (0/1). Five texture/UV-set uint32 pairs start at 60, ordered base color, metallic-roughness, normal, occlusion, emissive. Name offset/length uint32 fields are at 100/104; bytes 108–127 are zero. `UINT32_MAX` denotes an absent material/texture/sampler where declared; absent texture references use UV set zero. Names are bounded to 256 bytes and reject C0/DEL controls.

Geometry bounds must contain all primitive vertices. Frames remain unit/orthogonal within the original 0.002 tolerance. Indices are unsigned triples within the primitive's vertex range. Each primitive has one to three LODs; LOD0 error is zero, lower levels never increase triangle count and carry nondecreasing conservative combined error bounds. Index payloads are stored coarsest-first for fetch locality; public LOD ordering remains finest-first. Instances require finite affine nonsingular transforms; world bounds are computed and checked. Source node IDs remain below 4,096. Point/spot intensities remain candela, directional intensity lux; zero range means unbounded. Unused non-spot cone fields are canonical zero. All declared source light definitions and active instances are retained.

Image format tags are 1 = native RGBA8 UNORM, 2 = native RGBA8 sRGB. The admitted KTX2 profile is 2D, one face, no array/depth, scalar size 1, no supercompression, and a complete mip chain. Vulkan format 37/43, descriptor transfer/layout, top-left orientation, counts, ranges, metadata lengths and nonoverlap must agree. No runtime transcoder, gamma reinterpretation, channel swizzle or external image lookup is permitted. Material color/emissive slots require sRGB; normal/metallic-roughness/occlusion require UNORM. Raw pixel bytes are owned after loading, not borrowed from a file or staging buffer.

The laptop cooker caps derived texture dimensions at 2,048 while retaining the original source unchanged. Area filtering handles odd dimensions. Color/emissive filters work in linear light; alpha-weighted color filtering is limited to mask/blend materials. Linear data is not gamma converted. Normal mips are renormalized, with neutral +Z for exact opposing cancellation. Distinct source-image roles receive separate derived images when interpretation/filtering differs. The source mesh remains LOD0; attribute-aware permissive simplification protects used UV seams and mirrored normal-map handedness, with normal weight 0.5 and relative combined error bound 0.01. Independent targets that stop at a larger count retain the previous valid LOD. Quality still requires actual rendering/visual review.

Limits: 4,194,304 vertices; 18,874,368 total LOD indices; 4,096 primitives/instances/light definitions/light instances; 256 materials/bindings/derived images; 64 samplers; 8,192-pixel image extent; aggregate decoded mip bytes bounded by the package budget; 1 MiB name pool; 64 KiB provenance. These are admission limits, not a hard process-memory/time sandbox. Loading returns an owning immutable shared pin only after complete validation. File loads reject missing/nonregular/oversized/changing reads and check cancellation between chunks; parsing checks cancellation during extraction. Source/tool paths are trusted local build inputs.

The assembler privately stages output, verifies it through the importer-free runtime executable, checks the source hash, and publishes a new immutable directory only after success. Nonempty output and symlinks reject; an empty Ninja-created output directory may be removed only at final publication. Source assets and generated packages remain ignored. The manifest explicitly preserves the lack of redistribution approval. It records the KTX tool's actual reported version string and binary hash; the current archived Conan build reports a missing-Git-version marker, which is not rewritten into an invented successful version. The pinned source recipe/archive, rather than that marker alone, identifies the selected KTX release.
