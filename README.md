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

**[Download a source release.](https://github.com/kyaulabs/ngen/releases)**
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
  undo/redo and source packaging. The optional [headless board playtest](docs/creator-play.md)
  inspects and reloads saved board scenes without running project code.
- Bounded cosmetic particles with a synthetic GPU fixture, and opt-in Vulkan
  motion vectors for opaque and alpha-masked geometry. The flight-history fixture
  verifies camera, shadows, particles and vectors across an origin transfer.
- Checked astronomical coordinates and live origin transfers in one bounded
  physics domain. An opt-in [two-system replay](docs/evidence/M4/flight-acceptance.md)
  prefetches destination assets and waits for upload and collision before arrival.
  The ordinary interactive demo has no warp command.
- Pinned Conan/CMake/Ninja build graphs, tests/sanitizers and source-release
  verification with reviewed back-merges.

The SDK and starter are not a general-purpose scene editor or a stable binary
ABI. The optional [MCP companion](docs/creator-mcp.md) provides read-only local
project discovery. MCP authoring, gameplay economy, multiplayer and sovereignty are planned.
Historical hardware and performance results retain their original source and
configuration limits; see the [executive brief](docs/planning/executive-brief.md).

## Get started

Start with the CPU-only harness. It needs no GPU, display, account, service or
private scene. The first dependency build needs network access.

Follow the [Linux build toolchain guide](docs/build-toolchain.md) bundled with
this source version. Run the following from the root of that checkout or
verified source extraction, using the same shell:

```sh
sh tools/prepare.sh release
cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure
./build/release/sim_harness
```

Graphical execution additionally needs supported Vulkan hardware/drivers, the
desktop audio path and a separately authorized compatible scene; the private
flight scene is not included.

`main` contains reviewed public delivery; `develop` integrates future work.
Use the instructions bundled with your source version rather than assuming
branches share the same behavior or toolchain.

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
