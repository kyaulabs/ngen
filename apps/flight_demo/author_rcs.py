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


def fairing_ring(x, side, lower):
    # Exact authored side-drive fairing sections, not the enclosed pressure hull.
    sections = [(77, 7.2 * 1.1, 7.2 * .8), (91, 7.2 * 2.15, 7.2 * 1.8),
                (126, 7.2 * 2.2, 7.2 * 1.9), (148, 7.2 * 1.95, 7.2 * 1.85)]
    for a, b in zip(sections, sections[1:]):
        if a[0] <= x <= b[0]:
            t = (x - a[0]) / (b[0] - a[0])
            width, height = [a[i] * (1 - t) + b[i] * t for i in (1, 2)]
            return np.array([(x, side * 16 + width * y / 2,
                              (-7 if lower else 7) + height * z / 2)
                             for y, z in [(-.65, 1), (.65, 1), (1, .5), (1, -.5),
                                          (.65, -1), (-.65, -1), (-1, -.5), (-1, .5)]])
    raise ValueError("RCS station outside authored drive fairing")


def mount_banks():
    result = []

    def add(x, side, lower, fairing):
        facet = (5 if side < 0 else 3) if lower else (7 if side < 0 else 1)
        sample = lambda at: fairing_ring(at, side, lower) if fairing else ring(at)
        vertices = sample(x)
        p = (vertices[facet] + vertices[(facet + 1) % 8]) * .5
        v = vertices[(facet + 1) % 8] - vertices[facet]
        v /= np.linalg.norm(v)
        ahead, behind = sample(x + .01), sample(x - .01)
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
                       "surface": "drive_fairing" if fairing else "hull",
                       "position_metres": center, "force_direction": direction})

    for lower in (False, True):
        for side in (-1, 1):
            for x in (-102., -58., 113.):
                add(x, side, lower, x == 113)
    for lower in (False, True):
        for side in (-1, 1):
            add(141., side, lower, True)
    require(len(result) == 16, "RCS bank count differs")
    return result


def banks():
    result = mount_banks()
    for bank in result:
        index = bank["index"]
        side = math.copysign(1., bank["force_direction"][0])
        if index in (0, 2, 3, 5, 8, 11):
            force = np.array([side, 0, 0.])
        elif index in (6, 9):
            # Cant bowward exhaust outboard so the paired sockets cannot obstruct
            # one another. Mirrored banks cancel the lateral braking component.
            force = np.array([side * .8, 0, 1.])
        else:
            force = np.array([0, 1. if bank["lower"] else -1., 0])
        force /= np.linalg.norm(force)
        mount = bank["model"].copy()
        before = mount[:3, 0] / np.linalg.norm(mount[:3, 0])
        after = -(BODY.T @ force)
        cross = np.cross(before, after)
        skew = np.array([[0, -cross[2], cross[1]], [cross[2], 0, -cross[0]],
                         [-cross[1], cross[0], 0]])
        cosine = float(before @ after)
        require(cosine > -1 + 1e-8, "nozzle aim reversal requires an explicit frame")
        rotation = np.eye(3) + skew + skew @ skew / (1 + cosine)
        model = mount.copy()
        model[:3, :3] = rotation @ mount[:3, :3]
        model[:3, 3] += mount[:3, 0] * .3
        bank.update(mount_model=mount, aim_rotation=rotation, model=model,
                    position_metres=BODY @ model[:3, 3] + np.array([0, -2.07433414, 0]),
                    force_direction=force)
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
    # Forward thrust belongs exclusively to the accepted main rear exhaust.
    targets[:, 5] = 0
    solutions = [None] * 12
    scores = [math.inf] * 12
    for selected in itertools.combinations(range(len(rig)), 6):
        basis = matrix[:, selected]
        if abs(np.linalg.det(basis)) < 1e-10:
            continue
        weights = np.linalg.solve(basis, targets)
        for axis in range(12):
            column = weights[:, axis]
            if column.min() < -1e-7:
                continue
            full = np.zeros(len(rig))
            full[list(selected)] = np.maximum(column, 0)
            full[full < 1e-10] = 0
            if np.max(np.abs(matrix @ full - targets[:, axis])) > 1e-6:
                continue
            requested = axis // 2
            sign = 1 if axis % 2 == 0 else -1
            if np.any((full > 1e-7) & (matrix[requested] * sign < -1e-8)):
                continue
            # Minimum total impulse at fixed common bank force, not minimum peak
            # or a blend of redundant solutions that fires counteracting jets.
            score = float(full.sum())
            if score < scores[axis] - 1e-10:
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


