## 📜 Changelog
### [0.14.0](https://github.com/kyaulabs/ngen/releases/tag/v0.14.0)

#### 🚀 Features

- **lifecycle:** Preserve wreck cargo and grant empty starter recovery ([a86f416](https://github.com/kyaulabs/ngen/commit/a86f41625384ee70f5031cf06658973c40fc0023))

- **lifecycle:** Integrate wreck ownership and recovery (#149) ([83345f4](https://github.com/kyaulabs/ngen/commit/83345f4d7597ada74d099d250fa03ad7ed09365a))

### [0.13.0](https://github.com/kyaulabs/ngen/releases/tag/v0.13.0)

#### 🚀 Features

- **creator:** Grant bounded revision-checked MCP edits ([2c057d5](https://github.com/kyaulabs/ngen/commit/2c057d5c722b9770577d3a308a10fb1fbd906ca4))

- **creator:** Integrate revision-checked MCP edits (#146) ([fe598fe](https://github.com/kyaulabs/ngen/commit/fe598feb0707ab4b119588bc4d674b58043446b6))

### [0.12.0](https://github.com/kyaulabs/ngen/releases/tag/v0.12.0)

#### 🚀 Features

- **combat:** Resolve both fixture weapons through one damage path ([bf54bef](https://github.com/kyaulabs/ngen/commit/bf54bef22cac8857e6ac5b0f15103147293ef072))

- **combat:** Integrate shared fixture weapon damage (#143) ([0908e50](https://github.com/kyaulabs/ngen/commit/0908e50f3c5b19f3aecdf99004bfe4b94ed69d33))

### [0.11.0](https://github.com/kyaulabs/ngen/releases/tag/v0.11.0)

#### 🚀 Features

- **mining:** Reserve and transfer bounded offline ore yields ([1b4bd67](https://github.com/kyaulabs/ngen/commit/1b4bd67a4f6d49ded042638753635b3b51dd6ca7))

- **mining:** Integrate bounded offline ore yields (#140) ([6853861](https://github.com/kyaulabs/ngen/commit/68538616ac68fea949e5e7742277a7b0789f33e8))

### [0.10.0](https://github.com/kyaulabs/ngen/releases/tag/v0.10.0)

#### 📝 Documentation

- **roadmap:** Mark the Space MMO project handoffs ([21fde81](https://github.com/kyaulabs/ngen/commit/21fde81d557a08f77b473856aeed2a903e4dd2df))

- **roadmap:** Integrate Space MMO handoff checkpoints (#136) ([7f0ab37](https://github.com/kyaulabs/ngen/commit/7f0ab3770abed4e3ec82787ec2aa8ab0d022f304))


#### 🚀 Features

- **economy:** Add local idempotent inventory and credit transfers ([a9346d8](https://github.com/kyaulabs/ngen/commit/a9346d8683eab92da359db9523c6d36142fbb7aa))

- **economy:** Merge the local offline market fixture (#137) ([322b1c4](https://github.com/kyaulabs/ngen/commit/322b1c4cee5ec094c73f022325f72292ec60655b))

### [0.9.0](https://github.com/kyaulabs/ngen/releases/tag/v0.9.0)

#### 🐛 Fixes

- **flight:** Keep crossing prefetch and arrival failures bounded ([d81a1bf](https://github.com/kyaulabs/ngen/commit/d81a1bfddb7347481b267c9127077a1a2425d11f))


#### 📝 Documentation

- **world:** Define system-wide security for the streaming fixture ([3d2cf35](https://github.com/kyaulabs/ngen/commit/3d2cf354160969b163374d699d29724fed94e07f))

- **world:** Record system-wide streaming security (#115) ([7a3d666](https://github.com/kyaulabs/ngen/commit/7a3d66697ee01a4f1295767ae5a56204ebdff5d6))


#### 🚀 Features

- **travel:** Define a connected two-system fixture ([62d0cd1](https://github.com/kyaulabs/ngen/commit/62d0cd1417089f48aca7526cd4654c4fc439c65b))

- **travel:** Integrate the connected two-system fixture (#116) ([ed77354](https://github.com/kyaulabs/ngen/commit/ed77354c933e68dae1e76658976259697b1ecbc0))

- **travel:** Stage bounded destination collision data ([5ef13a6](https://github.com/kyaulabs/ngen/commit/5ef13a6d6941fae689521a7a20627bb52d031f31))

- **travel:** Integrate destination collision staging (#117) ([d1433c1](https://github.com/kyaulabs/ngen/commit/d1433c1a295bd622ff7145b08e6c330959a57ab2))

- **render:** Retain a scene proxy during destination upload ([a6aa6cd](https://github.com/kyaulabs/ngen/commit/a6aa6cd6a475ede11d1de3d9fdf5cce274e20201))

- **render:** Integrate scene proxy retention during upload (#118) ([beab91f](https://github.com/kyaulabs/ngen/commit/beab91ffc8686bef3780b5ca46ad97650c0f246c))

- **render:** Evict a cached destination scene ([92476f7](https://github.com/kyaulabs/ngen/commit/92476f7e433abe4057dd73bc83c4345bc8181942))

- **render:** Integrate cached destination eviction (#119) ([af68c36](https://github.com/kyaulabs/ngen/commit/af68c36b705aed82fde770ff834920a858a23bd0))

- **flight:** Record host presentation queue timestamps ([de9aecb](https://github.com/kyaulabs/ngen/commit/de9aecb7cecbefc47b7f54a09456888f77243607))

- **flight:** Integrate host presentation queue timestamps (#126) ([7ff9412](https://github.com/kyaulabs/ngen/commit/7ff9412e3608c37e30761e1485e3d00fcaa452b9))

- **flight:** Run a bounded crossing in the flight replay ([ee3d3ab](https://github.com/kyaulabs/ngen/commit/ee3d3abcce8a572cb3505045100a4580512dc4d2))

- **flight:** Integrate a bounded flight crossing replay (#127) ([c10c68e](https://github.com/kyaulabs/ngen/commit/c10c68ec6436c0b2271dc0be9d4ca0c29a37627e))

- **flight:** Prefetch a destination file during crossing replay ([b418e59](https://github.com/kyaulabs/ngen/commit/b418e59fd5c8368bac97ef3365faa4e158385a89))

- **flight:** Render a distinct streamed destination landmark ([6db65b2](https://github.com/kyaulabs/ngen/commit/6db65b2f4f8ec77765f10d2800ee5b67025703e9))

- **flight:** Integrate file-backed destination streaming (#128) ([26eaa35](https://github.com/kyaulabs/ngen/commit/26eaa3520f03fc8f60962e660974c2ffcdf62da0))


#### 🧪 Tests

- **travel:** Gate the native crossing on destination upload ([2416633](https://github.com/kyaulabs/ngen/commit/2416633f3e37c20f19323035d9a047c583a71fbf))

- **travel:** Exercise file failures at the crossing gate ([2ce192f](https://github.com/kyaulabs/ngen/commit/2ce192f5ef904c065bcf7071e4c69191ded607cd))

- **travel:** Integrate the file-backed crossing probe (#120) ([cc7c655](https://github.com/kyaulabs/ngen/commit/cc7c6554efcf40b463de3d6429e5d317cf0d9e4e))

- **travel:** Hold a departure prefetch through warp transit ([a691ab4](https://github.com/kyaulabs/ngen/commit/a691ab493e8e41751940f531108cd588247a5c28))

- **travel:** Integrate delayed departure prefetch (#121) ([d46a61f](https://github.com/kyaulabs/ngen/commit/d46a61fd5e51da8f054d6a68184824358a00d00a))

- **render:** Read back the proxy while warp arrival waits ([d41c4c2](https://github.com/kyaulabs/ngen/commit/d41c4c2f0a6a22ebed8c851491799a91484419d7))

- **render:** Integrate the blocked-crossing proxy readback (#122) ([fa3d5bc](https://github.com/kyaulabs/ngen/commit/fa3d5bc0ea8877c0a4d81fa1d4cd18be8720a7ac))

- **travel:** Measure a warmed destination crossing replay ([8d1aa56](https://github.com/kyaulabs/ngen/commit/8d1aa56b580faf8155f8a6fc0f7e2a95ad65f7eb))

- **travel:** Integrate the warmed crossing replay (#123) ([f2d1a82](https://github.com/kyaulabs/ngen/commit/f2d1a8255b2b76d987187ecb040fa03f441c10e9))

- **travel:** Exercise resource pressure before warp arrival ([3a93f70](https://github.com/kyaulabs/ngen/commit/3a93f7017752df1b48e6d31643be907ec9b05fbc))

- **travel:** Integrate crossing resource pressure checks (#124) ([78dc20b](https://github.com/kyaulabs/ngen/commit/78dc20bcd3f14303878bb30f57749542912264d3))

- **render:** Time proxy frames during destination upload ([19e8ef7](https://github.com/kyaulabs/ngen/commit/19e8ef783906971a4a27f06c7676e1a848d560fc))

- **render:** Integrate proxy frame queue timing (#125) ([145976e](https://github.com/kyaulabs/ngen/commit/145976eade0aa51a11b6e777b79bbeda9d44a6c5))

- **flight:** Recover after an occupied destination berth ([9cbc604](https://github.com/kyaulabs/ngen/commit/9cbc60435ec03dffef8022f4c06f781870e5dc27))

### [0.8.0](https://github.com/kyaulabs/ngen/releases/tag/v0.8.0)

#### 🚀 Features

- **creator:** Expose read-only project discovery over mcp ([ffeca5d](https://github.com/kyaulabs/ngen/commit/ffeca5d52ec92a4216d09f44e2df199ecff59e4e))

- **creator:** Integrate read-only project discovery over mcp (#112) ([26df6d5](https://github.com/kyaulabs/ngen/commit/26df6d5095f82fc68a156657b79164d822d908a8))

### [0.7.0](https://github.com/kyaulabs/ngen/releases/tag/v0.7.0)

#### 🚀 Features

- **creator:** Add an owned headless board playtest ([4bb6527](https://github.com/kyaulabs/ngen/commit/4bb652745628fe50f4ff4cdd80341ecf3f2487bc))

- **creator:** Integrate owned headless board playtest (#108) ([3ad3f2a](https://github.com/kyaulabs/ngen/commit/3ad3f2a5cf1ab83279db8ab2c39330bae7c9f14b))

### [0.6.0](https://github.com/kyaulabs/ngen/releases/tag/v0.6.0)

#### 🚀 Features

- **travel:** Add a bounded warp with gated local arrival ([d65508c](https://github.com/kyaulabs/ngen/commit/d65508c25720d3d137cf103a17ad3355ff629a4f))

- **travel:** Integrate bounded warp with gated local arrival (#106) ([7b217ed](https://github.com/kyaulabs/ngen/commit/7b217edb36492f676d7b6fa73703bae82650068c))

### [0.5.0](https://github.com/kyaulabs/ngen/releases/tag/v0.5.0)

#### 🚀 Features

- **flight:** Align camera pose history across origin epochs ([20140fb](https://github.com/kyaulabs/ngen/commit/20140fb06fef8e8b6e50240134a8b53f51d3a988))

- **flight:** Preserve authored navigation and views during rebases ([2f22ab6](https://github.com/kyaulabs/ngen/commit/2f22ab6d8f9b414d4a735d24f4dda86bf7e330af))

- **flight:** Verify combined presentation history across rebases ([9540468](https://github.com/kyaulabs/ngen/commit/95404680ccd5e40911a1639155c5977660ea9061))

- **flight:** Integrate combined frame history across rebases (#102) ([0db7a9a](https://github.com/kyaulabs/ngen/commit/0db7a9a731ff394aed609fce05c946b2fd337cc2))


#### 🛠 Maintenance

- **flight:** Reconcile frame history with the motion attachment ([b7e5142](https://github.com/kyaulabs/ngen/commit/b7e51424b235011e12786a699ccaaff526e66636))


#### 🧪 Tests

- **flight:** Retain checked projection coordinates for history checks ([fe9f31d](https://github.com/kyaulabs/ngen/commit/fe9f31dd6ac98dd89efdab4bfc1b80bc5f8826fb))

### [0.4.0](https://github.com/kyaulabs/ngen/releases/tag/v0.4.0)

#### 🚀 Features

- **render:** Verify bounded cosmetic particles across origin transfers ([b12d806](https://github.com/kyaulabs/ngen/commit/b12d8068c769548558c930d0cfbdb7d86b6e774f))

- **render:** Integrate bounded cosmetic particle simulation (#99) ([541ceb9](https://github.com/kyaulabs/ngen/commit/541ceb911e847e8616f91fbbce717839975273a4))

- **render:** Verify motion-vector shader output on device ([ba983a7](https://github.com/kyaulabs/ngen/commit/ba983a71a9abc412b1e7d6d998b2b7fddfc12d82))

- **render:** Own motion-vector output in the flight mesh pass ([ede90a7](https://github.com/kyaulabs/ngen/commit/ede90a75a89aa6d8c286598179dc78dd6756078a))

- **render:** Integrate optional gpu motion-vector output (#100) ([60ee7ce](https://github.com/kyaulabs/ngen/commit/60ee7cee005ead788af4e5d0978a3e12a3fcd15e))

### [0.3.0](https://github.com/kyaulabs/ngen/releases/tag/v0.3.0)

#### 🚀 Features

- **creator:** Author and save bounded starter scenes ([1b076e7](https://github.com/kyaulabs/ngen/commit/1b076e7051e610a1495cf6ee90105af63a6c5602))

- **creator:** Integrate bounded scene authoring and saves (#94) ([d39dd65](https://github.com/kyaulabs/ngen/commit/d39dd65cd3e27e286e1055c2ea990dd325a2b2c8))

### [0.2.0](https://github.com/kyaulabs/ngen/releases/tag/v0.2.0)

#### 🐛 Fixes

- **sdk:** Keep install metadata out of producer package search ([1c7b3f0](https://github.com/kyaulabs/ngen/commit/1c7b3f048017198e31502ac08bf040bf81e9a796))

- **physics:** Keep shape ownership in native body settings ([e120d56](https://github.com/kyaulabs/ngen/commit/e120d56fed976282a68cd7263a30a479b38596fc))


#### 📝 Documentation

- **plan:** Record the dependency-checked roadmap execution order ([57af656](https://github.com/kyaulabs/ngen/commit/57af65674b43cb0f94a098aef0700f0dac648ab2))

- **plan:** Add a resumable issue-by-issue execution order (#86) ([7840cb4](https://github.com/kyaulabs/ngen/commit/7840cb49ae558d7d6f30f6ee62582d734d00b162))

- **plan:** Record the verified c1-a sdk checkpoint ([58ba347](https://github.com/kyaulabs/ngen/commit/58ba347becdd021c5f94006a4eae381ed641d2da))

- **plan:** Record the verified c1-a sdk checkpoint (#88) ([124d172](https://github.com/kyaulabs/ngen/commit/124d1722bef5129fb29a4f8c9fc1837671a61cc1))


#### 🚀 Features

- **sdk:** Expose source-built engine targets to external projects ([647346e](https://github.com/kyaulabs/ngen/commit/647346e51b9803ec4fb1c7a23ad5e41a52c6c060))

- **sdk:** Expose source-built engine targets to external projects (#87) ([93c7339](https://github.com/kyaulabs/ngen/commit/93c7339e0e4fd9a9f5b9c547b20e6e33a2c6a844))

- **creator:** Add a standalone source project workflow ([05718b8](https://github.com/kyaulabs/ngen/commit/05718b88b1f688a063c9d8bab0d9cec983cb7435))

- **creator:** Add a standalone source project workflow (#89) ([61693cd](https://github.com/kyaulabs/ngen/commit/61693cd56eac5e5b8aedd1f943bae870def7271c))

- **world:** Implement bounded galactic coordinates and frame transforms ([82d3c9f](https://github.com/kyaulabs/ngen/commit/82d3c9f4e48d33ee9ae21bd99756944c96913101))

- **world:** Implement bounded galactic coordinates and frame transforms (#90) ([4db4cb3](https://github.com/kyaulabs/ngen/commit/4db4cb300fe1f6d4da5c95086325f63d36de9b6f))

- **physics:** Translate a live domain at the tick barrier ([4a1527f](https://github.com/kyaulabs/ngen/commit/4a1527fd6a296cbe667d2778c744b8d226025bdb))

- **physics:** Integrate live tick-barrier origin transfers (#91) ([d34f094](https://github.com/kyaulabs/ngen/commit/d34f094f22f434b9d74215083a2d1f6ea9d67fbb))


#### 🧪 Tests

- **release:** Check changelog format on untagged integration heads ([9db09fb](https://github.com/kyaulabs/ngen/commit/9db09fb7d2e6509b91497dab5a457901b2af92ce))

- **release:** Support untagged changelog verification (#85) ([7dfe1c2](https://github.com/kyaulabs/ngen/commit/7dfe1c2f529015501f755c898e563b84082676b7))

### [0.1.1](https://github.com/kyaulabs/ngen/releases/tag/v0.1.1)

#### 📝 Documentation

- **plan:** Define reusable creators and security-law roadmap ([5f6ce3b](https://github.com/kyaulabs/ngen/commit/5f6ce3bef93dc2801ca9c476eea21b2b06d36888))

- **plan:** Record the synchronized creator and sovereignty backlog ([17a93a2](https://github.com/kyaulabs/ngen/commit/17a93a23829207cb016f7951d790214bf0fabf38))

- **plan:** Integrate creator and security-law roadmap (#82) ([104adbd](https://github.com/kyaulabs/ngen/commit/104adbd7834e03d39d1fd1e6fd34f85083a8382f))

### [0.1.0](https://github.com/kyaulabs/ngen/releases/tag/v0.1.0)

#### ♻ Refactoring

- **ui:** Share the bounded context overlay frontend ([7b4615d](https://github.com/kyaulabs/ngen/commit/7b4615dc0031903565a1f65c3241c15513be49da))


#### 🎨 Style

- **ui:** Finalize the accepted half-opacity ui baseline ([8d4d89c](https://github.com/kyaulabs/ngen/commit/8d4d89c11b4dc3f730f8a9f6b9416091156b4d24))


#### 🐛 Fixes

- **ci:** Check the complete reviewed revision range ([9c33bb6](https://github.com/kyaulabs/ngen/commit/9c33bb611faab4f2fefa72319d6ee5cb07a5351b))

- **ci:** Use the published CMake compatibility package ([65080e1](https://github.com/kyaulabs/ngen/commit/65080e118363552aac7cdd0a787bda2a98663226))

- **ci:** Create the offline namespace before dropping privileges ([3b9b077](https://github.com/kyaulabs/ngen/commit/3b9b0779bf6d010e3d95ce77aad6fbc70428401e))

- **build:** Align Conan toolchain generation with Ninja ([e141aa7](https://github.com/kyaulabs/ngen/commit/e141aa7afdb16c830384dc04101f7154eedcaf1d))

- **build:** Select warning levels by compiler frontend ([54a7a63](https://github.com/kyaulabs/ngen/commit/54a7a63f03982d198e68c97a802a20ee743028e7))

- **windows:** Inherit configured Vulkan SDK for renderer builds ([c730a18](https://github.com/kyaulabs/ngen/commit/c730a18a541f66a294f620e88e680985f0160be3))

- **render:** Repair pinned sdl wayland object ownership ([4d368ab](https://github.com/kyaulabs/ngen/commit/4d368ab6c364643e2f93cb54854b6ab0749e8960))

- **test:** Keep wayland fixture stable across formatters ([4d98647](https://github.com/kyaulabs/ngen/commit/4d986473e1a49c2fbb697fd90201e97c7ef1c8ba))

- **ci:** Distinguish advisory vendor ast findings from owned failures ([0258bda](https://github.com/kyaulabs/ngen/commit/0258bda179eb64528c9fda1b1dddd3459e318bc4))

- **render:** Make aligned payload padding explicit for msvc ([354e11e](https://github.com/kyaulabs/ngen/commit/354e11e3f7f971661508cac7f2da8226a36d9202))

- **render:** Keep image aspects in the unsigned mask type ([e387dde](https://github.com/kyaulabs/ngen/commit/e387dde6a190b8003e4be719ee366659595d4cbe))

- **render:** Integrate the owner image aspect correction ([9c0c2e2](https://github.com/kyaulabs/ngen/commit/9c0c2e26b9aa3730536af6f59487f27fb9cc1dfe))

- **render:** Make aligned payload padding explicit for msvc ([8148b24](https://github.com/kyaulabs/ngen/commit/8148b242eba87890fe5daa1fa6e721860926ed73))

- **render:** Keep image aspects in the unsigned mask type ([5e7f391](https://github.com/kyaulabs/ngen/commit/5e7f391d8d25d39057d01c42b5335094591801e6))

- **ui:** Align panels to a shared outer gutter ([2e88dce](https://github.com/kyaulabs/ngen/commit/2e88dced74e07f7f696f9856a977d94d66556ed7))

- **ui:** Enclose rear engines in assembled ship bounds ([5262238](https://github.com/kyaulabs/ngen/commit/526223868073653835a7c6ef0e19bee0716b2d23))

- **windows:** Reject incomplete gpu observation windows ([b92ee53](https://github.com/kyaulabs/ngen/commit/b92ee53576e9443788c3035ee1fd293a75612f5f))

- **render:** Keep turbulent flow lattice interpolation continuous ([c6b82da](https://github.com/kyaulabs/ngen/commit/c6b82dac08c41f9b73b92cad7ae6b00e569a40fc))

- **physics:** Simplify owned box shape lifetime ([a8918ef](https://github.com/kyaulabs/ngen/commit/a8918ef56c5d99a34266471f1f7117e1bc089fc1))

- **demo:** Restore tactical framing after the replay turn ([660c9dd](https://github.com/kyaulabs/ngen/commit/660c9dd1dc8efc033760ca3a2de1468edf376bd9))

- **demo:** Bound selected-target hud projection ([fb8135d](https://github.com/kyaulabs/ngen/commit/fb8135dacd1d4ace6bb91a61d0980b79f57b8313))

- **flight:** Improve bounded frigate steering response ([8a37eb6](https://github.com/kyaulabs/ngen/commit/8a37eb61f206b6c14fe462159121894e9bc65ac5))

- **flight:** Trim the revised steering rate by 7.5 percent ([489fb53](https://github.com/kyaulabs/ngen/commit/489fb5388aa1c473213434d28f29ddc1a6109082))

- **flight:** Cancel stale navigation on manual takeover ([95645f6](https://github.com/kyaulabs/ngen/commit/95645f6deb565ca05d4c820f8c695b726f7fe38a))

- **windows:** Reuse the active visual studio environment ([131bc55](https://github.com/kyaulabs/ngen/commit/131bc550312afc5d89e49719b020ca9d44493b39))

- **render:** Make copied scene view alignment explicit ([02ce967](https://github.com/kyaulabs/ngen/commit/02ce967c804095010be3c0128d3f074d162b8463))

- **windows:** Resolve remaining flight compiler diagnostics ([41e71a8](https://github.com/kyaulabs/ngen/commit/41e71a8540a23fd29d6218d2cabef6751d4a461e))

- **flight:** Support full-hd capture without warm hud growth ([10e5a73](https://github.com/kyaulabs/ngen/commit/10e5a73bd365ed1def1378ab24e8744ee673f721))

- **physics:** Honor clang-cl runtime selection in jolt builds ([9740462](https://github.com/kyaulabs/ngen/commit/97404620396cf0cf0c1839f3cc2c04bcfee1a1ea))

- **windows:** Preserve absent environment after offline checks ([6a41458](https://github.com/kyaulabs/ngen/commit/6a41458f9982f239cf14e10d15aad14c0db70d93))

- **flight:** Expose aft rcs banks and balance jet coverage ([c63c452](https://github.com/kyaulabs/ngen/commit/c63c4520430f1e50ff93f6b45b112a5eb7cf5881))

- **flight:** Select directional rcs turn and arrest sets ([c561865](https://github.com/kyaulabs/ngen/commit/c561865dc36835b1e043bd05aa5a83b4dbbbcab6))

- **audio:** Share Linux playback with the desktop sound server ([c6880a1](https://github.com/kyaulabs/ngen/commit/c6880a1f0f80b0dd310b50cf4523e0d3eae86325))

- **package:** Omit unused desktop audio SDK imports from flight ([7c9b0d7](https://github.com/kyaulabs/ngen/commit/7c9b0d798d16462fae44161e43e1a7552c60d2c4))

- **tools:** Preserve structured gpu counter query failures ([5ea9e3c](https://github.com/kyaulabs/ngen/commit/5ea9e3c44fe5fa4660de38624b302ab637a1d1a6))

- **flight:** Prewarm bounded force-plot tooltip storage ([3fd093f](https://github.com/kyaulabs/ngen/commit/3fd093f48dfe112bc39af4651edfe21a64fc961c))

- **ci:** Preserve gpu diagnostic suite success exit ([a206b31](https://github.com/kyaulabs/ngen/commit/a206b31cd09fb4cc62fcaffdcd011689db1929af))

- **windows:** Capture pdh data in an explicit csv ([34e6adf](https://github.com/kyaulabs/ngen/commit/34e6adf70139e3168bab6c18aeee2b66ec2cf31d))

- **windows:** Collect workload gpu memory through pdh ([7afeefe](https://github.com/kyaulabs/ngen/commit/7afeefe568110a042c41ff258d44da490e2f0112))

- **ui:** Fit flight panels and remove repeated logos ([a43fa7a](https://github.com/kyaulabs/ngen/commit/a43fa7ab81515a5408c9f910736e472768220b5a))

- **ui:** Remove runtime shortcut annotations ([4a7aa4e](https://github.com/kyaulabs/ngen/commit/4a7aa4e5d711d1783aaa3e5a9b5206415ab65109))

- **ui:** Move target actions into a context menu ([e1f39d5](https://github.com/kyaulabs/ngen/commit/e1f39d5b8006e5b6eceaaa2c1816c2b4ddf0dd2a))

- **ui:** Preserve camera-only free-look clicks ([532b821](https://github.com/kyaulabs/ngen/commit/532b8217c6b858134b76878e99ea866408bddaac))

- **ui:** Keep context clicks absolute and soften menu backgrounds ([6bc1b77](https://github.com/kyaulabs/ngen/commit/6bc1b779a687f0aae5e9d85bc43592af38e6b97e))

- **demo:** Bind delivery to built source and preserve alt ownership ([21e616c](https://github.com/kyaulabs/ngen/commit/21e616c13c8a9344a84908a9b00bd53f1d35a8b8))

- **gui:** Coalesce mouse bursts without losing input order ([5fb8ddf](https://github.com/kyaulabs/ngen/commit/5fb8ddfd31003ffa7ff97fa1021ba961a444dcd5))


#### 📝 Documentation

- **render:** Record Windows RTX 5070 capability evidence ([7e982de](https://github.com/kyaulabs/ngen/commit/7e982de1355edaea256ace5ca89ee1d5ae896c77))

- **render:** Record Windows MSVC validated startup results ([2c5367e](https://github.com/kyaulabs/ngen/commit/2c5367e0a87a0dc0ed16d86ad7bd6fb94921399f))

- **evidence:** Record clang-cl startup and wayland leak tracing ([c9fce21](https://github.com/kyaulabs/ngen/commit/c9fce21f088d36ab8e82bf7b308bf35fed503591))

- **evidence:** Record windows first-light validation ([f86f1c9](https://github.com/kyaulabs/ngen/commit/f86f1c9599dc75a58a0dfcbb96e43e947cb7c68b))

- **evidence:** Record windows gpu resource validation ([bfc9d57](https://github.com/kyaulabs/ngen/commit/bfc9d57b259b8f8615384492395f5182dd9090e2))

- **evidence:** Retain windows debug scene validation ([c6fe31d](https://github.com/kyaulabs/ngen/commit/c6fe31db0290174250216f57ece3973756087e3a))

- **evidence:** Retain full frigate windows nvidia validation ([7be95f4](https://github.com/kyaulabs/ngen/commit/7be95f46e7bbaae4f5b0f3a2c94cb4a083978c5a))

- **evidence:** Record checked scene integration follow-up ([7225d41](https://github.com/kyaulabs/ngen/commit/7225d41372d667b81522b67fc0aca75f1d519d78))

- **evidence:** Record checked scene integration follow-up ([76e7c29](https://github.com/kyaulabs/ngen/commit/76e7c294188fc64e81c96dce6052b1636f4213e6))

- **ui:** Add original eve-inspired inspector mockup ([b5d3977](https://github.com/kyaulabs/ngen/commit/b5d3977c167c414437221d7dc689335a0fdf6242))

- **evidence:** Verify windows inspector bloom and reload workload ([625cef1](https://github.com/kyaulabs/ngen/commit/625cef142031086bad3a4776ec5fec015710d584))

- **evidence:** Refresh verified handoff document checksum ([e0c4cf0](https://github.com/kyaulabs/ngen/commit/e0c4cf02fdc679e0c8272182dbe43deac7774e72))

- **render:** Retain independent corrective review ([5a2f841](https://github.com/kyaulabs/ngen/commit/5a2f8415aa226d9f757dfa18af403ab039d51806))

- **render:** Retain independent native memory review ([b45d388](https://github.com/kyaulabs/ngen/commit/b45d38823f4e0a69e35dc4b64b1b8314392780d6))

- **assets:** Retain independent exhaust review ([ec0809c](https://github.com/kyaulabs/ngen/commit/ec0809c6db67c7513022c49a22894a34af8b1014))

- **render:** Record owner inspector acceptance ([478dd2d](https://github.com/kyaulabs/ngen/commit/478dd2dfeda827aa1933b8648c517d7d46d252be))

- **m2:** Retain final acceptance and Windows scope ([6210056](https://github.com/kyaulabs/ngen/commit/6210056784689c975464f1b769c665addbca82c8))

- **m2:** Distinguish final asset from stale Windows source ([062b4b9](https://github.com/kyaulabs/ngen/commit/062b4b99a206b8060067a355b43d640cc267bbd9))

- **m2:** Verify current flow and final asset on Windows ([8ab4b46](https://github.com/kyaulabs/ngen/commit/8ab4b46334005a0ff8c650b50eeab202bdc229ee))

- **evidence:** Retain final laptop and owner acceptance ([ef85f4c](https://github.com/kyaulabs/ngen/commit/ef85f4c28b50f9bf30f4e5b0342b9fa5d680cdf6))

- **plan:** Queue eve-inspired ui work before m3 completion ([9f5dbbf](https://github.com/kyaulabs/ngen/commit/9f5dbbf0e5c5bfbcf5a1a2ae6f8e339abfbd9b60))

- **plan:** Record offline acceptance and missing rcs visuals ([f360260](https://github.com/kyaulabs/ngen/commit/f36026085265ab63a6086ab3664367bf7291c4d5))

- **evidence:** Record sdk-less rcs delivery checkpoint ([bc65a1b](https://github.com/kyaulabs/ngen/commit/bc65a1b4d194530dca6f88e3701c9dbecbaed336))

- **evidence:** Record offline 32-jet rcs bundle ([b7f5c01](https://github.com/kyaulabs/ngen/commit/b7f5c01c0cb1644dfc129e9767b3ecfdc650b7e6))

- **evidence:** Record offline pulsed rcs delivery ([fc5d985](https://github.com/kyaulabs/ngen/commit/fc5d985f13671790a36ee870af4b4fa34121eaea))

- **evidence:** Record directional rcs offline bundle ([0ecaa50](https://github.com/kyaulabs/ngen/commit/0ecaa50519abd99a3877654d053fa980d9d8da5f))

- **evidence:** Record authoritative rcs offline delivery ([a5bda67](https://github.com/kyaulabs/ngen/commit/a5bda679449b6273dadd726c3351858eafc04dbb))

- **evidence:** Record shared desktop audio offline delivery ([c3952a7](https://github.com/kyaulabs/ngen/commit/c3952a70b716102d68bbc58c75060c415e08c797))

- **evidence:** Record owner confirmation of v13 desktop audio ([dfac270](https://github.com/kyaulabs/ngen/commit/dfac270ead06ad8998466566c93bc1bb9ea5612e))

- **evidence:** Record current physical windows verification ([7960c6f](https://github.com/kyaulabs/ngen/commit/7960c6f8b6ccc5b238cc3350297e9cb5f1745b88))

- **planning:** Record post-m3 roadmap and github project work ([98de851](https://github.com/kyaulabs/ngen/commit/98de8514d4e9b189e0eb68697ee348801c342739))

- **windows:** Record native pdh coverage and rtx timing miss ([7f12573](https://github.com/kyaulabs/ngen/commit/7f1257378702783b93d649e6cb5c36b38fbd49a6))

- **windows:** Verify msvc pdh coverage and frame cadence ([1e394f5](https://github.com/kyaulabs/ngen/commit/1e394f569cb97331aa2583ced95f643a8fee3709))

- **windows:** Record artemis measurement context ([add18d8](https://github.com/kyaulabs/ngen/commit/add18d832d0626d74ad26459454a916c547de857))

- **ui:** Record visual study and immersive hud direction ([358746b](https://github.com/kyaulabs/ngen/commit/358746b0015bd62bd57bbfdb95e37fe2e3d43483))

- **ui:** Record verified offline first-pass preview ([380ed9c](https://github.com/kyaulabs/ngen/commit/380ed9cb6398263def8458a1faf687293e689e32))

- **evidence:** Record scoped post-ui windows and laptop results ([31ab2af](https://github.com/kyaulabs/ngen/commit/31ab2afc1c4398d20bb2fb4f8e225b6415271854))

- **evidence:** Verify repaired native clangcl hover check ([1d3bcf2](https://github.com/kyaulabs/ngen/commit/1d3bcf26dfa44156bbb7017ebb4698b8358208aa))

- **evidence:** Record three valid active-profile msvc budget runs ([c27f1ec](https://github.com/kyaulabs/ngen/commit/c27f1ecf76dd950ab3cb1c37fff01a531831256d))

- **evidence:** Refresh the final source-bound linux delivery ([8f46cb2](https://github.com/kyaulabs/ngen/commit/8f46cb27b8c4fed72ee54dcfa922842b7a4644ce))

- **evidence:** Record the source-bound graphite preview and private clip ([059586c](https://github.com/kyaulabs/ngen/commit/059586caa179c14630c8f27ee4e8227a0095e0b9))

- **evidence:** Record identity 1.4 preview and unresolved runtime observations ([51e0793](https://github.com/kyaulabs/ngen/commit/51e07933f2502cbcc2bfaaec34720fe667098b32))

- **evidence:** Accept the identity 1.4 preview and scope final runtime checks ([254c709](https://github.com/kyaulabs/ngen/commit/254c709d0ff5b0444b4b4326d3aa64e6a1a6bc37))

- **planning:** Advance the roadmap from accepted M3 to the M4 entry gate ([469f5c6](https://github.com/kyaulabs/ngen/commit/469f5c661e09909e65f2ebb5be8b9009289d6e3b))

- **planning:** Index the dedicated kanban and complete remaining task backlog ([bc042ec](https://github.com/kyaulabs/ngen/commit/bc042ecb5e7b81f1351d6b9c487e952995a6d815))

- **planning:** Publish the post-m3 roadmap and task board (#67) ([cbb0c62](https://github.com/kyaulabs/ngen/commit/cbb0c62b4b1dfef2dcc514c43d10d68eda3b1274))


#### 🔧 Automation

- **windows:** Capture inspector bloom and reload workload evidence ([1f0a4b3](https://github.com/kyaulabs/ngen/commit/1f0a4b31a3db73f802513eab6edebd319f3c47cb))

- **release:** Publish verified source releases and open reviewed back-merges ([c34dc55](https://github.com/kyaulabs/ngen/commit/c34dc55768deb275b65dbcde2536c2d4c1001ecf))

- **release:** Add verified source release and back-merge automation (#68) ([ec750e8](https://github.com/kyaulabs/ngen/commit/ec750e845899f8d029fb923fcdd36f11d317bf5c))


#### 🚀 Features

- **core:** Establish the Linux CPU foundation ([888aa10](https://github.com/kyaulabs/ngen/commit/888aa10be749c4ca6c414e404765511a5e60617a))

- **render:** Add the validated Vulkan startup slice ([f208609](https://github.com/kyaulabs/ngen/commit/f20860924e8f16e3acade85e4d9bf9643236c6d2))

- **build:** Automate Windows CPU verification ([d49b23c](https://github.com/kyaulabs/ngen/commit/d49b23c57fd3b02235ace2a1c10e85e9f93c97ca))

- **build:** Add clang-cl Windows CPU verification ([63d0a53](https://github.com/kyaulabs/ngen/commit/63d0a538f48881e8abe210d233b13ef939740369))

- **build:** Capture Windows Vulkan capabilities with one command ([dc8eb76](https://github.com/kyaulabs/ngen/commit/dc8eb76f0b9e800098e0d1041a26fd4d0f7a9bd3))

- **render:** Prepare locked Windows Vulkan startup checks ([85ec7ca](https://github.com/kyaulabs/ngen/commit/85ec7ca5a8763ac2cba0ff7fe74400cbb34b691f))

- **render:** Select and inspect supported surface configurations ([4811662](https://github.com/kyaulabs/ngen/commit/481166284e7aa8eaf67e5eb8967a53a24c592526))

- **render:** Add fenced clear-only swapchain presentation ([b7f0334](https://github.com/kyaulabs/ngen/commit/b7f0334f3a932d3717c0a927d126f48c96f6f9cb))

- **render:** Own bounded gpu resources with timeline retirement ([dbdd3ba](https://github.com/kyaulabs/ngen/commit/dbdd3bad93c9205fcfab287e854714ff69f293ed))

- **render:** Integrate verified engine foundation ([e1cc4f4](https://github.com/kyaulabs/ngen/commit/e1cc4f4f031f845ca5509c200ad9c211919f1a00))

- **assets:** Import bounded static glb scenes with tangent cooking ([8775695](https://github.com/kyaulabs/ngen/commit/8775695623c24944d386728d17acbd2bd6ab40be))

- **assets:** Cook checked immutable scenes and load native mip packages ([97affbd](https://github.com/kyaulabs/ngen/commit/97affbd5e346c812fc586853b80829aae75e7854))

- **render:** Validate offline shader interfaces on native gpu fixtures ([1516b3e](https://github.com/kyaulabs/ngen/commit/1516b3ea54b12a4c891cd1a960920f4a6d359074))

- **render:** Integrate shader and immutable scene foundations ([ed8d039](https://github.com/kyaulabs/ngen/commit/ed8d039493b270bca89c039dede09c6f6c6f912e))

- **render:** Track complete mip chains and hdr depth cube images ([885e3d1](https://github.com/kyaulabs/ngen/commit/885e3d12c62d8c6385e35df6fcdd684270a960a1))

- **render:** Draw tracked hdr post passes with embedded shaders ([893efae](https://github.com/kyaulabs/ngen/commit/893efaea77a77539d27b0e3349c0265e9e5d1a50))

- **render:** Join sampled wsi frames to resource retirement ([0a7fc3b](https://github.com/kyaulabs/ngen/commit/0a7fc3bccb0f233b72ef5ba98874d1dcbd6e3c03))

- **render:** Publish owning prepared gpu scene versions ([70dad0b](https://github.com/kyaulabs/ngen/commit/70dad0b1886ad412569a5d6c0b031be7cfd5ba8f))

- **render:** Preserve imported uv and sampler shader contracts ([9b60f4f](https://github.com/kyaulabs/ngen/commit/9b60f4f73f662b78ebf31343020fd70f0b859b5b))

- **render:** Draw tracked indexed pbr batches with depth ([66b93ff](https://github.com/kyaulabs/ngen/commit/66b93ff1f6311380104ea9b49cfd5eda5882d5c4))

- **render:** Draw checked scene versions with owning frame pins ([862bf9e](https://github.com/kyaulabs/ngen/commit/862bf9eb5344590c5240da03c2cb273a798244a0))

- **tools:** Verify combined scene rendering on windows ([64faca6](https://github.com/kyaulabs/ngen/commit/64faca6be6e2db1eb4392b5bc796d75ca2538f6d))

- **render:** Generate bounded diffuse specular and brdf lighting data ([3fba1bf](https://github.com/kyaulabs/ngen/commit/3fba1bf2da7fb57cdfff2f200e8013b805c8782f))

- **render:** Integrate verified shader asset and draw foundations ([1b8e921](https://github.com/kyaulabs/ngen/commit/1b8e921e7d671149378b90c8d9fa6cb92e9fc88f))

- **render:** Base remaining scene work on integrated develop ([ad8cb92](https://github.com/kyaulabs/ngen/commit/ad8cb92439b5159f6982cf7c9cdab38137bb0fb5))

- **render:** Generate tracked shadows and validate full scene captures ([ef984ef](https://github.com/kyaulabs/ngen/commit/ef984ef18c6fb6ed9886f45003dc67e85d42170c))

- **render:** Integrate verified checked scene lighting and shadows (#6) ([eea84ad](https://github.com/kyaulabs/ngen/commit/eea84adbb79d2f6c1799d3043f60e2649ae200cb))

- **render:** Add instanced ship workload and gpu pass timing ([a346c29](https://github.com/kyaulabs/ngen/commit/a346c29ae95596d16cf8623f68a5396b2a095432))

- **tools:** Queue inspector snapshots and checked package reloads ([87fe98c](https://github.com/kyaulabs/ngen/commit/87fe98c7ed065ab39a287b5da095e11f06520aee))

- **tools:** Compose tracked inspector and publish scene reloads ([19b43a9](https://github.com/kyaulabs/ngen/commit/19b43a9607ebec292b237bc92610b57e49f20211))

- **render:** Add restrained bloom and fixed-resolution reload diagnostics ([42ce0e7](https://github.com/kyaulabs/ngen/commit/42ce0e7705e671a4bcc90534c9effedd378c1980))

- **render:** Deliver instanced workload and queued inspector (#8) ([20f1222](https://github.com/kyaulabs/ngen/commit/20f12220166335c7d94f0ed6086171f6dbd00c1f))

- **assets:** Retain accepted blue-white exhaust presentation ([afb94f7](https://github.com/kyaulabs/ngen/commit/afb94f7da8ea0def13677b2bde00f52674b70a46))

- **brand:** Add owner-supplied visual identity pack ([d036c91](https://github.com/kyaulabs/ngen/commit/d036c914180e6a16bdf333244c621305dee1f175))

- **render:** Add explicit turbulent emissive flow ([659d570](https://github.com/kyaulabs/ngen/commit/659d570f61e4d9a5ab8fb5e505cc03ab127775fc))

- **render:** Complete M2 acceptance (#9) ([4b554cc](https://github.com/kyaulabs/ngen/commit/4b554cccf9355ce36fa90e4896a694b4654279d6))

- **physics:** Add bounded headless local rigid bodies ([1189843](https://github.com/kyaulabs/ngen/commit/1189843b820821c7e541172ddf4e70d3772dfa15))

- **flight:** Share bounded direct and tactical actuation ([aae5f8a](https://github.com/kyaulabs/ngen/commit/aae5f8a4a08e0e5069cd4448171628dfe4e04d63))

- **camera:** Add tactical and ship-mounted presentation ([c3805b6](https://github.com/kyaulabs/ngen/commit/c3805b6d0475fad04662ef807c4bde407fbd1ac1))

- **render:** Apply copied rigid roots to immutable scenes ([0fd447b](https://github.com/kyaulabs/ngen/commit/0fd447bb5f91b8cd4a614d210201c40753932cc2))

- **input:** Bound flight controls and release stale intent ([c87d1e1](https://github.com/kyaulabs/ngen/commit/c87d1e1c6a6f950a95361395ca1b11d4528279cb))

- **flight:** Preserve copied static contact presets ([22f2bb0](https://github.com/kyaulabs/ngen/commit/22f2bb000ef09133a78a028fb3156fd1f5abd182))

- **demo:** Add the bounded playable flight bridge ([cf72519](https://github.com/kyaulabs/ngen/commit/cf725196bd0513245f113304dc99f1ec502ae124))

- **audio:** Add bounded procedural flight playback ([2837627](https://github.com/kyaulabs/ngen/commit/28376273e1dc524beab4a2180faf0784403d9a37))

- **demo:** Add offline bundles and the windows flight handoff ([19acb3c](https://github.com/kyaulabs/ngen/commit/19acb3c165016212affb771d0c22be7dcda3a8ba))

- **flight:** Render wrench-driven paired rcs jets ([464d3fd](https://github.com/kyaulabs/ngen/commit/464d3fda1f8781cea2b96199c8ffe4e5db9c3874))

- **flight:** Pulse rcs exhaust from fixed-tick actuation ([8968947](https://github.com/kyaulabs/ngen/commit/8968947fb9ecbb17bdc04e6bb462392e05980b70))

- **flight:** Make rcs firings authoritative physical actuators ([4aa217c](https://github.com/kyaulabs/ngen/commit/4aa217c41bc447abc98ca90435818bc6131e614e))

- **flight:** Add f12 whole-system performance overlay ([62a984d](https://github.com/kyaulabs/ngen/commit/62a984db10387c7e5721476ddee3abfaf0df2f2e))

- **windows:** Add read-only gpu provider diagnosis ([e92f2f9](https://github.com/kyaulabs/ngen/commit/e92f2f9328dd5fddc21b8e481a929a5625666fb7))

- **windows:** Compare direct pdh and cim memory counters ([e669b92](https://github.com/kyaulabs/ngen/commit/e669b92f9f29c34176c199824197988267894cb6))

- **ui:** Add immersive flight hud and shared branding ([754c72d](https://github.com/kyaulabs/ngen/commit/754c72d8011c8d2fcf79b53a2806a439cd1d8d58))

- **tools:** Add runnable windows flight check scripts ([a4ebfc6](https://github.com/kyaulabs/ngen/commit/a4ebfc6dd1819b6e63515557d096f62f7ac36308))

- **demo:** Apply graphite panels and record private linux clips ([64e275a](https://github.com/kyaulabs/ngen/commit/64e275ab76818916a524dfdc2b654dcdc3382b38))

- **ui:** Adopt visual identity 1.4 materials and licensed fonts ([e45ba67](https://github.com/kyaulabs/ngen/commit/e45ba673040a348959cda48f95e9380a2e9f980e))

- **flight:** Complete the M3 offline flight demonstration (#10) ([03b76c7](https://github.com/kyaulabs/ngen/commit/03b76c724a556a30944b39072afbee4aa2dfcf8b))


#### 🛠 Maintenance

- **repo:** Add AGENTS.md and update .gitignore ([dc52445](https://github.com/kyaulabs/ngen/commit/dc52445d90564e65ddd6471863c92b9ef31d674d))

- Add NGEN planning package and local Prism C++ support ([f6ccd18](https://github.com/kyaulabs/ngen/commit/f6ccd18e47fa066b9e5c8623324d3141d63f0f4a))

- **windows:** Record the verified MSVC profile and CPU results ([144404c](https://github.com/kyaulabs/ngen/commit/144404c90676e5a17947b83982aeabae71833240))

- **windows:** Record passing clang-cl CPU verification ([fbb7f1b](https://github.com/kyaulabs/ngen/commit/fbb7f1b07520e2f411fe2184adef475d7934fc8f))


#### 🧪 Tests

- **render:** Verify translucent scenes and process gpu memory ([b2fa7da](https://github.com/kyaulabs/ngen/commit/b2fa7da44d569cba2aec8221eac04b6df840b20a))

- **windows:** Verify native sampled process gpu memory ([25cf89f](https://github.com/kyaulabs/ngen/commit/25cf89ff8a4db4454b7adff895f2e6a5f494260a))

- **render:** Diagnose native flow wrap failures ([b6ea825](https://github.com/kyaulabs/ngen/commit/b6ea825e0b9037a43ea53f26130ee2da9d6aa0c7))
