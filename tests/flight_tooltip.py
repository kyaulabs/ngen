"""Opt-in native Debug regression; GDB injects only public SDL pointer events."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

SCRIPT = """set pagination off
set confirm off
set debuginfod enabled off
set $motion = (SDL_Event*)0
set $reads = 0
break ngen::tools::Gui::begin
commands 1
 silent
 if $motion == 0
  set $motion = (SDL_Event*)malloc(sizeof(SDL_Event))
  call (void*)memset($motion, 0, sizeof(SDL_Event))
  set $count = (int*)malloc(4)
  set $windows = (SDL_Window**)SDL_GetWindows($count)
  if *$count != 1
   quit 2
  end
  set $id = (unsigned int)SDL_GetWindowID($windows[0])
  call (void)SDL_free($windows)
  call (void)free($count)
 end
 set $motion->type = 1024
 set $motion->motion.windowID = $id
 set $motion->motion.x = 160.0f
 set $motion->motion.y = 450.0f
 call (bool)SDL_PushEvent($motion)
 continue
end
break ngen::tools::Gui::allocation_count
commands 2
 silent
 set $reads = $reads + 1
 if $reads == 2
  call (void)free($motion)
 end
 continue
end
run
quit
"""


@unittest.skipUnless(os.getenv("NGEN_FLIGHT_DEMO") and os.getenv("NGEN_FLIGHT_PACKAGE") and shutil.which("gdb"),
                     "Set native Debug NGEN_FLIGHT_DEMO/PACKAGE with GDB installed")
class Tooltip(unittest.TestCase):
    def test_force_plot_hover_does_not_allocate_after_warmup(self):
        with tempfile.TemporaryDirectory(prefix="ngen-tooltip-") as temporary:
            script = Path(temporary) / "hover.gdb"
            script.write_text(SCRIPT)
            result = subprocess.run(
                [shutil.which("gdb"), "--batch", "-x", str(script), "--args",
                 os.environ["NGEN_FLIGHT_DEMO"], os.environ["NGEN_FLIGHT_PACKAGE"],
                 "--replay", "--mute", "--frames=2400", "--warmup=60", "--width=1920", "--height=1080"],
                capture_output=True, text=True, timeout=180)
        self.assertEqual(result.returncode, 0, result.stdout[-4000:] + result.stderr[-4000:])
        summary = re.search(r"flight_frames=(\d+).*cpp_new_calls=(\d+) imgui_calls=(\d+) gpu_timing_rows=(\d+)", result.stdout)
        self.assertIsNotNone(summary, result.stdout[-4000:])
        self.assertEqual(tuple(map(int, summary.groups())), (2400, 0, 0, 2400), result.stdout[-4000:])
        self.assertIn("flight_hdr_finite=1", result.stdout)
        self.assertIn("allocation_probe=1", result.stdout)
        self.assertNotIn("Flight demo:", result.stderr)


if __name__ == "__main__":
    unittest.main()
