<p align="center"><picture><source media="(prefers-color-scheme: dark)" srcset="https://raw.githubusercontent.com/kyaulabs/ngen/vX.Y.Z/brand/logos/png/ngen-wordmark-dark-640.png"><img src="https://raw.githubusercontent.com/kyaulabs/ngen/vX.Y.Z/brand/logos/png/ngen-wordmark-light-640.png" width="400" alt="NGEN"></picture></p>

<!-- Copy to docs/releases/X.Y.Z.md. Replace every X.Y.Z and tailor the highlights,
changes and validation to actual evidence. Keep the logo first; do not add an H1.
The publisher appends git-cliff's nested Changelog section. This template describes
source-only distribution, not permission to upload private scene assets. -->

## ✨ Highlights

Describe this version's purpose, important changes and compatibility/migration impact.
State whether this is the initial release or explain the version bump from actual
Conventional Commits. Do not confuse merged code, a passing build, a private preview
or a source archive with a publicly downloadable playable game.

## 🚀 Features and fixes

Explain delivered behavior and defaults, distinguish fixes from maintenance, and
identify future capabilities that are not implemented. Link additional details
below. Preserve relevant known limitations rather than converting historical
failures into passing results.

## 📦 Downloads

This release distributes **source only**. There are no public Linux/Windows runtime
bundles, packaged private ship/scene assets, recordings or desktop installers.
GitHub's automatic source links are distinct from the explicitly verified payload.

| File | Purpose |
|---|---|
| `ngen-X.Y.Z-source.tar.gz` | Versioned source, build/recipe inputs, public documentation and notices |
| `ngen-X.Y.Z-source.tar.gz.sha256` | SHA-256 checksum for the source archive |
| `ngen-X.Y.Z-SHA256SUMS` | Complete release-payload checksum inventory |

Checksums detect corruption; they are not independent publisher signatures. The
annotated source tag points to the reviewed release merge but is not independently
GPG-signed by the publication workflow. Published payloads are not overwritten on retry.

## 🛠️ Get started

Start with the CPU-only harness; it needs no GPU, window, account or private scene.
Building dependencies needs network access on the first run. Runtime offline support
does not mean a cold dependency build can download packages without a connection.

The primary source-build profile is **Linux x86-64, Clang 23.1.4, Python 3.11+,
Conan 2.34.0, CMake 4.4.5 and Ninja 1.13.2**, with a compatible C++ standard library,
Git and the native build tools required by the declared Conan recipes. Install the
required Clang toolchain using your platform's supported method; an arbitrary
system `clang` version is not the pinned primary profile. CI's Clang 18 compatibility
lane is separate evidence, not a promise that `tools/prepare.sh` accepts Clang 18.

Download the source archive and `.sha256` from this release into a new directory:

```sh
sha256sum -c ngen-X.Y.Z-source.tar.gz.sha256
tar -xzf ngen-X.Y.Z-source.tar.gz
cd ngen-X.Y.Z

python3 -m venv .venv
. .venv/bin/activate
python -m pip install 'conan==2.34.0' 'cmake==4.4.5' 'ninja==1.13.2'
clang++ --version  # Must report the required primary compiler version.

sh tools/prepare.sh release
cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure
./build/release/sim_harness
```

For a Git checkout instead, use the same setup/build commands after:

```sh
git clone --branch vX.Y.Z --depth 1 https://github.com/kyaulabs/ngen.git
cd ngen
```

To build the graphical flight application on the primary Linux setup:

```sh
sh tools/prepare.sh demo-release
cmake --preset demo-release
cmake --build --preset demo-release
ctest --preset demo-release --output-on-failure
# Supply your separately authorized compatible cooked scene; it is NOT a release download.
./build/demo-release/flight_demo /absolute/path/to/your-scene.ngas
```

Graphical execution additionally needs a supported desktop/Vulkan device and driver,
compatible Vulkan development files for the build, and the existing desktop Pulse
service for audio. Do not substitute private archived binaries or assume public
source includes the approved private flight scene. No Qt/KPipeWire recorder SDK is
required to build or run NGEN; that recorder is optional host development tooling.

Use a fresh source/build directory when upgrading, retain local assets separately,
and rebuild the selected profile. No save-game compatibility or system-wide installer
is promised. Windows compiler checks and scripted build paths are described in the
linked infrastructure guide; this source release includes no Windows binary package.

## 📚 Documentation

- [Current scope and executive brief](https://github.com/kyaulabs/ngen/blob/vX.Y.Z/docs/planning/executive-brief.md)
- [Roadmap and milestone gates](https://github.com/kyaulabs/ngen/blob/vX.Y.Z/docs/planning/milestones.md)
- [Build/review infrastructure](https://github.com/kyaulabs/ngen/blob/vX.Y.Z/docs/planning/build-review-infrastructure.md)
- [Release preparation and recovery](https://github.com/kyaulabs/ngen/blob/vX.Y.Z/docs/releasing.md)
- [Architecture decisions and their actual statuses](https://github.com/kyaulabs/ngen/blob/vX.Y.Z/docs/adr/INDEX.md)
- [Source and dependency notices](https://github.com/kyaulabs/ngen/tree/vX.Y.Z/licenses)

Additional end-user tutorials and API/reference documentation are coming later;
links will be added when those guides are published. Existing planning documents
are not evidence that future roadmap features are implemented.

## ⚠️ Compatibility and limitations

State this version's tested configurations and actual source revisions. Keep native
hardware, instrumented validation, isolated UI tests and owner acceptance distinct.
List unavailable assets/platforms and unimplemented systems explicitly. Preserve
reported failures and source/driver caveats; do not infer portable ABI, reproducible
builds, continuous memory peaks or unexercised-path sanitizer safety.

## 🔒 Security and licensing

No account, token or service is required by the offline harness. Private assets,
recordings and credentials are not release payloads. The repository's AGPL and
third-party source/notice obligations remain; supplied commercial typefaces retain
separate redistribution permissions and are not relicensed as AGPL. Review the
included licenses before redistributing a modified build or its assets.

## ✅ Validation

Replace this paragraph with performed checks and remaining limits for this version.
Report the exact reviewed/merged source, CI and public artifact verification; do not
mark a pending publication or back-merge as completed. Source-only publication does
not rerun or relabel historical Windows/hardware evidence as a new binary matrix.
