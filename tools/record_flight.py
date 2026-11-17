"""Record only a newly launched NGEN window and its own Pulse playback stream."""
import argparse
import array
import json
import os
from pathlib import Path, PurePosixPath
import selectors
import signal
import shutil
import subprocess
import sys
import time
import uuid


_termination_signal = 0


def check_interrupted():
    if _termination_signal:
        raise KeyboardInterrupt(f"recording interrupted by signal {_termination_signal}")


def owned_audio_input(inputs, pid):
    """Return the unique owned (sink-input, sink) pair, or None before playback."""
    if not isinstance(pid, int) or isinstance(pid, bool) or not 0 < pid <= 2147483647:
        raise ValueError("invalid owned process")
    if not isinstance(inputs, list) or len(inputs) > 256:
        raise ValueError("invalid playback stream list")
    owned = []
    for item in inputs:
        if not isinstance(item, dict):
            continue
        properties = item.get("properties", {})
        if not isinstance(properties, dict) or properties.get("application.process.id") != str(pid):
            continue
        pair = (item.get("index"), item.get("sink"))
        if any(not isinstance(value, int) or isinstance(value, bool) or not 0 <= value < 2**32 for value in pair):
            raise ValueError("invalid owned playback stream")
        owned.append(pair)
    if len(owned) > 1:
        raise ValueError("ambiguous owned playback streams")
    return owned[0] if owned else None


def owned_window(payload, pid):
    """Validate the compositor's bounded identity for the newly launched window."""
    if not isinstance(payload, str) or len(payload) > 4096:
        raise ValueError("invalid owned window response")
    value = json.loads(payload)
    if not isinstance(value, dict) or set(value) != {"pid", "uuid", "width", "height"}:
        raise ValueError("invalid owned window shape")
    if type(value["pid"]) is not int or value["pid"] != pid:
        raise ValueError("foreign window process")
    for key in ("width", "height"):
        if type(value[key]) is not int or not 2 <= value[key] <= 4096:
            raise ValueError("unbounded owned window")
    identifier = str(uuid.UUID(value["uuid"]))
    if value["uuid"] not in (identifier, "{" + identifier + "}"):
        raise ValueError("invalid window identifier")
    return dict(value, uuid=identifier)


def owned_monitor(sinks, sink):
    """Resolve only the selected playback sink's monitor, never a default input."""
    if not isinstance(sinks, list) or len(sinks) > 256:
        raise ValueError("invalid sink collection")
    matches = [item for item in sinks if isinstance(item, dict) and item.get("index") == sink]
    if len(matches) != 1:
        raise ValueError("owned sink is missing or ambiguous")
    name = matches[0].get("monitor_source")
    if (not isinstance(name, str) or not 1 <= len(name) <= 512 or
            not name.endswith(".monitor") or any(ord(c) < 32 for c in name)):
        raise ValueError("owned sink has no admitted monitor source")
    return name


def pcm_peak(raw):
    """Admit only non-silent stereo PCM without either saturation endpoint."""
    samples = array.array("h", raw)
    if sys.byteorder != "little":
        samples.byteswap()
    peak = max((abs(value) for value in samples), default=0)
    if not peak or peak >= 32767:
        raise ValueError("silent or clipped NGEN audio")
    return peak


def video_pcm(raw, duration):
    """Return exactly the stereo PCM interval sent to the movie muxer."""
    length = int(duration * 48000) * 4
    aligned = raw[:length] + bytes(max(0, length - len(raw)))
    return aligned, pcm_peak(aligned)


