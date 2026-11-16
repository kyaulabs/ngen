"""Opt-in Debug tooltip regression using public ImGui positions and owned SDL events."""
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
set $frames = 0
set $hovered = 0
break ngen::tools::Gui::begin
commands 1
 silent
 if $motion == 0
  set $motion = (SDL_Event*)malloc(sizeof(SDL_Event))
  set $count = (int*)malloc(4)
  set $windows = (SDL_Window**)SDL_GetWindows($count)
  if *$count != 1
   quit 2
  end
  set $id = (unsigned int)SDL_GetWindowID($windows[0])
  call (void)SDL_free($windows)
  call (void)free($count)
 end
 set $frames = $frames + 1
 if $frames == 80
  call (void*)memset($motion, 0, sizeof(SDL_Event))
  set $motion->type = 768
  set $motion->key.windowID = $id
  set $motion->key.scancode = 59
  set $motion->key.down = 1
  call (bool)SDL_PushEvent($motion)
  set $motion->type = 769
  set $motion->key.down = 0
  call (bool)SDL_PushEvent($motion)
 end
 continue
end
break ImGui::PlotLines
commands 2
 silent
 if $frames > 80 && label[0] == '#' && label[1] == '#' && label[2] == 'f'
  set $position = ImGui::GetCursorScreenPos()
  call (void*)memset($motion, 0, sizeof(SDL_Event))
  set $motion->type = 1024
  set $motion->motion.windowID = $id
  set $motion->motion.x = $position.x + graph_size.x * 0.5f
  set $motion->motion.y = $position.y + graph_size.y * 0.5f
  call (bool)SDL_PushEvent($motion)
 end
 continue
end
break ImGui::SetTooltipV
commands 3
 silent
 if $frames > 100 && $hovered == 0
  printf "ngen_force_plot_tooltip=1\\n"
  set $hovered = 1
 end
 continue
