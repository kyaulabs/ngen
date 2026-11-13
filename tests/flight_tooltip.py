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
set breakpoint pending on
set $motion = (SDL_Event*)0
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
break exit
commands 2
 silent
 if $motion != 0
  call (void)free($motion)
  set $motion = (SDL_Event*)0
 end
 continue
end
run
quit
"""


@unittest.skipUnless(os.getenv("NGEN_FLIGHT_DEMO") and os.getenv("NGEN_FLIGHT_PACKAGE") and shutil.which("gdb"),
                     "Set native Debug NGEN_FLIGHT_DEMO/PACKAGE with GDB installed")
class Tooltip(unittest.TestCase):
    def replay(self, script_text):
        with tempfile.TemporaryDirectory(prefix="ngen-tooltip-") as temporary:
            script = Path(temporary) / "hover.gdb"
            script.write_text(script_text)
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
        return result

    def test_force_plot_hover_does_not_allocate_after_warmup(self):
        self.replay(SCRIPT)

    def test_force_plot_hover_with_live_performance_overlay_stays_allocation_free(self):
        key = """  set $motion->type = 768
  set $motion->key.windowID = $id
  set $motion->key.scancode = 69
  set $motion->key.down = 1
  set $motion->key.repeat = 0
  call (bool)SDL_PushEvent($motion)
  call (void*)memset($motion, 0, sizeof(SDL_Event))
"""
        script = SCRIPT.replace("  call (void)SDL_free($windows)\n",
                                key + "  call (void)SDL_free($windows)\n")
        result = self.replay(script)
        self.assertIn("performance_visible=1 performance_toggles=1", result.stdout)
        self.assertIn("system_ram_available=1", result.stdout)


if __name__ == "__main__":
    unittest.main()