def bake_hardware(mesh, models):
    result = Mesh()
    source = np.asarray(mesh.vertices, dtype=float)
    for model in models:
        linear = model[:3, :3]
        require(np.linalg.det(linear) > 0, "hardware frame must preserve facing")
        vertices = source.copy()
        vertices[:, :3] = source[:, :3] @ linear.T + model[:3, 3]
        normals = source[:, 3:6] @ np.linalg.inv(linear)
        normals /= np.linalg.norm(normals, axis=1)[:, None]
        tangents = source[:, 10:13] @ linear.T
        tangents -= normals * np.einsum("ij,ij->i", normals, tangents)[:, None]
        tangents /= np.linalg.norm(tangents, axis=1)[:, None]
        vertices[:, 3:6], vertices[:, 10:13] = normals, tangents
        first = len(result.vertices)
        result.vertices.extend(vertices.tolist())
        result.indices.extend(first + i for i in mesh.indices)
    return result


def aimed_mesh(mesh, bank):
    result = bake_hardware(mesh, [bank["mount_model"]])
    vertices = np.asarray(result.vertices)
    source = np.asarray(mesh.vertices)
    pivot = np.zeros((len(source), 3))
    pivot[:, 0] = .3
    pivot[:, 1] = np.where(source[:, 1] < 0, -.6, .6)
    mount, rotation = bank["mount_model"], bank["aim_rotation"]
    pivot = pivot @ mount[:3, :3].T + mount[:3, 3]
    vertices[:, :3] = pivot + (vertices[:, :3] - pivot) @ rotation.T
    vertices[:, 3:6] = vertices[:, 3:6] @ rotation.T
    vertices[:, 10:13] = vertices[:, 10:13] @ rotation.T
    result.vertices = vertices.tolist()
    return result


def combine_meshes(meshes):
    result = Mesh()
    for mesh in meshes:
        first = len(result.vertices)
        result.vertices.extend(mesh.vertices)
        result.indices.extend(first + i for i in mesh.indices)
    return result


