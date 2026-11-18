<p align="center"><picture><source media="(prefers-color-scheme: dark)" srcset="brand/logos/png/ngen-wordmark-dark-640.png"><img src="brand/logos/png/ngen-wordmark-light-640.png" width="400" alt="NGEN"></picture></p>

<p align="center">
<a href="https://github.com/kyaulabs/ngen/releases"><img alt="Latest release" src="https://img.shields.io/github/v/release/kyaulabs/ngen?logo=semver&sort=semver"></a>
<a href="LICENSE"><img alt="License" src="https://img.shields.io/github/license/kyaulabs/ngen?logo=creativecommons"></a>
<a href="https://github.com/kyaulabs/ngen/actions/workflows/cpu.yml"><img alt="CPU foundation" src="https://github.com/kyaulabs/ngen/actions/workflows/cpu.yml/badge.svg?branch=develop"></a>
</p>

NGEN is a **C++20 3D game engine under development**, with Linux as its primary
platform and Windows compiler/runtime evidence at recorded revisions. Its first
showcase is an offline spacecraft flight demonstration; the longer-term goal is
a reusable engine and creator toolchain, alongside an authoritative PvPvE universe.

**[0.3.0 is available as source only.](https://github.com/kyaulabs/ngen/releases/tag/v0.3.0)**
It is not a downloadable playable game: the accepted private ship/scene assets,
runtime bundles and recordings are not public release payloads.

## What works today

- CPU-only simulation harness with bounded jobs/commands, strong IDs, explicit
  ownership and immutable snapshots.
- SDL/Vulkan rendering with VMA resource ownership, offline Slang shaders,
  cooked assets, PBR/IBL/shadows, post-processing and a development inspector.
- Double-position Jolt physics, shared tactical/direct flight control, authored
  cameras, physical RCS actuation and snapshot-driven presentation/audio.
- An opt-in, [source-built CMake SDK](docs/creator-sdk.md) and a Linux
  [standalone starter](docs/creator-project.md) that cooks and loads its own asset.
- Linux CLI [scene authoring](docs/creator-scene.md) with versioned saves,
  undo/redo and source packaging. The authored scene is not yet rendered.
- Checked astronomical coordinates and live origin transfers in one bounded
  physics domain. Presentation continuity, warp and streaming are still planned.
- Pinned Conan/CMake/Ninja build graphs, tests/sanitizers and source-release
  verification with reviewed back-merges.

The SDK and starter are not a general-purpose scene editor or a stable binary
ABI. Gameplay economy, multiplayer, sovereignty and the MCP server are planned.
Historical hardware and performance results retain their original source and
configuration limits; see the [executive brief](docs/planning/executive-brief.md).

## Get started

Start with the CPU-only harness. It needs no GPU, display, account, service or
private scene. The first dependency build needs network access.

The primary profile requires **Linux x86-64, Clang 23.1.4, Python 3.11+,
Conan 2.34.0, CMake 4.4.5 and Ninja 1.13.2**, plus Git, a compatible C++ standard
library and native build tools. Install the required Clang toolchain through
your platform; an arbitrary system compiler is not the pinned primary profile.
CI's Clang 18 compatibility lane is separate.

```sh
git clone --branch v0.3.0 --depth 1 https://github.com/kyaulabs/ngen.git
cd ngen

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

The release also provides a checked source tarball and SHA-256 inventory. See the
[0.3.0 instructions](docs/releases/0.3.0.md) for download verification, graphical
build commands, compatible separately authorized scene requirements and platform
limits. Graphical execution additionally needs supported Vulkan hardware/drivers
and the desktop audio path; the private flight scene is not included.

`main` contains reviewed public delivery; `develop` integrates future work.
Use the tagged checkout above for the published baseline, rather than assuming
the development branch has the same behavior or requirements.

## Direction

The reusable engine and its flagship game have different responsibilities:

- **Engine and creators:** a consumable SDK, project/build/asset workflows,
  practical scene tools, tutorials and both space and non-space examples.
  A local MCP server will expose validated authoring/build/playtest operations
  to compatible AI clients—not unrestricted host or production-server access.
- **Flagship PvPvE game:** everyone begins in protected space. Unprovoked
  aggression triggers overwhelming NPC enforcement; lawful self-defense and
  explicitly legal combat remain possible. Protected does not mean invulnerable.
  Lawless regions will greatly outnumber protected ones.
- **Ownership and sovereignty:** players/groups can own stations, planets, moons
  and resource rights. Sector sovereignty is claimable in lawless space;
  protected starter jurisdiction remains NPC-controlled. Property rights are
  not permission to bypass server authority or economic invariants.

These are roadmap goals, not implementation promises. Optional graphics and
external settlement retain separate gates; the game does not require a chain
adapter. No dates, player capacity or deployment commitment is implied.

## Documentation

- [Current delivery and executive brief](docs/planning/executive-brief.md)
- [Milestones and dependencies](docs/planning/milestones.md)
- [Creator toolchain and MCP plan](docs/planning/creator-toolchain.md)
- [Simulation, security and ownership contracts](docs/planning/simulation-network-persistence.md)
- [GitHub task index and roadmap board](docs/planning/github-tracker.md)
- [Architecture decisions and their actual statuses](docs/adr/INDEX.md)
- [Release preparation and recovery](docs/releasing.md)
- [Generated changelog](CHANGELOG.md)

The [SDK guide](docs/creator-sdk.md) and [starter walkthrough](docs/creator-project.md)
cover the verified source workflow. More creator tutorials and API reference are
forthcoming; no MCP installation procedure is available. Repository work follows
[AGENTS.md](AGENTS.md); the roadmap is not
blanket authorization to implement, purchase services or deploy infrastructure.

## License

The repository carries [GNU AGPL version 3](LICENSE). Third-party components and
supplied commercial typefaces retain their [separate notices](licenses/brand-typefaces/NOTICE.md)
and permissions; they are not automatically relicensed as AGPL. Review the
applicable source, asset and dependency terms before redistributing an engine or
game build. This roadmap does not grant a proprietary-game licensing exception.
