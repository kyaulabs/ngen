# M5-F saved Surveyors campaign

`quest_survey` is a playable, headless offline quest. It starts in protected space with a lesson on lawful self-defense, police consequences for a first strike, and the lack of NPC protection on the lawless route. Accepting the lesson unlocks one first-party Lua choice: join the Surveyors' prospecting career. C++ authorizes each transition and owns credits, ore, vessel identity and replay. No Lua library or native callback can change game state.

A sortie costs 2 credits. The scripted belt yields up to 3 units from a finite 12-unit deposit. The rival encounter resolves through the M5-C combat fixture; both victory and loss are playable routes. Loss transfers ore to one wreck. Free starter recovery grants a new vessel with no ore, credits or sellable equipment. Salvage transfers only the existing wreck ore. The M5-E berth captures the same vessel and gates the M5-A sale at 5 credits per unit. The first completed survey earns 4 extra credits from the finite station account; subsequent surveys do not pay the bonus. Across four sorties, both victory and recovery routes conserve 12 ore and 130 credits. There is no deposit reset, payout for self-destruction, or M8 police simulation.

## Run and save

Build on Linux with `pkg-config` and the exact Lua 5.5.1 C library as described in the [rule boundary](quest-rule-boundary.md). Then:

```sh
cmake --build build/quest-release --target quest_survey
workspace=$(mktemp -d)
app=build/quest-release/quest_survey
"$app" "$workspace" status
"$app" "$workspace" step 0 learn
"$app" "$workspace" step 1 join
"$app" "$workspace" step 2 venture
"$app" "$workspace" step 3 mine
"$app" "$workspace" step 4 lose
"$app" "$workspace" step 5 recover
"$app" "$workspace" step 6 salvage
"$app" "$workspace" step 7 sell
```

`status` shows the next action and current revision. At the encounter choose `defeat` instead of `lose` to skip recovery/salvage. Reopening the same owner-only directory reconstructs the same state; retry a previous accepted revision and action to receive its original receipt. A changed action at that revision conflicts. Future or illegal commands do not advance the save.

The Linux owner locks the directory, then publishes one checksummed, versioned sequence of at most 64 admitted actions. Both save-format and campaign versions must match; replay reconstructs state and receipts without serializing VM internals. The checksum detects accidental corruption, not deliberate tampering. A new save replaces the old one only after the pending file is synced. Load ignores unpublished staging. A failed directory sync after rename returns `save_uncertain`: retry the same revision and action, not a new one. Unknown versions and invalid sequences fail closed; this fixture does not migrate old rules, guarantee power-loss behavior on all filesystems, or admit concurrent uncooperative editors.

## Verification and limits

Clang 23.1.5/Linux x86-64, CMake 4.5.1, libstdc++, locked Conan inputs and the SHA-256-verified Lua 5.5.1 C archive: all 13 opt-in Release CTests passed. The four quest CTests passed with both Lua and the adapter instrumented under ASan/UBSan/LSan; a non-PIE link avoids this host's hardened shadow-map overlap. Run the focused suite with `ctest --test-dir build/quest-release -R '^(quest_script|survey_quest|survey_save|survey_cli)$' --output-on-failure --no-tests=error`. Format, clang-tidy and workflow lint passed. Review caught an unreadable save under a restrictive umask; the permission/reopen regression failed before the fix. The test suite also checks historical replies, staged-but-unpublished bytes, failed staging, truncated/nonregular saves, and a fresh CLI process at each step of a loss and victory run.

This is a text-driven integration of the game fixtures. It does not attach the quest to live flight/render streaming, measure a one-hour workload, record an owner playthrough, ratify ADR 0013, or close M5's broader acceptance gate.
