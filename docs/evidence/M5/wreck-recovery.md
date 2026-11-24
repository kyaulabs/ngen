# M5-D wreck ownership and starter recovery

The offline `lifecycle::Session` owns one pilot's live ship, wrecks and recovery rights. Trusted game logic records a loss only after the named generation is destroyed. One lock publishes the loss: the live ship disappears and exactly one wreck takes all surviving cargo. The fixture treats all of its one fungible resource as surviving. The combat test destroys the ship through M5-C, then explicitly records the loss; this is not automatic integration with live flight.

Each loss grants one successor generation. Recovery restores a live starter with an empty hold and grants no credits, detachable or sellable equipment, or bonus salvage. Salvage transfers the wreck's existing cargo in full to the named live generation and empties the wreck. If death wins the race, the old wreck keeps the stock; if salvage wins, the dying receiver passes that stock to its own wreck. A precondition rejection consumes nothing. An exact committed retry returns the historical result without changing current holdings; a spent wreck cannot be claimed for a different receiver.

The 16-record session does not evict losses or receipts. Every issued ship can have its loss recorded; after loss 16 a new recovery rejects before issuing an untracked ship. Earlier receipts remain replayable. Generations are local to the session; `ship()` and `wreck()` are separately consistent reads, not a combined snapshot. There is no save/restart guarantee, sale interface, multiplayer or multi-pilot salvage claim. This is not the integrated M5 loop.

```sh
cmake --build --preset release && ctest --preset release --output-on-failure --no-tests=error
cmake --build --preset asan && ctest --preset asan --output-on-failure --no-tests=error
cmake --build --preset tsan && ctest --preset tsan --output-on-failure --no-tests=error
ctest --preset tsan -R '^lifecycle$' --repeat until-fail:20 --output-on-failure --no-tests=error
```

Linux x86-64, Clang 23.1.5/libstdc++, CMake 4.5.1, Ninja 1.13.2 and the existing Conan graph: all eight headless CTests passed in Release, strict ASan/UBSan/LSan and TSan. The lifecycle target passed five cases with 112 assertions in the recorded Release run and 20 repeated TSan runs. The first loss, recovery and salvage assertions each failed before their implementation. Cases cover multiple empty recovery cycles, duplicate commands, both allowed race outcomes, stock at `UINT64_MAX`, and fixture exhaustion. Scoped clang-format and clang-tidy passed. Independent source review found no state-machine defect within the fixture contract; the barrier starts contenders together but does not force both schedules.