def json_command(arguments):
    """Read bounded native metadata; retain no unrelated application properties."""
    process = subprocess.Popen(arguments, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    selector = selectors.DefaultSelector()
    streams = {process.stdout: bytearray(), process.stderr: bytearray()}
    for stream in streams:
        selector.register(stream, selectors.EVENT_READ)
    deadline = time.monotonic() + 15
    try:
        while selector.get_map():
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("native metadata deadline")
            for key, _ in selector.select(min(remaining, .5)):
                data = os.read(key.fd, 65536)
                if not data:
                    selector.unregister(key.fileobj)
                else:
                    streams[key.fileobj].extend(data)
                    if sum(len(value) for value in streams.values()) > 2 * 1024**2:
                        raise ValueError("native metadata exceeds its byte bound")
        if process.wait(timeout=2):
            raise RuntimeError("native metadata command failed: " + Path(arguments[0]).name)
        return json.loads(streams[process.stdout])
    finally:
        selector.close()
        for stream in streams:
            stream.close()
        if process.poll() is None:
            process.kill()
            process.wait()


def desktop_graph():
    cards = json_command(["pactl", "-f", "json", "list", "cards"])
    sinks = json_command(["pactl", "-f", "json", "list", "sinks"])
    default = subprocess.check_output(["pactl", "get-default-sink"], text=True, timeout=5).strip()
    return {"cards": {c["name"]: c["active_profile"] for c in cards},
            "sinks": {s["name"]: (s["index"], s.get("active_port")) for s in sinks},
            "default": default}


def admitted_bundle(path):
    from package_demo import private_path, regular, sha256
    path = Path(path).absolute()
    if private_path(path) or any(p.is_symlink() for p in (path, *path.parents)):
        raise ValueError("bundle must not use protected paths or links")
    root = path.resolve(strict=True)
    if not root.is_dir():
        raise ValueError("bundle must be a regular directory")
    manifest = json.loads(regular(root / "manifest.json", 2 * 1024**2).read_text())
    if manifest.get("schema") != 1 or manifest.get("platform") != "Linux":
        raise ValueError("recording requires a source-bound Linux bundle")
    files = manifest.get("files")
    if not isinstance(files, dict) or not 1 <= len(files) <= 1100:
        raise ValueError("invalid bundle manifest")
    if not {"flight_demo", "run.sh", "assets/ship.ngas", "source.tar.gz"} <= files.keys():
        raise ValueError("missing required bundle identity members")
    revision = manifest.get("source_revision")
    if not isinstance(revision, str) or len(revision) != 40 or any(c not in "0123456789abcdef" for c in revision):
        raise ValueError("invalid bundle source revision")
    total_bytes = 0
    for name, expected in files.items():
        relative = PurePosixPath(name)
        if relative.is_absolute() or ".." in relative.parts or "\\" in name:
            raise ValueError("unsafe bundle member")
        target = regular(root / relative, 256 * 1024**2, root=root)
        total_bytes += target.stat().st_size
        if total_bytes > 1024**3:
            raise ValueError("bundle exceeds its aggregate byte bound")
        if sha256(target) != expected:
            raise ValueError("bundle member hash mismatch")
    if files.get("assets/ship.ngas") != "99c996f33d3cc98dbd801e3aa302ee2a5894d0fa6bcfdd18f59b73e72e74d834":
        raise ValueError("bundle does not contain the accepted asset")
    regular(root / "flight_demo", 128 * 1024**2, root=root)
    return root, manifest


def owned_app_pid(parent, executable):
    """Resolve only descendants of our bubblewrap process, never named owner clients."""
    pending = [parent.pid]
    visited = set()
    while pending and len(visited) < 16:
        pid = pending.pop()
        if pid in visited:
            continue
        visited.add(pid)
        try:
            if Path(os.readlink(f"/proc/{pid}/exe")) == executable:
                return pid
            children = Path(f"/proc/{pid}/task/{pid}/children").read_text().split()
            pending.extend(int(value) for value in children)
        except (FileNotFoundError, ProcessLookupError):
            pass
    return None


def stop_owned_child(app, pid, executable):
    """Stop only our verified child while its supervisor can still reap it."""
    if app is None or app.poll() is not None or pid is None:
        return None
    descriptor = None
    failure = None
    try:
        descriptor = os.pidfd_open(pid)
        if owned_app_pid(app, executable) == pid:
            for termination in (signal.SIGTERM, signal.SIGKILL):
                signal.pidfd_send_signal(descriptor, termination)
                try:
                    app.wait(timeout=5)
                    break
                except subprocess.TimeoutExpired:
                    pass
    except ProcessLookupError:
        pass
    except OSError as error:
        failure = str(error)
    finally:
        if descriptor is not None:
            os.close(descriptor)
    if app.poll() is None:
        try:
            app.wait(timeout=5)
        except subprocess.TimeoutExpired:
            pass
    return failure


def record_bundle(bundle_path, output, seconds):
    from package_demo import regular, sha256
    root = Path(__file__).resolve().parents[1]
    bundle, manifest = admitted_bundle(bundle_path)
    source_names = ("tools/record_flight.py", "tools/record_flight/CMakeLists.txt",
                    "tools/record_flight/main.cpp", "tools/record_flight/screencast.xml")
    source_hashes = {name: sha256(regular(root / name, 2 * 1024**2, root=root)) for name in source_names}
    helper_build = root / "build/record-flight-helper-clang"
    subprocess.run(["cmake", "-S", str(root / "tools/record_flight"), "-B", str(helper_build),
                    "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_C_COMPILER=clang",
                    "-DCMAKE_CXX_COMPILER=clang++"], check=True, timeout=60)
    subprocess.run(["cmake", "--build", str(helper_build), "-j2"], check=True, timeout=180)
    check_interrupted()
    output = Path(output).absolute()
    if not output.parent.resolve(strict=True).is_relative_to((root / "build").resolve(strict=True)) or any(p.is_symlink() for p in (output, *output.parents)):
        raise ValueError("recording output must be a fresh regular directory under project build")
    before = desktop_graph()
    output.mkdir(mode=0o700, exist_ok=False)
    status = output / "capture-status.json"
    plugin = "ngen-capture-" + uuid.uuid4().hex
    sandbox = ["bwrap", "--die-with-parent", "--unshare-net", "--ro-bind", "/", "/",
               "--dev-bind", "/dev", "/dev", "--proc", "/proc", "--tmpfs",
               str(Path.home() / ".conan2"), "--chdir", str(bundle),
               "--setenv", "PATH", "/usr/bin:/bin"]
    for name in ("VULKAN_SDK", "VK_LAYER_PATH", "VK_INSTANCE_LAYERS", "LD_LIBRARY_PATH",
                 "LD_PRELOAD", "CMAKE_PREFIX_PATH", "CONAN_HOME", "ALSA_CONFIG_PATH", "ALSA_CONFIG_DIR", "ALSA_PLUGIN_DIR",
                 "SDL_AUDIO_DRIVER", "SDL_AUDIODRIVER"):
        sandbox += ["--unsetenv", name]
    arguments = sandbox + [str(bundle / "run.sh"), "--walkthrough", "--frames=3600",
                           "--warmup=60", "--width=1280", "--height=720"]
    handles = []
    app = capture = monitor = None
    pid = None
    segments = []
    pcm_frames = 0
    last_end = 0
    capture_info = {}
    current_pair = None
    segment = None
    next_query = 0.0
    started = 0
    selector = selectors.DefaultSelector()
    result = {"schema": 1, "status": "failed", "source_revision": manifest["source_revision"],
              "recorder_source_hashes": source_hashes,
              "recorder_executable_sha256": sha256(helper_build / "ngen_window_capture"),
              "bundle_manifest_sha256": sha256(bundle / "manifest.json"),
              "network_unshared": True, "conan_hidden": True, "sdk_environment_absent": True,
              "microphone": False, "system_audio_mix": False}

    def log(name):
        stream = (output / name).open("xb")
        handles.append(stream)
        return stream

    def stop_monitor():
        nonlocal monitor
        if monitor is not None:
            try:
                selector.unregister(monitor.stdout)
            except KeyError:
                pass
            if monitor.poll() is None:
                monitor.terminate()
                try:
                    monitor.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    monitor.kill()
                    monitor.wait()
            monitor.stdout.close()
            monitor = None

    try:
        app = subprocess.Popen(arguments, stdout=log("flight.txt"), stderr=log("flight.err"))
        deadline = time.monotonic() + 20
        while app.poll() is None and time.monotonic() < deadline:
            pid = owned_app_pid(app, bundle / "flight_demo")
            if pid:
                break
            time.sleep(.05)
        if not pid:
            raise RuntimeError("owned sandboxed NGEN process was not found")
        result["target_pid"] = pid
        check_interrupted()
        capture = subprocess.Popen([str(helper_build / "ngen_window_capture"), "--pid", str(pid),
                                    "--seconds", str(seconds), "--output", str(output / "video.webm"),
                                    "--status", str(status), "--script-name", plugin],
                                   stdout=log("capture.txt"), stderr=log("capture.err"))
        deadline = time.monotonic() + seconds + 45
        with (output / "audio.raw").open("xb+") as audio:
            while capture.poll() is None:
                check_interrupted()
                if time.monotonic() > deadline or app.poll() is not None:
                    raise RuntimeError("owned recording lifecycle deadline or early NGEN exit")
                if status.exists():
                    capture_info = json.loads(regular(status, 65536).read_text())
                    if capture_info.get("microphone") is not False or capture_info.get("system_audio") is not False:
                        raise ValueError("capture backend enabled an unapproved audio source")
                    if capture_info.get("recording_started_ns"):
                        started = int(capture_info["recording_started_ns"])
                        owned_window(json.dumps(capture_info["window"]), pid)
                if started and time.monotonic() >= next_query:
                    next_query = time.monotonic() + .1
                    pair = owned_audio_input(json_command(["pactl", "-f", "json", "list", "sink-inputs"]), pid)
                    if pair != current_pair or monitor is not None and monitor.poll() is not None:
                        stop_monitor()
                        current_pair = pair
                        if pair is not None:
                            device = owned_monitor(json_command(["pactl", "-f", "json", "list", "sinks"]), pair[1])
                            monitor = subprocess.Popen(["parec", f"--monitor-stream={pair[0]}",
                                                        "--device=" + device, "--format=s16le", "--rate=48000",
                                                        "--channels=2", "--no-remix", "--no-remap",
                                                        "--latency-msec=20", "--process-time-msec=10",
                                                        "--client-name=ngen-flight-recorder"],
                                                       stdout=subprocess.PIPE, stderr=log(f"audio-{len(segments)}.err"))
                            selector.register(monitor.stdout, selectors.EVENT_READ)
                            segment = {"sink_input": pair[0], "sink": pair[1], "monitor": device,
                                       "started_ns": time.monotonic_ns(), "pcm_frames": 0}
                            segments.append(segment)
                if monitor is not None:
                    for key, _ in selector.select(.02):
                        data = os.read(key.fd, 65536)
                        if data:
                            if len(data) % 4:
                                raise ValueError("unaligned stereo PCM packet")
                            frames = len(data) // 4
                            position = max(last_end, max(0, int((time.monotonic_ns() - started) * 48000 / 1e9) - frames))
                            if position + frames > (seconds + 10) * 48000:
                                raise ValueError("captured audio exceeds its duration bound")
                            audio.seek(position * 4)
                            audio.write(data)
                            last_end = position + frames
                            pcm_frames += frames
                            segment["pcm_frames"] += frames
                else:
                    time.sleep(.02)
                if (output / "video.webm").exists() and (output / "video.webm").stat().st_size > 512 * 1024**2:
                    raise ValueError("captured video exceeds its byte bound")
        stop_monitor()
        capture_info = json.loads(regular(status, 65536).read_text())
        if capture.wait(timeout=5) or capture_info.get("state") != "complete":
            raise RuntimeError("owned window video capture failed; inspect retained diagnostics")
        media = json_command(["ffprobe", "-v", "error", "-count_frames", "-show_entries",
                              "stream=codec_type,width,height,nb_read_frames:format=duration", "-of", "json",
                              str(output / "video.webm")])
        duration = float(media["format"]["duration"])
        if len(media["streams"]) != 1 or media["streams"][0]["codec_type"] != "video" or not 1 <= duration <= seconds + 5:
            raise ValueError("invalid own-window video")
        if int(media["streams"][0]["nb_read_frames"]) < seconds * 10:
            raise ValueError("insufficient captured video frames")
        if pcm_frames < duration * 48000 * .85:
            raise ValueError("insufficient actual NGEN audio samples; no fallback source is allowed")
        raw = regular(output / "audio.raw", (seconds + 10) * 192000).read_bytes()
        aligned, peak = video_pcm(raw, duration)
        with (output / "audio-aligned.raw").open("xb") as stream:
            stream.write(aligned)
        subprocess.run(["ffmpeg", "-v", "error", "-nostdin", "-i", str(output / "video.webm"),
                        "-f", "s16le", "-ar", "48000", "-ac", "2", "-i", str(output / "audio-aligned.raw"),
                        "-map", "0:v:0", "-map", "1:a:0", "-c:v", "copy", "-c:a", "flac",
                        str(output / "recording.mkv")], stdout=log("mux.txt"), stderr=log("mux.err"),
                       check=True, timeout=60)
        muxed = json_command(["ffprobe", "-v", "error", "-count_frames", "-show_entries",
                              "stream=codec_type,width,height,channels,sample_rate,nb_read_frames:format=duration",
                              "-of", "json", str(output / "recording.mkv")])
        if sorted(stream["codec_type"] for stream in muxed["streams"]) != ["audio", "video"]:
            raise ValueError("final recording must have exactly one video and one own-audio stream")
        video_stream = next(stream for stream in muxed["streams"] if stream["codec_type"] == "video")
        audio_stream = next(stream for stream in muxed["streams"] if stream["codec_type"] == "audio")
        if video_stream["nb_read_frames"] != media["streams"][0]["nb_read_frames"] or audio_stream["channels"] != 2 or audio_stream["sample_rate"] != "48000":
            raise ValueError("mux changed video frame coverage or stereo audio format")
        if {name: sha256(root / name) for name in source_names} != source_hashes:
            raise ValueError("recording tool source changed during capture")
        # Let the explicitly bounded automated walkthrough finish; never close an owner preview.
        deadline = time.monotonic() + 180
        while app.poll() is None:
            check_interrupted()
            if time.monotonic() >= deadline:
                raise TimeoutError("automated NGEN completion deadline")
            time.sleep(.1)
        check_interrupted()
        if app.returncode:
            raise RuntimeError("automated NGEN walkthrough did not complete successfully")
        graph_unchanged = desktop_graph() == before
        check_interrupted()
        result.update(status="recorded", target_pid=pid, window=owned_window(json.dumps(capture_info["window"]), pid),
                      recording_sha256=sha256(output / "recording.mkv"), video=media, muxed_media=muxed,
                      audio_segments=segments, observed_pcm_frames=pcm_frames, peak_s16=peak,
                      desktop_graph_unchanged=graph_unchanged,
                      scope="Own KDE window via PipeWire v6; only the launched NGEN PID's Pulse sink-input monitor. Audio alignment is monotonic-time estimated, not sample-accurate AV certification. Not clean timing or owner acceptance.")
        if not result["desktop_graph_unchanged"]:
            raise RuntimeError("desktop audio configuration changed during capture; no restoration is attempted")
    except BaseException as error:
        result.update(status="failed", error=str(error))
        raise
    finally:
        stop_monitor()
        selector.close()
        # Stop the inner NGEN child while bubblewrap can still reap it. Killing
        # its supervisor first can leave an asynchronously dying/orphaned child.
        cleanup_error = stop_owned_child(app, pid, bundle / "flight_demo")
        if cleanup_error:
            result.update(status="failed", cleanup_error=cleanup_error)
        for process in (capture, app):
            if process is not None and process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
        try:
            cleanup = subprocess.run(["qdbus6", "org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting.unloadScript", plugin],
                                     capture_output=True, timeout=10)
            if cleanup.returncode:
                raise RuntimeError("owned KWin script cleanup failed")
        except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
            result.update(status="failed", cleanup_error=str(error))
        finally:
            for stream in handles:
                stream.close()
            if _termination_signal:
                result.update(status="failed", error=f"recording interrupted by signal {_termination_signal}")
            (output / "recording.json").write_text(json.dumps(result, indent=2) + "\n")
    if result["status"] != "recorded":
        raise RuntimeError("recording cleanup failed; inspect retained metadata")
    return output / "recording.mkv"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bundle", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--seconds", type=int, default=20)
    args = parser.parse_args()
    if sys.platform != "linux" or not 5 <= args.seconds <= 30:
        parser.error("this KDE Wayland recorder requires Linux and 5..30 recording seconds")
    for name in ("cmake", "ninja", "clang", "clang++", "qdbus6", "bwrap", "pactl", "parec", "ffmpeg", "ffprobe"):
        if not shutil.which(name):
            parser.error("missing recording tool: " + name)
    output = args.output or Path(__file__).resolve().parents[1] / "build" / ("flight-recording-" + uuid.uuid4().hex)
    def terminated(signum, _frame):
        # Defer to a managed safepoint, never interrupt Popen before assignment.
        global _termination_signal
        _termination_signal = signum
    global _termination_signal
    _termination_signal = 0
    previous = {number: signal.signal(number, terminated) for number in (signal.SIGTERM, signal.SIGINT)}
    try:
        movie = record_bundle(args.bundle, output, args.seconds)
        check_interrupted()
        print(movie)
    except KeyboardInterrupt as error:
        print(str(error), file=sys.stderr)
        raise SystemExit(130) from None
    finally:
        for number, handler in previous.items():
            signal.signal(number, handler)


if __name__ == "__main__":
    main()
