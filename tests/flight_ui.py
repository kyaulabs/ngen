"""Opt-in rendered HUD/brand contracts through owned native demo clients."""
import hashlib
import json
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

    def target_context(self, gesture="tap", dismiss=False, choose_focus=False, performance=False, edge=False, reopen=False):
        if not shutil.which("gdb"):
            self.skipTest("GDB is required for owned-window context input")
        release = 110 if gesture == "hold" else 82
        free_look = gesture in ("free_look", "right_free_look", "left_alt_overlap", "right_alt_overlap")
        direct_look = free_look and gesture != "free_look"
        right_alt = gesture in ("right_free_look", "right_alt_overlap")
        overlap = gesture.endswith("alt_overlap")
        drag = int(gesture == "drag" or direct_look)
        events = f""" if $frames == 65 && {int(direct_look)}
  call (void*)memset($motion, 0, sizeof(SDL_Event))
  set $motion->type = 768
  set $motion->key.windowID = $id
  set $motion->key.scancode = 43
  set $motion->key.down = 1
  call (bool)SDL_PushEvent($motion)
  set $motion->type = 769
  set $motion->key.down = 0
  call (bool)SDL_PushEvent($motion)
 end
 if ($frames == 70 || $frames == 120) && {int(free_look)}
  call (void*)memset($motion, 0, sizeof(SDL_Event))
  set $motion->type = $frames == 70 ? 768 : 769
  set $motion->key.windowID = $id
  set $motion->key.scancode = $frames == 70 ? {230 if right_alt else 226} : {226 if right_alt and overlap else 230 if overlap or right_alt else 226}
  set $motion->key.mod = $frames == 70 ? {512 if right_alt else 256} : 0
  set $motion->key.down = $frames == 70
  call (bool)SDL_PushEvent($motion)
 end
 if ($frames == 74 || $frames == 75) && {int(overlap)}
  call (void*)memset($motion, 0, sizeof(SDL_Event))
  set $motion->type = $frames == 74 ? 768 : 769
  set $motion->key.windowID = $id
  set $motion->key.scancode = $frames == 74 ? {226 if right_alt else 230} : {230 if right_alt else 226}
  set $motion->key.mod = $frames == 74 ? 768 : {256 if right_alt else 512}
  set $motion->key.down = $frames == 74
  call (bool)SDL_PushEvent($motion)
 end
 if $frames == 80 || $frames == {release} || ({int(reopen)} && ($frames == 140 || $frames == 142))
  call (void*)memset($motion, 0, sizeof(SDL_Event))
  set $motion->type = ($frames == 80 || $frames == 140) ? 1025 : 1026
  set $motion->button.windowID = $id
  set $motion->button.button = 3
  set $motion->button.down = $frames == 80 || $frames == 140
  set $motion->button.x = $x
  set $motion->button.y = $y
  call (bool)SDL_PushEvent($motion)
 end
 if $frames == 81 && {drag}
  call (void*)memset($motion, 0, sizeof(SDL_Event))
  set $motion->type = 1024
  set $motion->motion.windowID = $id
  set $motion->motion.xrel = 30
  call (bool)SDL_PushEvent($motion)
 end
 if ($frames == 98 || $frames == 99) && {int(choose_focus)}
  call (void*)memset($motion, 0, sizeof(SDL_Event))
  set $motion->type = 1024
  set $motion->motion.windowID = $id
  set $motion->motion.x = $x
  set $motion->motion.y = $y
  call (bool)SDL_PushEvent($motion)
  call (void*)memset($motion, 0, sizeof(SDL_Event))
  set $motion->type = $frames == 98 ? 1025 : 1026
  set $motion->button.windowID = $id
  set $motion->button.button = 1
  set $motion->button.down = $frames == 98
  set $motion->button.x = $x
  set $motion->button.y = $y
  call (bool)SDL_PushEvent($motion)
 end
 if $frames == 110 && {int(dismiss or reopen)}
  call (void*)memset($motion, 0, sizeof(SDL_Event))
  set $motion->type = 768
  set $motion->key.windowID = $id
  set $motion->key.scancode = 41
  set $motion->key.down = 1
  call (bool)SDL_PushEvent($motion)
  set $motion->type = 769
  set $motion->key.down = 0
  call (bool)SDL_PushEvent($motion)
 end
 continue
end"""
        script = re.sub(r" if \$frames == 80\n.*?\n end\n continue\nend", lambda _: events,
                        HOVER_SCRIPT, count=1, flags=re.DOTALL)
        script = script.replace("break ImGui::SmallButton", "break ImGui::MenuItem")
        script = script.replace("set $overlap = 0", "set $overlap = 0\nset $orbit = 0\nset $focus = 0\nset $late = 0\nset $camera_drag = 0\nset $focused = 0\nset $relative = 0\nset $warps = 0\nset $second_origin = 0")
        if reopen:
            script = script.replace("  set $x = $position.x + 120\n  set $y = $position.y + 65", """  set $x = $frames < 130 ? $position.x + 120 : 640
  set $y = $frames < 130 ? $position.y + 65 : 420""")
        if edge:
            script = script.replace("$position.x + 120", "1274").replace("$position.y + 65", "714")
            script = script.replace(" if $frames > 85\n  if label[0]", """ if $frames > 85
  set $popup_pos = ImGui::GetWindowPos()
  set $popup_size = ImGui::GetWindowSize()
  if $popup_pos.x < 0 || $popup_pos.y < 0 || $popup_pos.x + $popup_size.x > 1280 || $popup_pos.y + $popup_size.y > 720
   printf "popup_outside_viewport\\n"
   quit 2
  end
  if label[0]""")
        if choose_focus:
            script = script.replace(" if fmt[0] == 'A'",  " if $frames < 90 && fmt[0] == 'A'")
        if performance:
            script = script.replace("  call (void)SDL_free($windows)", """  call (void*)memset($motion, 0, sizeof(SDL_Event))
  set $motion->type = 768
  set $motion->key.windowID = $id
  set $motion->key.scancode = 69
  set $motion->key.down = 1
  call (bool)SDL_PushEvent($motion)
  set $motion->type = 769
  set $motion->key.down = 0
  call (bool)SDL_PushEvent($motion)
  call (void)SDL_free($windows)""")
        script = script.replace(" if label[0] == 'A' && label[1] == 'p'", """ if $frames > 85
  if label[0] == 'O'
   set $orbit = $orbit + 1
  end
  if label[0] == 'F'
   set $focus = $focus + 1
  end
  if $frames > 120
   set $late = $late + 1
  end
 end
 if label[0] == 'A' && label[1] == 'p'""")
        if choose_focus:
            script = script.replace("  if label[0] == 'F'\n", """  if label[0] == 'F' && $frames == 96
   set $position = ImGui::GetCursorScreenPos()
   set $x = $position.x + 12
   set $y = $position.y + 6
  end
  if label[0] == 'F'
""")
        effects = """break ngen::flight::CameraRig::orbit
commands 5
 silent
 if $frames > 79 && (motion.yaw_radians != 0 || motion.pitch_radians != 0)
  set $camera_drag = $camera_drag + 1
 end
 continue
end
break ngen::flight::CameraRig::focus
commands 6
 silent
 if $frames > 79
  set $focused = $focused + 1
 end
 continue
end
break ngen::flight::CameraRig::look
commands
 silent
 if $frames > 79 && (motion.yaw_radians != 0 || motion.pitch_radians != 0)
  set $camera_drag = $camera_drag + 1
 end
 continue
end
break SDL_WarpMouseInWindow
commands
 silent
 if $frames > 79
  set $warps = $warps + 1
 end
 continue
end
"""
        if reopen:
            script = script.replace(" set $frames = $frames + 1", """ set $frames = $frames + 1
 if $frames == 81 || $frames == 141
  set $window = (SDL_Window*)SDL_GetWindowFromID($id)
  if (bool)SDL_GetWindowRelativeMouseMode($window)
   set $relative = $relative + 1
  end
 end""")
            script = script.replace(" if $frames > 85\n  if label[0]",  """ if $frames > 145
  set $popup_pos = ImGui::GetWindowPos()
  if $popup_pos.x == 640 && $popup_pos.y == 420
   set $second_origin = 1
  end
 end
 if $frames > 85
  if label[0]""")
        script = script.replace("run\n", effects + "run\n", 1)
        script = script.removesuffix("quit\n") + 'printf "context_items=%d,%d,%d,%d,%d,%d\\n", $after, $orbit, $focus, $late, $camera_drag, $focused\nprintf "context_cursor=%d,%d,%d\\n", $relative, $warps, $second_origin\nquit\n'
        with tempfile.TemporaryDirectory(prefix="ngen-context-ui-") as directory:
            commands = Path(directory) / "context.gdb"
            commands.write_text(script)
            result = subprocess.run(
                [shutil.which("gdb"), "--batch", "-x", str(commands), "--args",
                 os.environ["NGEN_FLIGHT_DEMO"], os.environ["NGEN_FLIGHT_PACKAGE"],
                 "--frames=150", "--warmup=60", "--mute"],
                capture_output=True, text=True, timeout=180)
        self.assertEqual(result.returncode, 0, result.stdout[-4000:] + result.stderr[-4000:])
        self.assertIn("allocation_probe=1", result.stdout)
        self.assertIn("cpp_new_calls=0 imgui_calls=0", result.stdout, result.stdout[-4000:])
        match = re.search(r"context_items=(\d+),(\d+),(\d+),(\d+),(\d+),(\d+)", result.stdout)
        self.assertIsNotNone(match, result.stdout[-4000:])
        if reopen:
            cursor = re.search(r"context_cursor=(\d+),(\d+),(\d+)", result.stdout)
            self.assertIsNotNone(cursor, result.stdout[-4000:])
            self.assertEqual(int(cursor[3]), 1, "The reopened menu must use the new click location\n" + result.stdout[-4000:])
            self.assertEqual(int(cursor[1]), 0, "Menu clicks must not enter cursor-restoring relative mode")
            self.assertEqual(int(cursor[2]), 0, "Menu clicks must not request a cursor warp")
        return tuple(map(int, match.groups()))

    def test_reopened_context_menu_keeps_cursor_at_the_new_click(self):
        approach, orbit, focus, *_ = self.target_context(reopen=True)
        self.assertGreater(approach, 0)
        self.assertGreater(orbit, 0)
        self.assertGreater(focus, 0)

    def test_right_click_opens_selected_target_actions(self):
        approach, orbit, focus, *_ = self.target_context()
        self.assertGreater(approach, 0, "A right-click must open the target menu")
        self.assertGreater(orbit, 0)
        self.assertGreater(focus, 0)

    def test_walkthrough_exercises_context_menu_without_a_debugger(self):
        result = subprocess.run(
            [os.environ["NGEN_FLIGHT_DEMO"], os.environ["NGEN_FLIGHT_PACKAGE"],
             "--walkthrough", "--frames=278", "--warmup=2", "--mute"],
            capture_output=True, text=True, timeout=180)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stderr, "")
        count = re.search(r"hud_context_menu_drawn_frames=(\d+)", result.stdout)
        self.assertIsNotNone(count, result.stdout)
        self.assertGreater(int(count.group(1)), 0, "The native walkthrough must exercise the popup")

    def test_context_menu_fits_near_the_viewport_edge(self):
        approach, orbit, focus, *_ = self.target_context(edge=True)
        self.assertGreater(approach, 0)
        self.assertGreater(orbit, 0)
        self.assertGreater(focus, 0)

    def test_right_drag_and_hold_do_not_open_context_menu(self):
        for gesture in ("drag", "hold"):
            with self.subTest(gesture=gesture):
                approach, orbit, focus, _, camera_drag, _ = self.target_context(gesture)
                self.assertEqual((approach, orbit, focus), (0, 0, 0))
                if gesture == "drag":
                    self.assertGreater(camera_drag, 0, "RMB dragging must still orbit the camera")

    def test_right_alt_drag_in_direct_mode_moves_only_the_camera(self):
        approach, orbit, focus, _, camera_drag, _ = self.target_context("right_free_look")
        self.assertEqual((approach, orbit, focus), (0, 0, 0))
        self.assertGreater(camera_drag, 0, "Right Alt must route direct-mode motion to free look")

    def test_releasing_one_alt_keeps_the_other_alt_in_camera_only_mode(self):
        for gesture in ("left_alt_overlap", "right_alt_overlap"):
            with self.subTest(gesture=gesture):
                approach, orbit, focus, _, camera_drag, _ = self.target_context(gesture)
                self.assertEqual((approach, orbit, focus), (0, 0, 0))
                self.assertGreater(camera_drag, 0, "The remaining Alt must keep free look active")

    def test_free_look_click_does_not_open_context_menu(self):
        approach, orbit, focus, *_ = self.target_context("free_look")
        self.assertEqual((approach, orbit, focus), (0, 0, 0), "Alt+RMB remains camera-only")

    def test_context_menu_dismisses_and_dispatches_focus(self):
        approach, _, _, late, _, _ = self.target_context(dismiss=True)
        self.assertGreater(approach, 0)
        self.assertEqual(late, 0, "Escape must close the context menu")
        approach, _, _, late, _, focused = self.target_context(choose_focus=True)
        self.assertGreater(approach, 0)
        self.assertEqual(focused, 1, "The Focus menu item must invoke the existing camera action")
        self.assertEqual(late, 0, "Choosing an action must close the context menu")

    def test_right_click_over_performance_panel_does_not_open_world_menu(self):
        approach, orbit, focus, *_ = self.target_context(performance=True)
        self.assertEqual((approach, orbit, focus), (0, 0, 0))

    def test_selected_target_hover_does_not_reveal_action_buttons(self):
        for performance in (False, True):
            with self.subTest(performance=performance):
                before, after, _ = self.target_hover(performance)
                self.assertEqual(before, 0, "Selection/hover must not expose target actions")
                self.assertEqual(after, 0, "Target actions belong only in the context menu")

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

    @unittest.skipUnless(shutil.which("gdb"), "Native rendered-text fixture needs GDB")
    def test_rendered_actions_and_telemetry_have_no_hotkey_hints(self):
        with tempfile.TemporaryDirectory(prefix="ngen-ui-text-") as directory:
            output = Path(directory) / "labels.json"
            script = Path(directory) / "text.gdb"
            script.write_text(f"""set pagination off
set confirm off
set debuginfod enabled off
python
import gdb, json
labels = set()
ordinary = set()
frames = 0
in_menu = False
class Menu(gdb.Breakpoint):
 def stop(self):
  global in_menu
  in_menu = True
  return False
class EndMenu(gdb.Breakpoint):
 def stop(self):
  global in_menu
  in_menu = False
  return False
class Frame(gdb.Breakpoint):
 def stop(self):
  global frames
  frames += 1
  return False
class RenderedText(gdb.Breakpoint):
 def stop(self):
  if frames < 9:
   return False
  begin = gdb.parse_and_eval('text_begin')
  end = gdb.parse_and_eval('text_end')
  length = int(end) - int(begin)
  if int(begin) and int(end) and 0 < length <= 1024 and len(labels) < 8192:
   text = begin.string(encoding='utf-8', errors='strict', length=length)
   labels.add(text)
   if not in_menu:
    ordinary.add(text)
  return False
Frame('ngen::tools::Gui::begin', internal=True)
Menu('ImGui::MenuItem', internal=True)
EndMenu('ImGui::EndPopup', internal=True)
RenderedText('ImFont::RenderText', internal=True)
end
run
python
with open({str(output)!r}, 'w') as stream:
 json.dump(dict(all=sorted(labels), ordinary=sorted(ordinary)), stream)
end
quit
""")
            result = subprocess.run(
                [shutil.which("gdb"), "--batch", "-x", str(script), "--args",
                 os.environ["NGEN_FLIGHT_DEMO"], os.environ["NGEN_FLIGHT_PACKAGE"],
                 "--walkthrough", "--frames=330", "--warmup=2", "--mute"],
                capture_output=True, text=True, timeout=180)
            self.assertEqual(result.returncode, 0, result.stdout[-4000:] + result.stderr[-4000:])
            self.assertNotIn("Python Exception", result.stderr)
            observed = json.loads(output.read_text())
            labels, ordinary = observed["all"], observed["ordinary"]
        self.assertLess(len(labels), 8192, "The rendered-text capture must not truncate its observations")
        self.assertIn("hud_data_drawn_frames=10", result.stdout)
        self.assertIn("performance_visible=1 performance_toggles=3", result.stdout)
        hints = re.compile(r"\b(?:F(?:[1-9]|1[0-2])|TAB|SPACE|SHIFT|CTRL|ALT|RMB|LMB|ESC)\b|"
                           r"\[[A-Z]\]|\b(?:Approach|Orbit|Focus) [A-Z]\b|W/S|A/D|Q/E|"
                           r"\b(?:click|arrows|wheel|keyboard)\b", re.IGNORECASE)
        self.assertFalse([label for label in labels if hints.search(label)],
                         "Flight UI still renders shortcut hints or input instructions")
        for action in ("Controls", "Flight data", "Switch view", "Reset", "Approach", "Orbit", "Focus"):
            self.assertIn(action, labels, "Removing binding hints must preserve usable actions")
        self.assertFalse(set(ordinary) & {"Approach", "Orbit", "Focus"},
                         "Target actions must not appear in ordinary controls/data panels")
        for heading in ("CONTROLS", "FLIGHT DATA", "PERFORMANCE"):
            self.assertIn(heading, labels)
        self.assertTrue(any("m/s" in label for label in labels), "Speed telemetry must remain")

    @unittest.skipUnless(shutil.which("gdb"), "Native menu opacity fixture needs GDB")
    def test_flight_panels_follow_brand14_materials_without_translucent_overlap(self):
        with tempfile.TemporaryDirectory(prefix="ngen-menu-alpha-") as directory:
            output = Path(directory) / "alpha.json"
            script = Path(directory) / "alpha.gdb"
            script.write_text(f"""set pagination off
set confirm off
set debuginfod enabled off
python
import gdb, json
surfaces = dict((name, dict(panel=set(), header=set(), border=set())) for name in ('utility', 'performance', 'context'))
text_alphas = set()
under_header = set()
border_inner_edges = set()
origins = dict(utility=(8, 64), performance=(922, 64))
def rgba(value):
 return tuple((int(value) >> shift) & 255 for shift in (0, 8, 16, 24))
def origin(x, y):
 for name, point in origins.items():
  if abs(x - point[0]) < 2 and 0 <= y - point[1] <= 33:
   return name, y - point[1]
 return None
class PanelClip(gdb.Breakpoint):
 def stop(self):
  caller = gdb.newest_frame().older()
  if caller and 'panel_surface' in (caller.name() or ''):
   point = gdb.parse_and_eval('cr_min')
   x, y = float(point['x']), float(point['y'])
   if not any(abs(x - px) < 2 and abs(y - py) < 2 for px, py in (origins['utility'], origins['performance'])):
    origins['context'] = (x, y)
  return False
class Foreground(gdb.Breakpoint):
 def stop(self):
  begin, end = gdb.parse_and_eval('text_begin'), gdb.parse_and_eval('text_end')
  length = int(end) - int(begin)
  if int(begin) and int(end) and 0 < length < 128:
   text = begin.string(encoding='utf-8', errors='strict', length=length)
   if text in ('CONTROLS', 'FLIGHT DATA', 'PERFORMANCE') or text.startswith('SELECTED / ASTEROID'):
    text_alphas.add((int(gdb.parse_and_eval('col')) >> 24) & 255)
  return False
class Background(gdb.Breakpoint):
 def stop(self):
  lo, hi = gdb.parse_and_eval('p_min'), gdb.parse_and_eval('p_max')
  x, y = float(lo['x']), float(lo['y'])
  match = origin(x, y)
  if match and float(hi['x']) - x > 100:
   name, dy = match
   height = float(hi['y']) - y
   if dy < 1 and height > 40:
    surfaces[name]['panel'].add(rgba(gdb.parse_and_eval('col')))
    under_header.add(name)
   elif abs(dy - 1) < .1 and 10 < height <= 32:
    surfaces[name]['header'].add(rgba(gdb.parse_and_eval('col')))
   elif abs(dy - 32) < .1 and height > 10:
    surfaces[name]['panel'].add(rgba(gdb.parse_and_eval('col')))
  return False
class Border(gdb.Breakpoint):
 def stop(self):
  lo, hi = gdb.parse_and_eval('p_min'), gdb.parse_and_eval('p_max')
  match = origin(float(lo['x']), float(lo['y']))
  if match and match[1] < 1 and float(hi['x']) - float(lo['x']) > 100:
   surfaces[match[0]]['border'].add(rgba(gdb.parse_and_eval('col')))
   # Pinned ImGui applies its own half-pixel alignment; a 1 px stroke's
   # inner edge must not cross the header fill beginning 1 px inside.
   border_inner_edges.add((match[0], match[1] + 1.0))
  return False
PanelClip('ImDrawList::PushClipRect', internal=True)
Foreground('ImFont::RenderText', internal=True)
Background('ImDrawList::AddRectFilled', internal=True)
Border('ImDrawList::AddRect', internal=True)
end
run
python
with open({str(output)!r}, 'w') as stream:
 json.dump(dict(surfaces=dict((name, dict((role, sorted(values)) for role, values in roles.items())) for name, roles in surfaces.items()), text=sorted(text_alphas), under_header=sorted(under_header), border_inner_edges=sorted(border_inner_edges)), stream)
end
quit
""")
            result = subprocess.run(
                [shutil.which("gdb"), "--batch", "-x", str(script), "--args",
                 os.environ["NGEN_FLIGHT_DEMO"], os.environ["NGEN_FLIGHT_PACKAGE"],
                 "--walkthrough", "--frames=330", "--warmup=2", "--mute"],
                capture_output=True, text=True, timeout=180)
            self.assertEqual(result.returncode, 0, result.stdout[-4000:] + result.stderr[-4000:])
            self.assertNotIn("Python Exception", result.stderr)
            observed = json.loads(output.read_text())
        self.assertEqual(observed["text"], [255], "Menu text must remain fully opaque")
        expected = {"panel": [10, 16, 29, 168], "header": [16, 24, 42, 140],
                    "border": [125, 155, 210, 36]}
        for menu, roles in observed["surfaces"].items():
            for role, values in roles.items():
                self.assertEqual(values, [expected[role]], f"{menu} {role}: actual drawn RGBA")
        self.assertEqual(observed["under_header"], [],
                         "Do not stack a translucent header on a second translucent panel fill")
        for menu, inner_edge in observed["border_inner_edges"]:
            self.assertLessEqual(inner_edge, 1.01,
                                 f"{menu} border stroke overlaps the translucent header fill")

    @unittest.skipUnless(shutil.which("gdb"), "Native panel geometry fixture needs GDB")
    def test_optional_panels_fit_content_including_return_from_data_to_controls(self):
        # Geometry only: sample settled walkthrough phases after two warmup frames
        # plus cold GUI preparation. Allocation tests retain 60-frame warmup.
        script = """set pagination off
set confirm off
set debuginfod enabled off
set $frames = 0
set $controls = -1.0
set $data = -1.0
set $performance = -1.0
set $returned = -1.0
break ngen::tools::Gui::begin
commands 1
 silent
 set $frames = $frames + 1
 continue
end
break ImGui::End
commands 2
 silent
 if ($frames > 13 && $frames < 20) || ($frames > 286 && $frames < 293) || $frames > 325
  set $position = ImGui::GetWindowPos()
  if ($position.x == 8 && $position.y == 64) || ($position.x == 922 && $position.y == 64)
   set $size = ImGui::GetWindowSize()
   set $gap = $size.y - ImGui::GetCursorPosY()
   if $position.x == 922
    set $performance = $gap
   else
    if $frames < 20
     set $controls = $gap
    else
     if $frames < 293
      set $data = $gap
     else
      set $returned = $gap
     end
    end
   end
  end
 end
 continue
end
run
printf "panel_bottom_gaps=%.2f,%.2f,%.2f,%.2f\\n", $controls, $data, $performance, $returned
quit
"""
        with tempfile.TemporaryDirectory(prefix="ngen-panel-fit-") as directory:
            commands = Path(directory) / "geometry.gdb"
            commands.write_text(script)
            result = subprocess.run(
                [shutil.which("gdb"), "--batch", "-x", str(commands), "--args",
                 os.environ["NGEN_FLIGHT_DEMO"], os.environ["NGEN_FLIGHT_PACKAGE"],
                 "--walkthrough", "--frames=330", "--warmup=2", "--mute", "--width=1280", "--height=720"],
                capture_output=True, text=True, timeout=180)
        self.assertEqual(result.returncode, 0, result.stdout[-4000:] + result.stderr[-4000:])
        match = re.search(r"panel_bottom_gaps=([-\d.]+),([-\d.]+),([-\d.]+),([-\d.]+)", result.stdout)
        self.assertIsNotNone(match, result.stdout[-4000:])
        for name, gap in zip(("F1", "F2", "F12", "F1 after F2"), map(float, match.groups()), strict=True):
            self.assertGreaterEqual(gap, 0, f"{name} content was not fully visible/measured")
            self.assertLessEqual(gap, 24, f"{name} retains excessive empty space below its content")

    def test_flight_menus_keep_branding_only_in_the_upper_left_icon(self):
        for frames, visible in ((10, "hud_controls_visible=1 hud_data_visible=0"),
                                (285, "hud_controls_visible=0 hud_data_visible=1")):
            with self.subTest(frames=frames), tempfile.TemporaryDirectory(prefix="ngen-menu-brand-") as directory:
                capture = Path(directory) / "menu.bmp"
                result = subprocess.run(
                    [os.environ["NGEN_FLIGHT_DEMO"], os.environ["NGEN_FLIGHT_PACKAGE"],
                     "--walkthrough", f"--frames={frames}", "--warmup=2", "--mute", f"--capture={capture}"],
                    capture_output=True, text=True, timeout=180)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual(result.stderr, "")
                self.assertIn(visible, result.stdout)
                data = capture.read_bytes()
            width, height = struct.unpack_from("<ii", data, 18)
            offset = struct.unpack_from("<I", data, 10)[0]
            def violet_pixels(left, top, right, bottom):
                total = 0
                for y in range(top, bottom):
                    for x in range(left, right):
                        at = offset + ((height - 1 - y) * width + x) * 4
                        b, g, r = data[at:at + 3]
                        total += r > 120 and b > 180 and b > g + 25
                return total
            self.assertGreater(violet_pixels(16, 12, 76, 64), 40, "The upper-left icon must remain")
            self.assertEqual(violet_pixels(16, 76, 420, 140), 0, "F1/F2 must not repeat the logo")

    @unittest.skipUnless(shutil.which("gdb"), "Native font loading fixture needs GDB")
    def test_ui_loads_supplied_brand_typefaces_without_external_font_files(self):
        with tempfile.TemporaryDirectory(prefix="ngen-brand-fonts-") as directory:
            output = Path(directory) / "fonts.json"
            commands = Path(directory) / "fonts.gdb"
            commands.write_text(f"""set pagination off
set confirm off
set debuginfod enabled off
python
import gdb, hashlib, json
fonts = []
class Font(gdb.Breakpoint):
 def stop(self):
  config = gdb.parse_and_eval('font_cfg_in')
  size = int(config['FontDataSize'])
  if size <= 0 or size > 4 * 1024 * 1024:
   raise RuntimeError('Unbounded GUI font source')
  data = bytes(gdb.selected_inferior().read_memory(config['FontData'], size))
  fonts.append((config['Name'].string(), float(config['SizePixels']), hashlib.sha256(data).hexdigest()))
  return False
Font('ImFontAtlas::AddFont', internal=True)
end
run
python
with open({str(output)!r}, 'w') as stream:
 json.dump(fonts, stream)
end
quit
""")
            result = subprocess.run([shutil.which("gdb"), "--batch", "-x", str(commands), "--args",
                os.environ["NGEN_FLIGHT_DEMO"], os.environ["NGEN_FLIGHT_PACKAGE"],
                "--replay", "--mute", "--frames=3", "--warmup=2"], capture_output=True, text=True, timeout=180)
            self.assertEqual(result.returncode, 0, result.stdout[-4000:] + result.stderr[-4000:])
            self.assertNotIn("Python Exception", result.stderr)
            fonts = json.loads(output.read_text())
        root = Path(__file__).resolve().parents[1] / "brand/web/fonts"
        sans = hashlib.sha256((root / "NeoSansPro-Regular.ttf").read_bytes()).hexdigest()
        mono = hashlib.sha256((root / "OperatorMonoLigNerdFont-Regular.otf").read_bytes()).hexdigest()
        self.assertCountEqual(fonts, [["Neo Sans Pro", 15.0, sans], ["Operator Mono", 13.0, mono],
                                     ["Neo Sans Pro", 28.0, sans]])

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