def reaim_original_sockets(parts, rig):
    vertices = np.frombuffer(parts[1], dtype="<f4").reshape(-1, 14).copy()
    indices = np.frombuffer(parts[2], dtype="<u4")
    touched = np.zeros(len(vertices), dtype=bool)
    for bank in rig:
        if bank["index"] not in (0, 1, 3, 4):
            continue
        mount, rotation = bank["mount_model"], bank["aim_rotation"]
        inverse_mount = np.linalg.inv(mount)
        for offset in range(0, len(parts[9]), 72):
            primitive = struct.unpack_from("<I", parts[9], offset)[0]
            first, count, material, levels = struct.unpack_from("<4I", parts[3], primitive * 80)
            if material not in (3, 4):
                continue
            instance = np.array(struct.unpack_from("<16f", parts[9], offset + 8)).reshape(4, 4).T
            linear = instance[:3, :3]
            data = vertices[first:first + count]
            world = data[:, :3] @ linear.T + instance[:3, 3]
            local = world @ inverse_mount[:3, :3].T + inverse_mount[:3, 3]
            selected = np.zeros(count, dtype=bool)
            for socket in (-.6, .6):
                selected |= ((local[:, 0] >= .174) & (local[:, 0] <= .491) &
                             (np.linalg.norm(local[:, 1:] - [socket, 0], axis=1) < .46))
            if not selected.any():
                continue
            require(not touched[first:first + count][selected].any(), "RCS vertices selected twice")
            for level in range(levels):
                begin, size = struct.unpack_from("<2I", parts[3], primitive * 80 + 40 + level * 12)
                faces = selected[indices[begin:begin + size].reshape(-1, 3)]
                require(np.all(faces.all(axis=1) | ~faces.any(axis=1)), "RCS selection cuts a mesh face")
            pivot = np.zeros((int(selected.sum()), 3))
            pivot[:, 0] = .3
            pivot[:, 1] = np.where(local[selected, 1] < 0, -.6, .6)
            pivot = pivot @ mount[:3, :3].T + mount[:3, 3]
            transform = np.linalg.inv(linear) @ rotation @ linear
            changed = pivot + (world[selected] - pivot) @ rotation.T
            data[selected, :3] = (changed - instance[:3, 3]) @ np.linalg.inv(linear).T
            normals = data[selected, 3:6] @ np.linalg.inv(transform)
            normals /= np.linalg.norm(normals, axis=1)[:, None]
            tangents = data[selected, 10:13] @ transform.T
            tangents -= normals * np.einsum("ij,ij->i", normals, tangents)[:, None]
            tangents /= np.linalg.norm(tangents, axis=1)[:, None]
            data[selected, 3:6], data[selected, 10:13] = normals, tangents
            touched[first:first + count] |= selected
    require(touched.any(), "original forward socket geometry was not found")
    parts[1] = vertices.tobytes()
    return int(touched.sum())


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
    changed_socket_vertices = reaim_original_sockets(parts, rig)
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

    plate, sleeve, throat = hardware()
    plate_id = append_mesh(plate, 2)[0]
    for bank in rig:
        index = bank["index"]
        model = bank["model"].T.reshape(-1)
        if bank["lower"] and index < 12:
            parts[9] += struct.pack("<2I16f", plate_id, 1024 + index,
                                    *bank["mount_model"].T.reshape(-1))
            for mesh, material in ((sleeve, 3), (throat, 4)):
                primitive = append_mesh(aimed_mesh(mesh, bank), material)[0]
                parts[9] += struct.pack("<2I16f", primitive, 1024 + index, *np.eye(4).T.reshape(-1))
        name = f"M3_RCS_{index:02d}".encode()
        material = bytearray(original[4][13 * 128:14 * 128])
        struct.pack_into("<2I", material, 100, len(parts[12]), len(name))
        parts[12] += name
        parts[4] += material
        local_plume = bake_hardware(aimed_mesh(plume(), bank), [np.linalg.inv(bank["model"])])
        primitive = append_mesh(local_plume, 17 + index)[0]
        parts[9] += struct.pack("<2I16f", primitive, 1024 + index, *model)
    # Group added hardware by existing material, preserving the draw/triangle limits.
    added = [b for b in rig
             if b["index"] >= 12 or (not b["lower"] and b["surface"] == "drive_fairing")]
    for mesh, material in ((plate, 2), (sleeve, 3), (throat, 4)):
        meshes = [bake_hardware(mesh, [b["mount_model"]]) if material == 2 else aimed_mesh(mesh, b)
                  for b in added]
        primitive = append_mesh(combine_meshes(meshes), material)[0]
        parts[9] += struct.pack("<2I16f", primitive, 2048 + material, *np.eye(4).T.reshape(-1))
    for key in (5, 6, 7, 8, 10, 11):
        require(parts[key] == original[key], "frozen texture/light section changed")
    for key in (2, 3, 4, 9, 12):
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
               "changed_original_socket_vertices": changed_socket_vertices,
               "scope": "Preserve all 32 approved mount locations. Re-aim sockets and exhaust for selective turn/coast/arrest couples; minimum-total-impulse allocation, no forced bank participation. Change original vertices only at the four forward upper socket pairs; preserve hull, rear exhaust, other original records, textures and lights."}
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
            require(runtime["instances"] == 247 and runtime["materials"] == 33,
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
