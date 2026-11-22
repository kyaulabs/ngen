"""Opt-in native RCS/asset checks; requires a physical Vulkan device for replay.

NGEN_FLIGHT_DEMO, NGEN_FLIGHT_PACKAGE and NGEN_RCS_BASE select owned local inputs.
Private package/capture bytes are never printed or published.
"""
import csv
import math
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest


class FlightRcs(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        executable = os.environ.get("NGEN_FLIGHT_DEMO")
        asset = os.environ.get("NGEN_FLIGHT_PACKAGE")
        if not executable or not asset:
            raise unittest.SkipTest("set native demo and private RCS package")
        cls.executable, cls.asset = Path(executable), Path(asset)

    def run_demo(self, *arguments):
        result = subprocess.run([str(self.executable), str(self.asset), *arguments],
                                capture_output=True, text=True, timeout=180, check=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stderr, "")
        return result.stdout

    def test_compiled_mix_matches_authored_force_and_moment_axes(self):
        text = self.run_demo("--verify")
        self.assertIn("flight_rcs_banks=16 flight_rcs_sockets=32", text)
        self.assertIn("flight_rcs_geometry_verified=1", text)
        self.assertIn("triangles=249942", text)

    def test_crossing_prefetches_a_destination_file(self):
        text = self.run_demo("--crossing", "--replay", "--mute", "--frames=120", "--warmup=12",
                             "--destination=" + str(self.asset))
        self.assertIn("flight_crossing_arrived=1", text)
        self.assertIn("destination_file_ready=1", text)

    def test_crossing_draws_distinct_destination_geometry(self):
        landmark = os.environ.get("NGEN_CROSSING_LANDMARK")
        if not landmark:
            self.skipTest("set cooked public starter marker as NGEN_CROSSING_LANDMARK")
        with tempfile.TemporaryDirectory(prefix="ngen-landmark-") as directory:
            capture = Path(directory) / "destination.bmp"
            text = self.run_demo("--crossing", "--replay", "--mute", "--frames=120",
                                 "--warmup=12", "--destination=" + landmark,
                                 "--capture=" + str(capture))
            self.assertIn("flight_crossing_arrived=1", text)
            self.assertIn("destination_file_ready=1", text)
            self.assertRegex(text, r"destination_landmark_frames=[1-9][0-9]*")
            raw = capture.read_bytes()
            offset = struct.unpack_from("<I", raw, 10)[0]
            width, height = struct.unpack_from("<ii", raw, 18)
            self.assertEqual(struct.unpack_from("<H", raw, 28)[0], 32)
            self.assertTrue(any(green > 120 and green > 2 * red and green > 2 * blue
                                for blue, green, red, _ in
                                struct.iter_unpack("4B", raw[offset:offset + width * abs(height) * 4])),
                            "final flight capture did not contain the green destination landmark")

    def test_crossing_retries_after_a_corrupt_destination(self):
        landmark = os.environ.get("NGEN_CROSSING_LANDMARK")
        if not landmark:
            self.skipTest("set cooked public starter marker as NGEN_CROSSING_LANDMARK")
        with tempfile.TemporaryDirectory(prefix="ngen-crossing-retry-") as directory:
            corrupt = Path(directory) / "corrupt.ngas"
            corrupt.write_bytes(b"not a checked package\n")
            text = self.run_demo("--crossing", "--replay", "--mute", "--frames=120",
                                 "--warmup=12", "--destination=" + str(corrupt),
                                 "--destination-retry=" + landmark)
            self.assertIn("flight_crossing_arrived=1", text)
            self.assertIn("destination_rejected_queued_frames=6 destination_retried=1", text)
            self.assertRegex(text, r"destination_landmark_frames=[1-9][0-9]*")

    def test_crossing_keeps_queuing_when_destination_is_missing(self):
        with tempfile.TemporaryDirectory(prefix="ngen-missing-") as directory:
            result = subprocess.run([str(self.executable), str(self.asset), "--crossing", "--replay",
                                     "--mute", "--frames=32", "--warmup=4",
                                     "--destination=" + str(Path(directory) / "missing.ngas")],
                                    capture_output=True, text=True, timeout=180, check=False)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn("flight_crossing_arrived=0 queued_blocked_exit_frames=24", result.stdout)
            self.assertIn("destination file rejected", result.stderr)

    def test_crossing_queues_blocked_exit_and_resumes_local_ticks(self):
        with tempfile.TemporaryDirectory(prefix="ngen-crossing-") as directory:
            report = Path(directory) / "crossing.csv"
            text = self.run_demo("--crossing", "--crossing-hold", "--replay", "--mute", "--frames=60",
                                 "--warmup=12", "--report=" + str(report))
            self.assertIn("flight_crossing_arrived=1", text)
            with report.open(newline="") as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual(len(rows), 60)
            ticks = [int(row["tick"]) for row in rows]
            queues = [int(row["queue_ns"]) for row in rows]
            self.assertTrue(all(a < b for a, b in zip(queues, queues[1:])))
            fields = dict(word.split("=", 1) for line in text.splitlines()
                          if line.startswith(("flight_crossing_arrived=", "crossing_first_exit_frame="))
                          for word in line.split())
            start = int(fields["crossing_first_exit_frame"])
            count = int(fields["queued_blocked_exit_frames"])
            self.assertGreaterEqual(count, 6)
            self.assertIn("crossing_collision_blocked_frames=6", text)
            self.assertEqual(int(fields["crossing_arrival_frame"]), start + count)
            self.assertGreater(int(fields["crossing_transit_upload_polls"]), 0)
            self.assertTrue(all(tick == ticks[0] for tick in ticks[start:start + count]))
            self.assertGreater(ticks[start + count], ticks[start + count - 1])
            self.assertEqual(ticks[-1] - ticks[-2], 1)

    def test_replay_reports_host_queue_timestamps(self):
        with tempfile.TemporaryDirectory(prefix="ngen-queue-") as directory:
            report = Path(directory) / "queue.csv"
            self.run_demo("--replay", "--mute", "--frames=12", "--warmup=4",
                          "--report=" + str(report))
            with report.open(newline="") as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual(len(rows), 12)
            previous = 0
            for row in rows:
                queued = int(row["queue_ns"])
                ended = int(row["host_ns"])
                self.assertGreater(queued, previous)
                self.assertLessEqual(ended - int(row["frame_ns"]), queued)
                self.assertLessEqual(queued, ended)
                self.assertLessEqual(len(row), 64)
                previous = queued

    def test_native_replay_renders_physical_braking_and_quiet_coast(self):
        with tempfile.TemporaryDirectory(prefix="ngen-rcs-") as directory:
            report = Path(directory) / "replay.csv"
            capture = Path(directory) / "replay.bmp"
            text = self.run_demo("--replay", "--validation", "--mute", "--frames=330",
                                 "--warmup=60", "--report=" + str(report),
                                 "--capture=" + str(capture))
            self.assertIn("flight_hdr_finite=1", text)
            self.assertIn("imgui_calls=0", text)
            self.assertGreater(capture.stat().st_size, 54)
            self.assertLessEqual(capture.stat().st_size, 8 * 1024**2)
            with report.open(newline="") as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual(len(rows), 330)
            self.assertLessEqual(len(rows[0]), 64)  # Existing Windows observer capacity.
            active, idle = 0, 0
            for row in rows:
                count, power = int(row["rcs_active"]), float(row["rcs_power"])
                demand = float(row["rcs_demand"])
                self.assertTrue(0 <= count <= 16 and math.isfinite(power) and 0 <= power <= 1)
                self.assertTrue(math.isfinite(demand) and 0 <= demand <= 1)
                if count:
                    self.assertGreater(power, 0)
                    self.assertGreater(demand, 0)
                else:
                    self.assertEqual(power, 0)
                force = [float(row[k]) for k in ("fx_N", "fy_N", "fz_N")]
                torque = [float(row[k]) for k in ("tx_Nm", "ty_Nm", "tz_Nm")]
                if count == 0 and float(row["drive_power"]) == 0:
                    self.assertTrue(all(v == 0 for v in force + torque),
                                    "no physical thrust may exist with all actuators off")
                if all(v == 0 for v in force + torque):
                    self.assertEqual((count, power), (0, 0))
                    idle += 1
                # This unchanged fixture brakes along the unrotated forward axis.
                if force[2] > 1:
                    # These are now actual impulses, not a smooth desired wrench:
                    # braking thrust cannot exist while the corresponding jets are off.
                    self.assertGreaterEqual(count, 2)
                    self.assertGreater(power, 0)
                    active += 1
                self.assertEqual(row["gpu_ready"], "1")
            self.assertGreater(active, 0)
            self.assertGreater(idle, 0)

    def test_accepted_asset_prefixes_images_lights_and_rear_exhaust_are_exact(self):
        original = os.environ.get("NGEN_RCS_BASE")
        if not original:
            self.skipTest("set immutable accepted base for preservation comparison")

        def sections(path):
            self.assertFalse(path.is_symlink())
            self.assertLessEqual(path.stat().st_size, 256 * 1024**2)
            raw = path.read_bytes()
            self.assertEqual(raw[:8], b"NGASSET\0")
            result = {}
            for i in range(13):
                kind, stride, count, zero, offset, size = struct.unpack_from("<4I2Q", raw, 64 + i * 32)
                self.assertEqual((kind, zero, count * stride), (i + 1, 0, size))
                self.assertLessEqual(offset + size, len(raw))
                result[kind] = memoryview(raw)[offset:offset + size]
            return result

        before, after = sections(Path(original)), sections(self.asset)
        for key in (5, 6, 7, 8, 10, 11):
            self.assertEqual(before[key], after[key], "frozen texture/light section changed")
        for key in (2, 3, 4, 9, 12):
            self.assertEqual(before[key], after[key][:len(before[key])], "accepted prefix changed")
        # Only forward socket positions/normals/tangents may change, under the
        # owner-approved re-aim. The exact vertex whitelist and all 32 unchanged
        # pivots are independently checked against the old rig by rcs_visibility.


if __name__ == "__main__":
    unittest.main()
