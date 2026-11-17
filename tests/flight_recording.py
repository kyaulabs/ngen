"""Public recorder policy contracts; desktop APIs are exercised separately."""
import importlib.util
import hashlib
import json
import os
import signal
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
SPEC = importlib.util.spec_from_file_location("ngen_record_flight", ROOT / "tools/record_flight.py")
record = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(record)


class Recording(unittest.TestCase):
    def test_audio_source_is_one_owned_playback_stream_not_the_desktop_or_microphone(self):
        foreign = {"index": 71, "sink": 0, "properties": {"application.process.id": "999"}}
        owned = {"index": 84, "sink": 0, "properties": {"application.process.id": "123"}}
        self.assertEqual(record.owned_audio_input([foreign, owned], 123), (84, 0))
        self.assertIsNone(record.owned_audio_input([foreign], 123))
        with self.assertRaisesRegex(ValueError, "ambiguous"):
            record.owned_audio_input([owned, dict(owned, index=85)], 123)

    def test_window_identity_cannot_select_another_process_or_an_unbounded_surface(self):
        target = {"pid": 123, "uuid": "{86e00497-277e-4446-88ec-80fc2756a820}",
                  "width": 1280, "height": 720}
        self.assertEqual(record.owned_window(json.dumps(target), 123),
                         dict(target, uuid="86e00497-277e-4446-88ec-80fc2756a820"))
        for changed in ({"pid": 999}, {"width": 0}, {"height": 99999}, {"uuid": "screen"}):
            with self.subTest(changed=changed), self.assertRaises(ValueError):
                record.owned_window(json.dumps(dict(target, **changed)), 123)

    def test_audio_monitor_is_the_owned_sink_monitor_without_default_input_fallback(self):
        sinks = [{"index": 0, "monitor_source": "desktop.monitor"},
                 {"index": 7, "monitor_source": "game.monitor"}]
        self.assertEqual(record.owned_monitor(sinks, 7), "game.monitor")
        with self.assertRaises(ValueError):
            record.owned_monitor(sinks, 99)
        with self.assertRaises(ValueError):
            record.owned_monitor([{"index": 7, "monitor_source": "microphone"}], 7)

    def test_pcm_rejects_both_saturation_endpoints_and_silence(self):
        self.assertEqual(record.pcm_peak(struct.pack("<hh", -12000, 12000)), 12000)
        for sample in (32767, -32768, 0):
            with self.subTest(sample=sample), self.assertRaisesRegex(ValueError, "silent or clipped"):
                record.pcm_peak(struct.pack("<hh", sample, sample))

    def test_nonzero_audio_after_video_cannot_admit_a_silent_mux_interval(self):
        raw = bytes(5 * 192000) + struct.pack("<hh", 1000, -1000) * 4800
        with self.assertRaisesRegex(ValueError, "silent or clipped"):
            record.video_pcm(raw, 5)
        aligned, peak = record.video_pcm(struct.pack("<hh", 2000, -2000) * 48000, .5)
        self.assertEqual(len(aligned), 96000)
        self.assertEqual(peak, 2000)

    def test_pidfd_failure_does_not_skip_supervisor_reaping_or_failure_metadata(self):
        app = mock.Mock(spec=subprocess.Popen)
        app.poll.return_value = None
        with mock.patch.object(record.os, "pidfd_open", create=True, side_effect=PermissionError("test pidfd denied")):
            error = record.stop_owned_child(app, 12345, ROOT / "build/fixture/flight_demo")
        self.assertIn("test pidfd denied", error)
        app.wait.assert_called_once_with(timeout=5)

    @unittest.skipUnless(os.getenv("NGEN_FLIGHT_PACKAGE"), "Private real-asset bundle boundary")
    def test_an_unhashed_executable_is_not_admitted_for_launch(self):
        with tempfile.TemporaryDirectory(prefix="record-bundle-", dir=ROOT / "build") as directory:
            root = Path(directory)
            (root / "assets").mkdir()
            os.link(os.environ["NGEN_FLIGHT_PACKAGE"], root / "assets/ship.ngas")
            files = {"assets/ship.ngas": "99c996f33d3cc98dbd801e3aa302ee2a5894d0fa6bcfdd18f59b73e72e74d834"}
            for name, content in (("flight_demo", b"never execute this fixture"),
                                  ("run.sh", b"#!/bin/sh\nexit 99\n"),
                                  ("source.tar.gz", b"source archive fixture")):
                (root / name).write_bytes(content)
                files[name] = hashlib.sha256(content).hexdigest()
            for missing in ("flight_demo", "run.sh"):
                with self.subTest(missing=missing):
                    (root / "manifest.json").write_text(json.dumps({
                        "schema": 1, "platform": "Linux", "source_revision": "1" * 40,
                        "files": {key: value for key, value in files.items() if key != missing}}))
                    with self.assertRaisesRegex(ValueError, "required bundle"):
                        record.admitted_bundle(root)

    @unittest.skipUnless(os.getenv("NGEN_RECORDING_BUNDLE"), "Opt-in KDE/Pulse recorder integration")
    def test_sigterm_retains_failure_metadata_and_reaps_the_owned_app(self):
        for phase in ("startup", "recording"):
            with self.subTest(phase=phase), tempfile.TemporaryDirectory(prefix="record-cancel-", dir=ROOT / "build") as directory:
                output = Path(directory) / "private"
                with (Path(directory) / "stdout").open("wb") as stdout, (Path(directory) / "stderr").open("wb") as stderr:
                    process = subprocess.Popen([sys.executable, str(ROOT / "tools/record_flight.py"),
                        "--bundle", os.environ["NGEN_RECORDING_BUNDLE"], "--output", str(output), "--seconds", "30"],
                        stdout=stdout, stderr=stderr)
                    try:
                        deadline = time.monotonic() + 35
                        while time.monotonic() < deadline:
                            status = output / "capture-status.json"
                            if phase == "startup" and (output / "flight.txt").exists():
                                break
                            if phase == "recording" and status.exists() and json.loads(status.read_text()).get("state") == "recording":
                                break
                            self.assertIsNone(process.poll(), "Recorder exited before cancellation phase")
                            time.sleep(.01)
                        else:
                            self.fail("Recorder did not reach cancellation phase")
                        process.send_signal(signal.SIGTERM)
                        self.assertNotEqual(process.wait(timeout=30), 0)
                        metadata = json.loads((output / "recording.json").read_text())
                        self.assertEqual(metadata["status"], "failed")
                        target = metadata["target_pid"]
                        self.assertFalse(Path(f"/proc/{target}").exists(), "Owned NGEN process survived cancellation")
                    finally:
                        if process.poll() is None:
                            process.terminate()
                            try:
                                process.wait(timeout=30)
                            except subprocess.TimeoutExpired:
                                process.kill()
                                process.wait()


if __name__ == "__main__":
    unittest.main()
