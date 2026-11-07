# First Windows combined-scene build failure

Owner archive `/home/kyau/evidence.zip`, SHA256 `9f64be4c03fbcb730b18466fa9b2970474cf29071242408f5b4098b7bf41ce92`, records clean signed source `64faca6be6e2db1eb4392b5bc796d75ca2538f6d`. Bounded ZIP metadata/path/type checks and a redacted log secret scan passed before selected non-transcript inspection. The account-bearing session transcript is not retained.

MSVC 14.44.35207 Debug stopped at compilation, before CTest or GPU execution. `/W4 /WX` correctly promoted C4324 alignment-padding warnings in `shader::CullConstants` and `PostDraw` to C2220 failures. Dependency/configuration and offline shader generation completed; that does not establish shader/device acceptance.

The correction names the existing reserved bytes instead of suppressing diagnostics: CullConstants retains size 112/count offset 96; PostDraw retains size 64/source offset 24/constants offset 48. Post recording uses designated members so explicit gaps do not reorder call arguments. Static ABI assertions and existing CPU/reflection checks protect those layouts. No shader interface, warning/sanitizer policy, SDK or driver change.

Four Linux graphs pass 9/9 and actual shader/resource/post/WSI regression probes with the correction; 21 actual Clang consumer analyses pass. **Corrected native MSVC/clang-cl execution is still pending the owner rerun.** This archive proves only the original failed MSVC build, not rendering or M2 acceptance. Selected diagnostics below preserve the failure without account paths. The correction also blocks consolidation PR #5 until incorporated/verified there; unfinished environment work remains separate.
