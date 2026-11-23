"""Bounded scene authoring for an owned Linux project; no runtime or tool execution."""

from contextlib import contextmanager, suppress
import fcntl
import json
import os
import re
import secrets
import stat
import struct


SCENE = "ngen-scene.json"
MAX_BYTES = 256 * 1024
MAX_ENTITIES = 64
MAX_ASSET_BYTES = 16 * 1024**2
MAX_ASSETS = 8
MAX_HISTORY = 8
MAX_REVISION = (1 << 63) - 1


class Busy(ValueError):
    pass


class StaleRevision(ValueError):
    pass


@contextmanager
def locked(directory, mode):
    # A separate open description is required for flock to serialize callers
    # that share the launcher's directory descriptor.
    fd = os.open(".", os.O_RDONLY | os.O_DIRECTORY, dir_fd=directory)
    try:
        try:
            fcntl.flock(fd, mode | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise Busy("scene is busy") from error
        yield fd
    finally:
        os.close(fd)


def read_file(directory, name, limit):
    fd = os.open(name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=directory)
    with os.fdopen(fd, "rb") as stream:
        entry = os.fstat(stream.fileno())
        if not stat.S_ISREG(entry.st_mode) or not 0 < entry.st_size <= limit:
            raise ValueError(f"{name}: expected a nonempty regular file of at most {limit} bytes")
        if entry.st_nlink != 1:
            raise ValueError(f"{name}: hard-linked files are not admitted")
        data = stream.read(limit + 1)
        if len(data) != entry.st_size:
            raise ValueError(f"{name}: file size changed during validation")
        return data


def unique_members(pairs):
    record = dict(pairs)
    if len(record) != len(pairs):
        raise ValueError("duplicate JSON member")
    return record


def decode(data):
    try:
        return json.loads(data.decode("utf-8"), object_pairs_hook=unique_members)
    except (UnicodeError, ValueError, RecursionError) as error:
        raise ValueError("invalid scene JSON") from error


def fields(value, required, optional=()):
    return isinstance(value, dict) and required <= value.keys() <= required.union(optional)


def scalar(value, low, high):
    # The comparisons reject NaN and infinities without converting huge integers.
    return type(value) in (int, float) and low <= value <= high


def vector(value, count, low, high):
    return isinstance(value, list) and len(value) == count and all(scalar(x, low, high) for x in value)


def entity_id(value):
    return isinstance(value, str) and re.fullmatch(r"[a-z][a-z0-9-]{0,39}", value) is not None


def asset_name(value):
    return isinstance(value, str) and (
        value == "marker.glb" or re.fullmatch(r"asset-[a-z][a-z0-9-]{0,39}\.glb", value) is not None
    )


def validate_entity(entity):
    if not fields(entity, {"transform"}, {"asset", "camera", "light", "material"}):
        raise ValueError("expected transform and optional asset, camera, light or material")
    transform = entity["transform"]
    if not fields(transform, {"position_m", "rotation_xyzw", "scale"}):
        raise ValueError("invalid transform fields")
    position, rotation, scale = (transform[k] for k in ("position_m", "rotation_xyzw", "scale"))
    if not (vector(position, 3, -8192, 8192) and vector(rotation, 4, -1, 1)
            and abs(sum(x * x for x in rotation) - 1.0) <= 1e-5 and vector(scale, 3, 0.001, 100)):
        raise ValueError("invalid position, unit rotation or positive scale")
    if "asset" in entity and not asset_name(entity["asset"]):
        raise ValueError("invalid asset reference")
    if "camera" in entity:
        camera = entity["camera"]
        if not (fields(camera, {"vertical_fov_deg", "near_m", "far_m"})
                and scalar(camera["vertical_fov_deg"], 1, 179) and scalar(camera["near_m"], 0.001, 8192)
                and scalar(camera["far_m"], camera["near_m"], 8192) and camera["far_m"] > camera["near_m"]):
            raise ValueError("invalid perspective camera")
    if "light" in entity:
        light = entity["light"]
        if not (fields(light, {"color", "lumens"}) and vector(light["color"], 3, 0, 1)
                and scalar(light["lumens"], 0, 100_000)):
            raise ValueError("invalid point light")
    if "material" in entity:
        material = entity["material"]
        if not ("asset" in entity and fields(material, {"base_color", "metallic", "roughness"})
                and vector(material["base_color"], 4, 0, 1) and scalar(material["metallic"], 0, 1)
                and scalar(material["roughness"], 0, 1)):
            raise ValueError("invalid material override")


def validate_entities(entities):
    if not isinstance(entities, dict) or len(entities) > MAX_ENTITIES:
        raise ValueError("invalid entity collection or entity limit exceeded")
    for key, entity in entities.items():
        if not entity_id(key):
            raise ValueError("invalid entity ID")
        validate_entity(entity)


def load(directory):
    try:
        data = read_file(directory, SCENE, MAX_BYTES)
    except FileNotFoundError:
        return {"schema": 1, "revision": 0, "entities": {}, "undo": [], "redo": []}
    document = decode(data)
    if not (fields(document, {"schema", "revision", "entities", "undo", "redo"})
            and type(document["schema"]) is int and document["schema"] == 1
            and type(document["revision"]) is int and 1 <= document["revision"] <= MAX_REVISION):
        raise ValueError("invalid scene schema or revision")
    undo, redo = document["undo"], document["redo"]
    if not (isinstance(undo, list) and isinstance(redo, list) and len(undo) + len(redo) <= MAX_HISTORY):
        raise ValueError("invalid scene history or history limit exceeded")
    for entities in [document["entities"], *undo, *redo]:
        validate_entities(entities)
    return document


def serialize(document):
    payload = (json.dumps(document, sort_keys=True, separators=(",", ":"), allow_nan=False) + "\n").encode()
    if len(payload) > MAX_BYTES:
        raise ValueError("scene document exceeds byte budget")
    return payload


def atomic_replace(directory, destination, payload):
    name = ".ngen-edit-" + secrets.token_hex(12)
    fd = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600, dir_fd=directory)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(payload)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(name, destination, src_dir_fd=directory, dst_dir_fd=directory)
    finally:
        with suppress(FileNotFoundError):
            os.unlink(name, dir_fd=directory)


