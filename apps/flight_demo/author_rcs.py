"""Author the private frigate RCS derivative from the accepted immutable package.

Trusted local art inputs only. Reuses the existing assembler and installed NumPy;
no runtime decoder, vendor recook, network service or original-file modification.
"""
import argparse
import hashlib
import importlib.util
import itertools
import json
import math
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
BASE_SHA256 = "a02d5438ad32d9c8fb917ce847d5a822688ca5b51d69125f6da71cf2ba2116ed"
STATIONS = [(-160, 2.2, 1, -2), (-145, 6, 3, -4), (-120, 11, 4, -5.5),
            (-90, 15.5, 5, -6.8), (-50, 20, 6.5, -8), (0, 25, 8, -10),
            (40, 26, 9, -11), (80, 28, 10, -12), (120, 27, 9, -12), (146, 22, 8, -10)]
SCALE = np.array([1, .9505835771560669, .981468915939331])
BODY = np.array([[0, 0, -1], [0, 1, 0], [1, 0, 0.]])
FORCE = np.array([24000000., 24000000., 72000000.])
TORQUE = np.array([1.72e11, 1.76e11, 1.17e10])


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def ring(x):
    dimensions = None
    for a, b in zip(STATIONS, STATIONS[1:]):
        if a[0] <= x <= b[0]:
            t = (x - a[0]) / (b[0] - a[0])
            dimensions = [a[i] * (1 - t) + b[i] * t for i in range(1, 4)]
            break
    require(dimensions is not None, "RCS station outside authored hull")
    width, top, bottom = dimensions
    center, half = (top + bottom) / 2, (top - bottom) / 2
    return np.array([(x, width * y, center + half * z)
                     for y, z in [(-.65, 1), (.65, 1), (1, .46), (1, -.46),
                                  (.65, -1), (-.65, -1), (-1, -.46), (-1, .46)]])


def banks():
    result = []
    for lower in (False, True):
        for side in (-1, 1):
            facet = (5 if side < 0 else 3) if lower else (7 if side < 0 else 1)
            for x in (-102., -58., 113.):
                vertices = ring(x)
                p = (vertices[facet] + vertices[(facet + 1) % 8]) * .5
                v = vertices[(facet + 1) % 8] - vertices[facet]
                v /= np.linalg.norm(v)
                ahead, behind = ring(x + .01), ring(x - .01)
                u = ((ahead[facet] + ahead[(facet + 1) % 8]) -
                     (behind[facet] + behind[(facet + 1) % 8])) * .5
                u /= np.linalg.norm(u)
                n = np.cross(u, v)
                n /= np.linalg.norm(n)
                p += n * .75
                # Blender X/Y/Z -> glTF X/Z/-Y, after the verified export scale.
                convert = lambda a: (a * SCALE)[[0, 2, 1]] * np.array([1, 1, -1])
                model = np.eye(4)
                model[:3, :3] = np.column_stack([convert(n), convert(u), convert(v)])
                model[:3, 3] = convert(p)
                center = BODY @ model[:3, 3] + np.array([0, -2.07433414, 0])
                direction = -(BODY @ model[:3, 0])
                direction /= np.linalg.norm(direction)
                result.append({"index": len(result), "lower": lower, "model": model,
                               "position_metres": center, "force_direction": direction})
    require(len(result) == 12, "RCS bank count differs")
    return result


def mixing(rig):
    matrix = np.column_stack([np.concatenate([b["force_direction"],
                              np.cross(b["position_metres"], b["force_direction"]) / 100])
                              for b in rig])
    targets = np.zeros((6, 12))
    for axis in range(6):
        magnitude = FORCE[axis] / 1e9 if axis < 3 else TORQUE[axis - 3] / 1e11
        targets[axis, axis * 2] = magnitude
        targets[axis, axis * 2 + 1] = -magnitude
    solutions = [None] * 12
    scores = [math.inf] * 12
    for selected in itertools.combinations(range(12), 6):
        basis = matrix[:, selected]
        if abs(np.linalg.det(basis)) < 1e-10:
            continue
        weights = np.linalg.solve(basis, targets)
        for axis in range(12):
            column = weights[:, axis]
            if column.min() < -1e-7:
                continue
            full = np.zeros(12)
            full[list(selected)] = np.maximum(column, 0)
            if np.max(np.abs(matrix @ full - targets[:, axis])) > 1e-6:
                continue
            score = float(full.max())
            if score < scores[axis]:
                solutions[axis], scores[axis] = full, score
    require(all(s is not None for s in solutions), "authored RCS cone cannot span the actuator axes")
    raw = np.column_stack(solutions)
    nominal = max(1., float(raw.max()))
    weights = raw / nominal
    require(np.max(np.abs(matrix @ weights - targets / nominal)) < 1e-6,
            "RCS mixing residual exceeds profile")
    return weights, nominal * 1e9, matrix


