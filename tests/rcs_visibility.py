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
                banks.append((names[material], model))
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
        for name, model in banks:
            axis = model[:3, 0]
            scale = np.linalg.norm(axis)
            direction = axis / scale
            for socket in (-.6, .6):
                with self.subTest(bank=name, socket=socket):
                    origin = model[:3, :3] @ np.array([.52, socket, 0]) + model[:3, 3]
                    hit = nearest_hit(triangles, origin, direction, 5.98 * scale)
                    self.assertIsNone(hit, f"{name} socket {socket}: opaque mesh blocks exhaust after {hit:.4f} m" if hit is not None else "")


if __name__ == "__main__":
    unittest.main()