def save(directory, document):
    atomic_replace(directory, SCENE, serialize(document))


def view(document):
    return {key: document[key] for key in ("schema", "revision", "entities")}


def edited(entities, data):
    if not 0 < len(data) <= MAX_BYTES:
        raise ValueError("invalid edit byte count")
    edits = decode(data)
    if not isinstance(edits, dict) or not 0 < len(edits) <= MAX_ENTITIES:
        raise ValueError("expected a bounded, nonempty edit map")
    entities = entities.copy()
    for key, entity in edits.items():
        if not entity_id(key):
            raise ValueError("invalid entity ID")
        if entity is None:
            if key not in entities:
                raise ValueError("cannot remove a missing entity")
            del entities[key]
        else:
            entities[key] = entity
    validate_entities(entities)
    return entities


def check_glb(name, data):
    # Only an embedded GLB v2 JSON/BIN pair is admitted here. Native assetc
    # still owns geometry, material, image and cooking validation.
    def invalid():
        return ValueError(f"{name}: expected an embedded GLB v2 asset")

    if len(data) < 28 or struct.unpack_from("<4sII", data) != (b"glTF", 2, len(data)):
        raise invalid()
    length, kind = struct.unpack_from("<I4s", data, 12)
    end = 20 + length
    if kind != b"JSON" or not 0 < length <= 512 * 1024 or length % 4 or end + 8 > len(data):
        raise invalid()
    size, kind = struct.unpack_from("<I4s", data, end)
    if kind != b"BIN\x00" or not size or size % 4 or end + 8 + size != len(data):
        raise invalid()
    meta = decode(data[20:end])
    if not isinstance(meta, dict):
        raise invalid()
    asset, buffers, images, meshes = (meta.get(key, []) for key in ("asset", "buffers", "images", "meshes"))
    if not isinstance(asset, dict) or asset.get("version") != "2.0":
        raise invalid()
    if not isinstance(buffers, list) or len(buffers) != 1 or not isinstance(buffers[0], dict):
        raise invalid()
    byte_length = buffers[0].get("byteLength")
    if "uri" in buffers[0] or type(byte_length) is not int or not max(1, size - 3) <= byte_length <= size:
        raise invalid()
    if not isinstance(images, list) or any(not isinstance(x, dict) or "uri" in x for x in images):
        raise invalid()
    if not isinstance(meshes, list) or not meshes:
        raise invalid()


def assets(directory, entities):
    names = sorted({item["asset"] for item in entities.values() if "asset" in item})
    if len(names) > MAX_ASSETS:
        raise ValueError("scene asset count exceeded")
    result = {}
    remaining = MAX_ASSET_BYTES
    for name in names:
        try:
            data = read_file(directory, name, remaining)
        except FileNotFoundError as error:
            raise ValueError(f"{name}: referenced asset is missing") from error
        check_glb(name, data)
        remaining -= len(data)
        result[name] = data
    return result


def authored_files(directory):
    document = load(directory)
    if not document["revision"]:
        return {}
    files = assets(directory, document["entities"])
    # Archives are source snapshots, not copies of the author's edit history.
    files[SCENE] = serialize({**view(document), "undo": [], "redo": []})
    return files


def prepare(directory, operation, expected, data):
    """Prepare a save under the caller's project lock without publishing it."""
    document = load(directory)
    revision = document["revision"]
    if revision != expected:
        raise StaleRevision("stale scene revision")
    if revision == MAX_REVISION:
        raise ValueError("scene revision exhausted")
    current, undo, redo = (document[key] for key in ("entities", "undo", "redo"))
    if operation == "edit":
        entities = edited(current, data)
        undo.append(current)
        del undo[:-MAX_HISTORY]
        redo.clear()
    else:
        source, destination = (undo, redo) if operation == "undo" else (redo, undo)
        if not source:
            raise ValueError(f"nothing to {operation}")
        entities = source.pop()
        destination.append(current)
    assets(directory, entities)
    document.update(revision=revision + 1, entities=entities)
    return current, document
