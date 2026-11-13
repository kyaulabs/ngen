"""Opt-in rendered HUD/brand contracts through owned native demo clients."""
import os
import re
import shutil
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest


HOVER_SCRIPT = """set pagination off
set confirm off
set debuginfod enabled off
set $frames = 0
set $motion = (SDL_Event*)0
set $before = 0
set $after = 0
set $overlap = 0
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
  set $motion->key.scancode = 69
  set $motion->key.down = 1
  call (bool)SDL_PushEvent($motion)
  set $motion->type = 769
  set $motion->key.down = 0
  call (bool)SDL_PushEvent($motion)
 end
 continue
end
break ImGui::TextV
commands 2
 silent
 if fmt[0] == 'A' && fmt[1] == 's'
  set $position = ImGui::GetWindowPos()
  set $x = $position.x + 120
  set $y = $position.y + 65
  call (void*)memset($motion, 0, sizeof(SDL_Event))
  set $motion->type = 1024
  set $motion->motion.windowID = $id
  set $motion->motion.x = $x
  set $motion->motion.y = $y
  call (bool)SDL_PushEvent($motion)
  if $frames > 85
   set $panels = ngen::demo::hud_layout(1280.0f, 720.0f, false, true)
   if $x > $panels.performance.x && $x < $panels.performance.x + $panels.performance.width && $y > $panels.performance.y && $y < $panels.performance.y + $panels.performance.height
    set $overlap = 1
   end
  end
 end
 continue
end
break ImGui::SmallButton
commands 3
 silent
 if label[0] == 'A' && label[1] == 'p'
  if $frames > 20 && $frames < 80
   set $before = $before + 1
  end
  if $frames > 85
   set $after = $after + 1
  end
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
printf "target_before=%d target_after=%d target_under_performance=%d\\n", $before, $after, $overlap
quit
"""


@unittest.skipUnless(os.getenv("NGEN_FLIGHT_DEMO") and os.getenv("NGEN_FLIGHT_PACKAGE"),
                     "Set native NGEN_FLIGHT_DEMO/PACKAGE")
