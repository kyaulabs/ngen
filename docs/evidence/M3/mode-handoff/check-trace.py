"""Check the real 330-frame SDL walkthrough's manual/tactical handoff contract."""
import csv
from pathlib import Path
import sys

rows = list(csv.DictReader(Path(sys.argv[1]).open(newline="")))
assert len(rows) == 330
velocity = tuple(rows[179]["v" + axis + "_mps"] for axis in "xyz")
for row in rows[180:210]:
    assert row["mode"] == "0"
    assert row["nav_goal"] == "0", "TAB must not reactivate the previous autopilot goal"
    assert all(abs(float(row["f" + axis + "_N"])) < 1e-6 for axis in "xyz")
    assert tuple(row["v" + axis + "_mps"] for axis in "xyz") == velocity
assert rows[270]["nav_goal"] == "2", "explicit reset must still restore the authored default orbit"
changes = [i for i in range(1, len(rows)) if rows[i]["entity_generation"] != rows[i - 1]["entity_generation"]]
assert changes[0] == 270 and len(changes) == 2
assert rows[changes[1]]["nav_goal"] == "2", "GUI reset must still restore its fresh orbit"
print("SDL TAB return coasts without stale navigation/force/velocity reset; both explicit resets restore a fresh orbit")