end
break exit
commands 4
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
                capture_output=True, text=True, timeout=240)
        self.assertEqual(result.returncode, 0, result.stdout[-4000:] + result.stderr[-4000:])
        summary = re.search(r"flight_frames=(\d+).*cpp_new_calls=(\d+) imgui_calls=(\d+) gpu_timing_rows=(\d+)", result.stdout)
        self.assertIsNotNone(summary, result.stdout[-4000:])
        self.assertEqual(tuple(map(int, summary.groups())), (2400, 0, 0, 2400), result.stdout[-4000:])
        self.assertIn("flight_hdr_finite=1", result.stdout)
        self.assertIn("allocation_probe=1", result.stdout)
        self.assertIn("hud_data_visible=1", result.stdout)
        self.assertIn("ngen_force_plot_tooltip=1", result.stdout,
                      "Moving over an old coordinate is not proof that a plot tooltip was shown")
        self.assertNotIn("Flight demo:", result.stderr)
        return result

    def test_force_plot_hover_does_not_allocate_after_warmup(self):
        self.replay(SCRIPT)

    def test_batched_mouse_motion_during_plot_hover_stays_allocation_free(self):
        burst = """ if $frames == 90
  call (void*)memset($motion, 0, sizeof(SDL_Event))
  set $motion->type = 1024
  set $motion->motion.windowID = $id
  set $burst = 0
  while $burst < 32
   set $motion->motion.x = 100 + $burst
   set $motion->motion.y = 100
   call (bool)SDL_PushEvent($motion)
   set $burst = $burst + 1
  end
 end
"""
        self.replay(SCRIPT.replace(" if $frames == 80\n", burst + " if $frames == 80\n"))

    def test_mouse_motion_on_each_side_of_a_button_keeps_its_order(self):
        events = """ if $frames == 90
  call (void*)memset($motion, 0, sizeof(SDL_Event))
  set $motion->type = 1024
  set $motion->motion.windowID = $id
  set $motion->motion.x = 300
  set $motion->motion.y = 300
  call (bool)SDL_PushEvent($motion)
  set $motion->type = 1025
  set $motion->button.windowID = $id
  set $motion->button.button = 1
  set $motion->button.down = 1
  set $motion->button.x = 300
  set $motion->button.y = 300
  call (bool)SDL_PushEvent($motion)
  call (void*)memset($motion, 0, sizeof(SDL_Event))
  set $motion->type = 1024
  set $motion->motion.windowID = $id
  set $motion->motion.x = 900
  set $motion->motion.y = 500
  call (bool)SDL_PushEvent($motion)
  set $motion->type = 1026
  set $motion->button.windowID = $id
  set $motion->button.button = 1
  set $motion->button.down = 0
  set $motion->button.x = 900
  set $motion->button.y = 500
  call (bool)SDL_PushEvent($motion)
 end
"""
        observe = """break ngen::tools::Gui::end
commands 5
 silent
 if $frames == 91 || $frames == 92
  set $io = &ImGui::GetIO()
  printf "ngen_ordered_mouse=%.0f,%.0f,%d\\n", $io->MousePos.x, $io->MousePos.y, $io->MouseDown[0]
 end
 continue
end
"""
        script = SCRIPT.replace(" if $frames == 80\n", events + " if $frames == 80\n")
        result = self.replay(script.replace("run\nquit\n", observe + "run\nquit\n"))
        positions = re.findall(r"ngen_ordered_mouse=([\d,]+)", result.stdout)
        self.assertEqual(positions, ["300,300,1", "900,500,0"])

    def test_returning_fractional_motion_preserves_mouse_source_transition(self):
        events = """ if $frames == 85 || $frames == 90
  call (void*)memset($motion, 0, sizeof(SDL_Event))
  set $motion->type = 1024
  set $motion->motion.windowID = $id
  set $motion->motion.which = $frames == 85 ? (SDL_MouseID)-1 : (SDL_MouseID)0
  set $motion->motion.x = 300.25
  set $motion->motion.y = 300.25
  call (bool)SDL_PushEvent($motion)
  if $frames == 90
   set $motion->motion.x = 300.5
   call (bool)SDL_PushEvent($motion)
   set $motion->motion.x = 301.25
   call (bool)SDL_PushEvent($motion)
   set $motion->motion.x = 300.75
   call (bool)SDL_PushEvent($motion)
  end
 end
"""
        observe = """break ngen::tools::Gui::end
commands 5
 silent
 if $frames == 86 || $frames == 91
  set $io = &ImGui::GetIO()
  printf "ngen_mouse_source=%d,%.0f,%.0f,%d\\n", $frames, $io->MousePos.x, $io->MousePos.y, $io->MouseSource
 end
 continue
end
"""
        script = SCRIPT.replace(" if $frames == 80\n", events + " if $frames == 80\n")
        script = script.replace("if $frames > 80 &&", "if $frames > 110 &&")
        result = self.replay(script.replace("run\nquit\n", observe + "run\nquit\n"))
        sources = re.findall(r"ngen_mouse_source=([\d,]+)", result.stdout)
        self.assertEqual(sources, ["86,300,300,1", "91,300,300,0"])

    def test_force_plot_hover_with_live_performance_overlay_stays_allocation_free(self):
        key = """  call (void*)memset($motion, 0, sizeof(SDL_Event))
  set $motion->type = 768
  set $motion->key.windowID = $id
  set $motion->key.scancode = 69
  set $motion->key.down = 1
  call (bool)SDL_PushEvent($motion)
  set $motion->type = 769
  set $motion->key.down = 0
  call (bool)SDL_PushEvent($motion)
"""
        script = SCRIPT.replace("  call (void)SDL_free($windows)\n",
                                key + "  call (void)SDL_free($windows)\n")
        result = self.replay(script)
        self.assertIn("performance_visible=1 performance_toggles=1", result.stdout)
        self.assertIn("system_ram_available=1", result.stdout)


if __name__ == "__main__":
    unittest.main()