class Mesh:
    def __init__(self):
        self.vertices, self.indices = [], []

    def polygon(self, points, normal, uv=None):
        p = np.asarray(points, dtype=float)
        n = np.asarray(normal, dtype=float)
        n /= np.linalg.norm(n)
        if np.dot(np.cross(p[1] - p[0], p[2] - p[0]), n) < 0:
            p = p[::-1]
        tangent = p[1] - p[0]
        tangent -= n * np.dot(n, tangent)
        tangent /= np.linalg.norm(tangent)
        first = len(self.vertices)
        for i, point in enumerate(p):
            tex = uv[i] if uv is not None else (float(i % 2), float(i // 2 % 2))
            self.vertices.append([*point, *n, *tex, *tex, *tangent, 1])
        for i in range(1, len(p) - 1):
            self.indices.extend([first, first + i, first + i + 1])


def hardware():
    plate, sleeve, throat = Mesh(), Mesh(), Mesh()
    contour = [(-1.25, -1.2), (1.25, -1.2), (1.65, -.8), (1.65, .8),
               (1.25, 1.2), (-1.25, 1.2), (-1.65, .8), (-1.65, -.8)]
    for x, normal in ((-.25, [-1, 0, 0]), (.25, [1, 0, 0])):
        plate.polygon([[x, y, z] for y, z in contour], normal)
    for a, b in zip(contour, contour[1:] + contour[:1]):
        middle = np.array([0, (a[0] + b[0]) * .5, (a[1] + b[1]) * .5])
        plate.polygon([[-.25, *a], [-.25, *b], [.25, *b], [.25, *a]], middle)
    for center in (-.6, .6):
        for segment in range(8):
            a, b = segment * math.tau / 8, (segment + 1) * math.tau / 8
            radial = lambda x, r, t: [x, center + r * math.cos(t), r * math.sin(t)]
            normal = [0, math.cos((a + b) * .5), math.sin((a + b) * .5)]
            sleeve.polygon([radial(.175, .42, a), radial(.175, .42, b),
                            radial(.425, .42, b), radial(.425, .42, a)], normal)
            sleeve.polygon([radial(.425, .42, a), radial(.425, .42, b),
                            radial(.425, .28, b), radial(.425, .28, a)], [1, 0, 0])
            sleeve.polygon([radial(.425, .28, a), radial(.425, .28, b),
                            radial(.175, .28, b), radial(.175, .28, a)], -np.asarray(normal))
        throat.polygon([[.49, center + .28 * math.cos(i * math.tau / 8),
                         .28 * math.sin(i * math.tau / 8)] for i in range(8)], [1, 0, 0])
    return plate, sleeve, throat


def plume():
    mesh = Mesh()
    profile = [(u, max(.005, (1 - u) ** 1.3 * (1 + 3 * u)))
               for u in (0., .08, .22, .42, .65, .85, 1.)]
    for socket in (-.6, .6):
        for angle in (0., math.pi / 2):
            across = np.array([0, math.cos(angle), math.sin(angle)])
            normal = np.array([0, -math.sin(angle), math.cos(angle)])
            for (u, r), (v, s) in zip(profile, profile[1:]):
                point = lambda q, radius, sign: (np.array([.51 + 6 * q, socket, 0]) +
                                                  across * (.2744 * radius * sign))
                mesh.polygon([point(u, r, -1), point(v, s, -1), point(v, s, 1), point(u, r, 1)],
                             normal, [(u, 0), (v, 0), (v, 1), (u, 1)])
    return mesh


def author(base, output, probe):
    require(base.suffix == ".ngas" and base.is_file() and not base.is_symlink(), "bounded .ngas base required")
    require(base.stat().st_size <= 256 * 1024**2 and digest(base) == BASE_SHA256, "unexpected accepted base")
    require(not output.exists() and not output.is_symlink(), "output already exists")
    raw = base.read_bytes()
    parts = {}
    for i in range(13):
        kind, stride, count, zero, offset, size = struct.unpack_from("<4I2Q", raw, 64 + i * 32)
        require(kind == i + 1 and zero == 0 and count * stride == size, "section mismatch")
        parts[kind] = raw[offset:offset + size]
    original = dict(parts)
    rig = banks()
    weights, nominal, matrix = mixing(rig)
    vertex_count, index_count = len(parts[1]) // 56, len(parts[2]) // 4

    def append_mesh(mesh, material):
        nonlocal vertex_count, index_count
        vertices = np.asarray(mesh.vertices, dtype="<f4")
        indices = np.asarray(mesh.indices, dtype="<u4")
        record = bytearray(80)
        struct.pack_into("<4I6f2If", record, 0, vertex_count, len(vertices), material, 1,
                         *vertices[:, :3].min(axis=0), *vertices[:, :3].max(axis=0),
                         index_count, len(indices), 0.)
        primitive = len(parts[3]) // 80
        parts[1] += vertices.tobytes()
        parts[2] += indices.tobytes()
        parts[3] += record
        vertex_count += len(vertices)
        index_count += len(indices)
        return primitive, bytes(record)

    hardware_ids = [append_mesh(m, material)[0] for m, material in zip(hardware(), (2, 3, 4))]
    template_id, template = append_mesh(plume(), 17)
    for bank in rig:
        index = bank["index"]
        model = bank["model"].T.reshape(-1)
        if bank["lower"]:
            for primitive in hardware_ids:
                parts[9] += struct.pack("<2I16f", primitive, 1024 + index, *model)
        name = f"M3_RCS_{index:02d}".encode()
        material = bytearray(original[4][13 * 128:14 * 128])
        struct.pack_into("<2I", material, 100, len(parts[12]), len(name))
        parts[12] += name
        parts[4] += material
        if index == 0:
            primitive = template_id
        else:
            primitive = len(parts[3]) // 80
            record = bytearray(template)
            struct.pack_into("<I", record, 8, 17 + index)
            parts[3] += record
        parts[9] += struct.pack("<2I16f", primitive, 1024 + index, *model)
    for key in (5, 6, 7, 8, 10, 11):
        require(parts[key] == original[key], "frozen texture/light section changed")
    for key in (1, 2, 3, 4, 9, 12):
        require(parts[key].startswith(original[key]), "accepted asset prefix changed")
    rendered = 8000
    for offset in range(0, len(parts[9]), 72):
        primitive = struct.unpack_from("<I", parts[9], offset)[0]
        levels = struct.unpack_from("<I", parts[3], primitive * 80 + 12)[0]
        count = struct.unpack_from("<I", parts[3], primitive * 80 + 44 + min(2, levels - 1) * 12)[0]
        rendered += count // 3
    require(rendered <= 250000 and len(parts[9]) // 72 < 256, "flight workload limit exceeded")
    profile = {"base_sha256": BASE_SHA256, "author_sha256": digest(Path(__file__)),
               "numpy_version": np.__version__, "banks": [
                   {k: (v.tolist() if isinstance(v, np.ndarray) else v) for k, v in b.items()} for b in rig],
               "mix": weights.tolist(), "nominal_bank_force_newtons": nominal,
               "mixing_matrix": matrix.tolist(), "visible_workload_triangles": rendered,
               "scope": "Append matching lower RCS hardware and 24 small original socket plumes; preserve every accepted asset prefix, rear exhaust, texture and light."}
    provenance = json.loads(parts[13])
    provenance["m3_rcs"] = profile
    parts[13] = json.dumps(provenance, sort_keys=True, separators=(",", ":")).encode()
    require(len(parts[13]) <= 65536, "provenance limit exceeded")
    spec = importlib.util.spec_from_file_location("ngen_assembler", ROOT / "tools/assetc/cook.py")
    cook = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(cook)
    output.mkdir(parents=False)
    created = []
    try:
        with tempfile.TemporaryDirectory(prefix=".rcs-", dir=output) as temporary:
            staged = Path(temporary) / "scene.ngas"
            staged.write_bytes(cook.assemble(parts))
            result = subprocess.run([str(probe), str(staged)], capture_output=True, text=True,
                                    timeout=60, check=False)
            require(result.returncode == 0 and not result.stderr, result.stdout + result.stderr)
            runtime = json.loads(result.stdout)
            require(runtime["instances"] == 240 and runtime["materials"] == 29,
                    "unexpected RCS package catalog")
            require(digest(base) == BASE_SHA256, "accepted base changed")
            manifest = {"package_sha256": digest(staged), "runtime": runtime, "m3_rcs": profile}
            with (output / "scene.ngas").open("xb") as target:
                created.append(output / "scene.ngas")
                with staged.open("rb") as stream:
                    shutil.copyfileobj(stream, target, 1024 * 1024)
            with (output / "manifest.json").open("x") as target:
                created.append(output / "manifest.json")
                target.write(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
        return manifest
    except BaseException:
        for path in created:
            path.unlink(missing_ok=True)
        try:
            output.rmdir()
        except OSError:
            pass
        raise


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--probe", type=Path, required=True)
    arguments = parser.parse_args()
    result = author(arguments.base, arguments.output, arguments.probe)
    print(json.dumps({"package": str(arguments.output / "scene.ngas"),
                      "sha256": result["package_sha256"], "runtime": result["runtime"]}, indent=2))
