"""Launch the owner-driven Surveyors charter and record only its window."""
import argparse
import json
import os
from pathlib import Path
import shutil
import signal
import stat
import subprocess
import sys
import time
import uuid

from package_demo import private_path, regular, sha256
import record_flight as recorder

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
VIDEO_LIMIT = 512 * 1024**2


def capture_window(info, pid):
    if (not isinstance(info, dict) or info.get("state") not in ("recording", "complete") or
            type(info.get("target_pid")) is not int or info["target_pid"] != pid or
            type(info.get("recording_started_ns")) is not int or
            info["recording_started_ns"] <= 0 or info.get("microphone") is not False or
            info.get("system_audio") is not False):
        raise ValueError("capture did not start the owned video-only window")
    return recorder.owned_window(json.dumps(info.get("window")), pid)


def checked_directory(path):
    path = path.absolute()
    if ".." in path.parts or any(private_path(p) or p.is_symlink() for p in (path, *path.parents)):
        raise ValueError("unsafe recording or save directory")
    return path


def terminate(process):
    if process is not None and process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=5)


def capture(args):
    executable = regular(args.flight_demo, 128 * 1024**2, root=BUILD)
    if executable.name != "flight_demo" or not os.access(executable, os.X_OK):
        raise ValueError("expected a local flight_demo executable under build")
    frigate = regular(args.frigate, 256 * 1024**2)
    landmark = regular(args.landmark, 16 * 1024**2)
    output = checked_directory(args.output or BUILD / ("survey-recording-" + uuid.uuid4().hex))
    if output == BUILD or not output.is_relative_to(BUILD) or output.exists():
        raise ValueError("recording output must be a new directory under build")
    save = checked_directory(args.save) if args.save else output / "charter"
    if args.save:
        identity = save.stat()
        if (not stat.S_ISDIR(identity.st_mode) or identity.st_uid != os.geteuid() or
                stat.S_IMODE(identity.st_mode) != 0o700):
            raise ValueError("an existing charter must be a private owner directory (0700)")

    helper_build = BUILD / "record-flight-helper-clang"
    subprocess.run(["cmake", "-S", str(ROOT / "tools/record_flight"), "-B", str(helper_build),
                    "-G", "Ninja", "-DCMAKE_CXX_COMPILER=clang++", "-DCMAKE_BUILD_TYPE=Release"],
                   check=True, timeout=90)
    subprocess.run(["cmake", "--build", str(helper_build), "-j", "2"], check=True, timeout=120)
    helper = regular(helper_build / "ngen_window_capture", 16 * 1024**2, root=BUILD)
    recorder.check_interrupted()
    output.mkdir(mode=0o700)
    if not args.save:
        save.mkdir(mode=0o700)
    print(f"Private recording directory: {output}\nCharter: {save}\n"
          "Wait for RECORDING before using the game. Close the window to finish.", flush=True)
    video, status = output / "recording.webm", output / "capture-status.json"
    plugin = "ngen-survey-capture-" + uuid.uuid4().hex
    result = {"status": "failed", "save_directory": str(save), "video_only": True,
              "executable_sha256": sha256(executable), "recorder_sha256": sha256(helper),
              "frigate_sha256": sha256(frigate), "landmark_sha256": sha256(landmark)}
    app = backend = pid = None
    announced = False
    info = {}
    try:
        with (output / "flight.log").open("xb") as log:
            app = subprocess.Popen([
                "unshare", "--user", "--map-root-user", "--net", "--", "env",
                "SDL_VIDEO_DRIVER=wayland", str(executable), str(frigate),
                f"--destination={landmark}", f"--survey={save}",
                "--width=1280", "--height=720", "--mute",
            ], stdout=log, stderr=subprocess.STDOUT)
        startup_deadline = time.monotonic() + 25
        while pid is None and app.poll() is None and time.monotonic() < startup_deadline:
            recorder.check_interrupted()
            pid = recorder.owned_app_pid(app, executable)
            if pid is None:
                time.sleep(.05)
        if pid is None or app.poll() is not None:
            raise RuntimeError("could not start the owned flight window; see flight.log")
        result["target_pid"] = pid
        with (output / "recorder.log").open("xb") as log:
            backend = subprocess.Popen([
                str(helper), "--pid", str(pid), "--until-close", "--seconds", str(args.seconds),
                "--output", str(video), "--status", str(status), "--script-name", plugin,
            ], stdout=log, stderr=subprocess.STDOUT)
        deadline = time.monotonic() + 25
        shutdown_deadline = None
        while True:
            recorder.check_interrupted()
            if status.exists():
                info = json.loads(regular(status, 16384, root=output).read_text())
                if info.get("state") == "failed":
                    raise RuntimeError("window capture failed: " + str(info.get("error", "")))
                if not announced and info.get("state") == "recording":
                    capture_window(info, pid)
                    print("RECORDING — play now. Close the game window when done.", flush=True)
                    announced = True
                    deadline = time.monotonic() + args.seconds + 17
            if video.exists():
                regular(video, VIDEO_LIMIT, root=output)
            if backend.poll() is not None:
                # Read again after reaping: the last atomic status publication may
                # have raced the status read at the start of this iteration.
                if backend.returncode or not status.exists():
                    raise RuntimeError("window recorder exited early; see recorder.log")
                info = json.loads(regular(status, 16384, root=output).read_text())
                if info.get("state") != "complete" or not announced:
                    raise RuntimeError("window recorder did not complete a started stream")
                break
            now = time.monotonic()
            if app.poll() is not None and not announced:
                raise RuntimeError("flight window closed before recording started; see flight.log")
            if app.poll() is not None and shutdown_deadline is None:
                shutdown_deadline = now + 17
            if now >= deadline or (shutdown_deadline is not None and now >= shutdown_deadline):
                phase = "finish after closing" if announced else "start recording"
                raise TimeoutError(f"owned window recorder did not {phase} within its deadline")
            time.sleep(.1)
        window = capture_window(info, pid)
        stopped = info.get("recording_stopped_ns")
        if type(stopped) is not int or stopped <= info["recording_started_ns"]:
            raise ValueError("capture did not publish a valid stop time")
        finish = info.get("stop_reason")
        if finish not in ("window_closed", "time_limit"):
            raise ValueError("capture did not publish a recognized stop reason")
        if finish == "time_limit" and stopped - info["recording_started_ns"] < args.seconds * 10**9:
            raise RuntimeError("capture ended before the time limit elapsed")
        try:
            app.wait(timeout=3)
        except subprocess.TimeoutExpired:
            if finish != "time_limit":
                raise RuntimeError("game did not exit after the owned window closed")
            print("Recording time limit reached; closing the launched game. Your save is retained.",
                  flush=True)
            if recorder.stop_owned_child(app, pid, executable):
                raise RuntimeError("could not stop the owned flight window")
        if app.poll() is None:
            raise RuntimeError("owned game did not exit after the recording limit")
        expected_exit = (0, -signal.SIGTERM) if finish == "time_limit" else (0,)
        if app.returncode not in expected_exit:
            print("The game reported an exit error (see flight.log); checking the recorded clip.",
                  file=sys.stderr, flush=True)
        media = recorder.json_command(["ffprobe", "-v", "error", "-show_entries",
                                       "stream=codec_type,width,height:format=duration",
                                       "-of", "json", str(regular(video, VIDEO_LIMIT, root=output))])
        if ([s["codec_type"] for s in media["streams"]] != ["video"] or
                not 0 < float(media["format"]["duration"]) <= args.seconds + 2):
            raise ValueError("recording is not a bounded video-only clip")
        result.update(status="recorded", finish_reason=finish, game_exit_code=app.returncode,
                      window=window, media=media,
                      recording_sha256=sha256(video))
    except BaseException as error:
        result.update(status="failed", error=str(error))
        raise
    finally:
        # The native helper unloads its own KWin script on normal exit. If
        # interrupted, its unique ID is safe for a targeted cleanup request.
        abnormal = backend is not None and backend.poll() != 0
        errors = []

        def cleanup(operation):
            try:
                if error := operation():
                    errors.append(str(error))
            except (OSError, RuntimeError, subprocess.SubprocessError) as error:
                errors.append(str(error))

        if app is not None and app.poll() is None:
            cleanup(lambda: recorder.stop_owned_child(app, pid, executable))
        cleanup(lambda: terminate(backend))
        cleanup(lambda: terminate(app))
        if abnormal:
            cleanup(lambda: subprocess.run(["qdbus6", "org.kde.KWin", "/Scripting",
                            "org.kde.kwin.Scripting.unloadScript", plugin], capture_output=True,
                            timeout=8).returncode or None)
        if errors:
            result["cleanup_errors"] = errors
        (output / "recording.json").write_text(json.dumps(result, indent=2) + "\n")
    return video


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--flight-demo", type=Path, required=True)
    parser.add_argument("--frigate", type=Path, required=True)
    parser.add_argument("--landmark", type=Path, required=True)
    parser.add_argument("--save", type=Path, help="resume an existing private charter")
    parser.add_argument("--output", type=Path, help="fresh directory under build")
    parser.add_argument("--seconds", type=int, default=1200, help="safety limit, 5–1800 (default 1200)")
    args = parser.parse_args()
    if sys.platform != "linux" or not 5 <= args.seconds <= 1800:
        parser.error("the KDE Wayland recorder requires Linux and 5..1800 seconds")
    for name in ("cmake", "ninja", "clang++", "unshare", "qdbus6", "ffprobe"):
        if not shutil.which(name):
            parser.error("missing recording tool: " + name)

    def interrupted(signum, _frame):
        recorder._termination_signal = signum

    previous = {number: signal.signal(number, interrupted) for number in (signal.SIGTERM, signal.SIGINT)}
    try:
        print(f"Recorded: {capture(args)}")
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError, KeyboardInterrupt) as error:
        print(f"Surveyors recording: {error}", file=sys.stderr)
        return 1
    finally:
        for number, handler in previous.items():
            signal.signal(number, handler)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