class FlightUi(unittest.TestCase):
    @unittest.skipUnless(shutil.which("gdb"), "Native Debug SDL-event fixture needs GDB")
    def test_flight_data_opens_on_f2_without_warm_allocations(self):
        script = """set pagination off
set confirm off
set debuginfod enabled off
set $frames = 0
set $event = (SDL_Event*)0
break ngen::tools::Gui::begin
commands 1
 silent
 if $event == 0
  set $event = (SDL_Event*)malloc(sizeof(SDL_Event))
  call (void*)memset($event, 0, sizeof(SDL_Event))
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
  set $event->type = 768
  set $event->key.windowID = $id
  set $event->key.scancode = 59
  set $event->key.down = 1
  call (bool)SDL_PushEvent($event)
  set $event->type = 769
  set $event->key.down = 0
  call (bool)SDL_PushEvent($event)
 end
 continue
end
break exit
commands 2
 silent
 if $event != 0
  call (void)free($event)
 end
 continue
end
run
quit
"""
        with tempfile.TemporaryDirectory(prefix="ngen-ui-data-") as directory:
            commands = Path(directory) / "events.gdb"
            commands.write_text(script)
            result = subprocess.run(
                [shutil.which("gdb"), "--batch", "-x", str(commands), "--args",
                 os.environ["NGEN_FLIGHT_DEMO"], os.environ["NGEN_FLIGHT_PACKAGE"],
                 "--replay", "--mute", "--frames=120", "--warmup=60"],
                capture_output=True, text=True, timeout=180)
        self.assertEqual(result.returncode, 0, result.stdout[-3000:] + result.stderr[-3000:])
        self.assertIn("hud_controls_visible=0 hud_data_visible=1", result.stdout)
        count = re.search(r"hud_data_drawn_frames=(\d+)", result.stdout)
        self.assertIsNotNone(count, result.stdout[-3000:])
        self.assertGreater(int(count.group(1)), 0)
        self.assertIn("allocation_probe=1", result.stdout)
        self.assertIn("cpp_new_calls=0 imgui_calls=0", result.stdout)

    def target_hover(self, performance):
        if not shutil.which("gdb"):
            self.skipTest("GDB is required for owned-window input")
        with tempfile.TemporaryDirectory(prefix="ngen-target-ui-") as temporary:
            script = Path(temporary) / "target.gdb"
            script.write_text(HOVER_SCRIPT if performance else HOVER_SCRIPT.replace("$frames == 80", "$frames == 9999"))
            result = subprocess.run(
                [shutil.which("gdb"), "--batch", "-x", str(script), "--args",
                 os.environ["NGEN_FLIGHT_DEMO"], os.environ["NGEN_FLIGHT_PACKAGE"],
                 "--frames=120", "--warmup=60", "--mute"],
                capture_output=True, text=True, timeout=180)
        self.assertEqual(result.returncode, 0, result.stdout[-4000:] + result.stderr[-4000:])
        match = re.search(r"target_before=(\d+) target_after=(\d+) target_under_performance=(\d+)", result.stdout)
        self.assertIsNotNone(match, result.stdout[-4000:])
        return tuple(map(int, match.groups()))

    def test_target_actions_reveal_on_actual_label_hover(self):
        before, after, _ = self.target_hover(False)
        self.assertGreater(before, 0, "World-anchored actions never appeared at the actual label position")
        self.assertGreater(after, 0)

    def test_performance_panel_does_not_expose_invisible_target_buttons(self):
        before, after, overlap = self.target_hover(True)
        self.assertGreater(before, 0, "The fixture must first reveal the actual target buttons")
        self.assertEqual(overlap, 1, "The owned pointer must actually overlap target and F12 bounds")
        self.assertEqual(after, 0, "Opaque diagnostics must not mask still-clickable target buttons")

    def test_small_admitted_viewport_keeps_controls_and_performance_bounded(self):
        result = subprocess.run(
            [os.environ["NGEN_FLIGHT_DEMO"], os.environ["NGEN_FLIGHT_PACKAGE"],
             "--walkthrough", "--frames=30", "--warmup=2", "--width=320", "--height=180",
             "--validation", "--mute"], capture_output=True, text=True, timeout=180)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stderr, "")
        self.assertIn("gpu_timing_rows=30", result.stdout)
        self.assertIn("performance_visible=1 performance_toggles=1", result.stdout)
        self.assertIn("hud_controls_drawn_frames=10", result.stdout)

    def test_header_renders_supplied_cyan_and_violet_identity(self):
        with tempfile.TemporaryDirectory(prefix="ngen-ui-") as directory:
            capture = Path(directory) / "brand.bmp"
            result = subprocess.run(
                [os.environ["NGEN_FLIGHT_DEMO"], os.environ["NGEN_FLIGHT_PACKAGE"],
                 "--replay", "--frames=3", "--warmup=2", "--mute", f"--capture={capture}"],
                capture_output=True, text=True, timeout=180)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(result.stderr, "")
            data = capture.read_bytes()
        width, height = struct.unpack_from("<ii", data, 18)
        offset = struct.unpack_from("<I", data, 10)[0]
        pixels = [data[at:at + 3] for y in range(12, 76) for x in range(16, 210)
                  for at in [offset + ((height - 1 - y) * width + x) * 4]]
        violet = sum(r > 120 and b > 180 and b > g + 25 for b, g, r in pixels)
        cyan = sum(b > 160 and g > 140 and g > r + 35 for b, g, r in pixels)
        self.assertGreater(violet, 40, "Approved violet wake artwork is missing from the header")
        self.assertGreater(cyan, 80, "Approved cyan mark is missing from the header")

    def test_default_flight_view_does_not_paint_the_old_telemetry_slab(self):
        with tempfile.TemporaryDirectory(prefix="ngen-immersive-") as directory:
            capture = Path(directory) / "immersive.bmp"
            result = subprocess.run(
                [os.environ["NGEN_FLIGHT_DEMO"], os.environ["NGEN_FLIGHT_PACKAGE"],
                 "--replay", "--frames=3", "--warmup=2", "--mute", f"--capture={capture}"],
                capture_output=True, text=True, timeout=180)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(result.stderr, "")
            data = capture.read_bytes()
        width, height = struct.unpack_from("<ii", data, 18)
        offset = struct.unpack_from("<I", data, 10)[0]
        panel_pixels = 0
        for y in range(80, 600):
            for x in range(20, 260):
                at = offset + ((height - 1 - y) * width + x) * 4
                b, g, r = data[at:at + 3]
                panel_pixels += abs(r - 14) <= 2 and abs(g - 34) <= 2 and abs(b - 44) <= 2
        self.assertLess(panel_pixels, 1000, "Persistent telemetry panel still obscures the scene")
        self.assertIn("performance_visible=0", result.stdout)


if __name__ == "__main__":
    unittest.main()
