"""Opt-in exact cooked-mesh RCS clearance regression; uses existing offline NumPy.

Set NGEN_FLIGHT_PACKAGE to the private candidate. This reads the rendered LOD 2
opaque triangles, not contact proxies, source-name guesses or authoring metadata.
No private geometry or image bytes are printed.
"""
import os
from pathlib import Path
import struct
import unittest
import zlib

import numpy as np


def scene_parts(path):
    if path.is_symlink() or not path.is_file() or path.stat().st_size > 256 * 1024**2:
        raise ValueError("bounded regular private package required")
    raw = path.read_bytes()
    if raw[:8] != b"NGASSET\0" or len(raw) < 480:
        raise ValueError("invalid package header")
    if zlib.crc32(memoryview(raw)[64:]) & 0xffffffff != struct.unpack_from("<I", raw, 24)[0]:
        raise ValueError("package checksum mismatch")
    parts = {}
    for i in range(13):
        kind, stride, count, zero, offset, size = struct.unpack_from("<4I2Q", raw, 64 + i * 32)
        if kind != i + 1 or zero or count * stride != size or offset + size > len(raw):
            raise ValueError("package section range mismatch")
        parts[kind] = memoryview(raw)[offset:offset + size]
    return parts


def rendered_geometry(parts):
    vertices = np.frombuffer(parts[1], dtype="<f4").reshape(-1, 14)[:, :3]
    indices = np.frombuffer(parts[2], dtype="<u4")
    names, alpha = [], []
    for offset in range(0, len(parts[4]), 128):
        start, size = struct.unpack_from("<2I", parts[4], offset + 100)
        names.append(bytes(parts[12][start:start + size]).decode("utf-8"))
        alpha.append(struct.unpack_from("<I", parts[4], offset + 52)[0])
    triangles, banks = [], []
    for offset in range(0, len(parts[9]), 72):
        primitive = struct.unpack_from("<I", parts[9], offset)[0]
        first, count, material, levels = struct.unpack_from("<4I", parts[3], primitive * 80)
        model = np.array(struct.unpack_from("<16f", parts[9], offset + 8)).reshape(4, 4).T
        if material != 0xffffffff and alpha[material] == 2:
            if names[material].startswith("M3_RCS_"):
                local = vertices[first:first + count].astype(float)
                world = local @ model[:3, :3].T + model[:3, 3]
                direction = model[:3, 0] / np.linalg.norm(model[:3, 0])
                exits = []
                for start in (0, count // 2):
                    quad = world[start:start + 4]
                    # The two leading corners of the first crossed-card quad
                    # average to the actual rendered socket centerline start.
                    leading = np.argsort(quad @ direction)[:2]
                    exits.append(quad[leading].mean(axis=0))
                banks.append((names[material], model, exits))
            continue
        begin, size = struct.unpack_from("<2I", parts[3], primitive * 80 + 40 + min(2, levels - 1) * 12)
        positions = vertices[first:first + count].astype(float)
        world = positions @ model[:3, :3].T + model[:3, 3]
        triangles.append(world[indices[begin:begin + size].reshape(-1, 3)])
    return np.concatenate(triangles), banks


def nearest_hit(triangles, origin, direction, maximum):
    a = triangles[:, 0]
    e1, e2 = triangles[:, 1] - a, triangles[:, 2] - a
    h = np.cross(direction, e2)
    determinant = np.einsum("ij,ij->i", e1, h)
    usable = np.abs(determinant) > 1e-10
    inverse = np.zeros_like(determinant)
    inverse[usable] = 1 / determinant[usable]
    relative = origin - a
    u = inverse * np.einsum("ij,ij->i", relative, h)
    q = np.cross(relative, e1)
    v = inverse * (q @ direction)
    distance = inverse * np.einsum("ij,ij->i", e2, q)
    hits = usable & (u >= -1e-7) & (v >= -1e-7) & (u + v <= 1 + 1e-7) & (distance > .001) & (distance < maximum)
    return float(distance[hits].min()) if hits.any() else None


class RcsVisibility(unittest.TestCase):
    def test_every_socket_has_an_unobstructed_opaque_mesh_exhaust_path(self):
        asset = os.environ.get("NGEN_FLIGHT_PACKAGE")
        if not asset:
            self.skipTest("set private NGEN_FLIGHT_PACKAGE")
        triangles, banks = rendered_geometry(scene_parts(Path(asset)))
        self.assertGreater(len(banks), 0)
        for name, model, exits in banks:
            axis = model[:3, 0]
            scale = np.linalg.norm(axis)
            direction = axis / scale
            for socket, origin in enumerate(exits):
                with self.subTest(bank=name, socket=socket):
                    hit = nearest_hit(triangles, origin, direction, 5.98 * scale)
                    self.assertIsNone(hit, f"{name} socket {socket}: opaque mesh blocks exhaust after {hit:.4f} m" if hit is not None else "")

    def test_all_32_socket_pivots_stay_at_the_approved_locations(self):
        asset, approved = os.environ.get("NGEN_FLIGHT_PACKAGE"), os.environ.get("NGEN_RCS_APPROVED")
        if not asset or not approved:
            self.skipTest("set candidate and NGEN_RCS_APPROVED 32-jet package")
        _, before = rendered_geometry(scene_parts(Path(approved)))
        _, after = rendered_geometry(scene_parts(Path(asset)))
        self.assertEqual(len(before), 16)
        self.assertEqual(len(after), 16)
        before = {name: (model, exits) for name, model, exits in before}
        for name, model, exits in after:
            old, previous = before[name]
            # Both rigs pivot at the original socket body center, x = 0.3;
            # the first plume ring is 0.21 farther along the nozzle axis.
            for old_exit, new_exit in zip(previous, exits):
                a, b = old_exit - old[:3, 0] * .21, new_exit - model[:3, 0] * .21
                with self.subTest(bank=name):
                    self.assertLess(float(np.max(np.abs(a - b))), 3e-5)

    def test_only_original_forward_socket_vertices_change(self):
        candidate, base, approved = (os.environ.get(name) for name in
                                    ("NGEN_FLIGHT_PACKAGE", "NGEN_RCS_BASE", "NGEN_RCS_APPROVED"))
        if not all((candidate, base, approved)):
            self.skipTest("set candidate, immutable base and approved rig")
        original, after = scene_parts(Path(base)), scene_parts(Path(candidate))
        old = np.frombuffer(original[1], dtype="<f4").reshape(-1, 14)
        new = np.frombuffer(after[1], dtype="<f4").reshape(-1, 14)[:len(old)]
        changed = np.any(old.view("<u4") != new.view("<u4"), axis=1)
        self.assertGreater(int(changed.sum()), 0)
        self.assertTrue(np.array_equal(old[:, 6:10].view("<u4"), new[:, 6:10].view("<u4")))
        self.assertTrue(np.array_equal(old[:, 13].view("<u4"), new[:, 13].view("<u4")))
        _, banks = rendered_geometry(scene_parts(Path(approved)))
        mounts = [np.linalg.inv(model) for name, model, _ in banks
                  if name in ("M3_RCS_00", "M3_RCS_01", "M3_RCS_03", "M3_RCS_04")]
        allowed = np.zeros(len(old), dtype=bool)
        for offset in range(0, len(original[9]), 72):
            primitive = struct.unpack_from("<I", original[9], offset)[0]
            first, count, material, _ = struct.unpack_from("<4I", original[3], primitive * 80)
            if material not in (3, 4):
                continue
            model = np.array(struct.unpack_from("<16f", original[9], offset + 8)).reshape(4, 4).T
            world = old[first:first + count, :3] @ model[:3, :3].T + model[:3, 3]
            for inverse in mounts:
                local = world @ inverse[:3, :3].T + inverse[:3, 3]
                for socket in (-.6, .6):
                    allowed[first:first + count] |= ((local[:, 0] >= .174) & (local[:, 0] <= .491) &
                        (np.linalg.norm(local[:, 1:] - [socket, 0], axis=1) < .46))
        self.assertFalse(np.any(changed & ~allowed), "non-RCS accepted vertex changed")


if __name__ == "__main__":
    unittest.main()
