# M5-A offline inventory and currency transfer

The local game session owns `economy::LocalMarket`. It runs on the session's owner thread; generic simulation, the installed SDK, and workers do not own or mutate its holdings. It trades one fungible resource between a player and station. Trusted game rules supply the buyer, quantity, and unit price; this is not player authorization, docking admission, quoting, minting, or a market order book. No graphics, database, wallet, or chain dependency is needed.

Credits are integer subunits of closed-loop `GameCredit`; quantities are integer units. The market checks the line total and both receiving balances before applying either debit or credit. A rejected transaction does not change holdings. A buy and a sell in the fixed fixture preserve the starting totals of 120 credit subunits and 13 units of stock.

A nonzero operation ID names the first admitted request and terminal outcome, including a rejection. The full request (buyer, quantity, unit price) is compared on retry. A match returns the original outcome, including the historical post-trade snapshot, without reapplying it. A mismatch returns a conflict; a new purchase after a rejection needs a fresh ID. Zero IDs are rejected without using a slot. The journal holds 64 operations and does not evict: after saturation, new IDs fail closed and known IDs remain replayable. This is an in-memory fixture bound, not a production capacity claim or a save/restart guarantee.

```sh
cmake --build --preset release
ctest --preset release --output-on-failure --no-tests=error
cmake --build --preset asan
ctest --preset asan --output-on-failure --no-tests=error
```

Linux x86-64, Clang 23.1.5 with libstdc++, CMake 4.5.1, Ninja and the existing Conan graph: Release and strict ASan/UBSan/LSan each passed all five headless CTests. The economy executable ran six cases with 104 assertions covering both directions, full-request conflicts, stable rejected retries, insufficient stock/credits, invalid input, overflow and saturation. The first buy assertion failed before settlement was implemented; the replay assertion failed before journal lookup was implemented. Scoped format and clang-tidy passed; an independent read-only source review found no blocking defects. The fixture executable links only standard runtime libraries. This does not accept the full M5 offline loop or promise durable receipts.
