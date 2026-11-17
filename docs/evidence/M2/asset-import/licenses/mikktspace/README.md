# Defined MikkTSpace seed rotation

`mikktspace/cci.20200325@ngen/defined-rotate` is an offline asset-tool dependency, not a runtime dependency. It retains upstream MikkTSpace commit `3e895b49d05ea07e4c2133156cfa94369e19e409` and archive SHA-256 `aeba65ddee85a679133d510d71d00b108f603752f90b15b9ecf7777033f6e351`.

The recipe and CMake wrapper derive from Conan Center's `mikktspace/cci.20200325#b085ba568f8ef6b19638d58d92b0a346`. `LICENSE.recipe` retains its MIT notice; `LICENSE.mikktspace` retains the source's Zlib notice. The package includes both notices and this provenance. NGEN's license is unchanged.

Both reference quicksort implementations rotate an unsigned 32-bit seed. When the rotation count is zero, the original right shift uses a count of 32, which is undefined in C. The local patch masks that right-shift count to 0–31. Zero rotation becomes the identity; other rotations are unchanged. The two added comments identify the altered source. No tangent formula, comparison, random seed, compiler warning or sanitizer is disabled or replaced.

The unpatched, instrumented dependency reproduces `shift exponent 32 is too large for 32-bit type 'unsigned int'` through the public GLB importer, using both a synthetic 32×32 indexed grid and the unchanged frigate. The synthetic regression is retained in `tests/asset_import.cpp`; it requires correct tangent frames and preserved geometry, not a private sorting implementation.

Export with `conan export recipes/mikktspace --version=cci.20200325 --user=ngen --channel=defined-rotate`, then use `conan-assets.lock`. This namespaced recipe does not modify the stock recipe or its cached sources. Windows execution and visual normal-map acceptance remain separate verification gates; this is not an upstream-acceptance claim.
