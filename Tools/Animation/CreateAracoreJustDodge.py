"""Author Aracore's backward dodge in its existing VMDL package (Windows/Python).

Run from any directory. Existing sections and clips are preserved byte for byte.
The root eases out of a quick push, with restrained leg loading and recovery.
"""

import ctypes as ct
import math
from pathlib import Path
import re
import struct


ROOT = Path(__file__).resolve().parents[2]
MODEL = ROOT / "Resources/Model/Aracore.vmdl"
DURATION = 0.7
DISTANCE = 240.0  # Model units, along local -Z; scaled by the runtime model scale.
LEG_CHAINS = (
    ("Box09", "Box10", "Box11"), ("Box20", "Box18", "Box19"),
    ("Box25", "Box22", "Box23"), ("Box26", "Box21", "Box24"),
    ("Box31", "Box28", "Box29"), ("Box35", "Box32", "Box36"),
    ("Box37", "Box33", "Box34"), ("Box38", "Box27", "Box30"),
)


class Reader:
    def __init__(self, data):
        self.data = data
        self.pos = 0

    def number(self, fmt="Q"):
        result = struct.unpack_from("<" + fmt, self.data, self.pos)[0]
        self.pos += struct.calcsize("<" + fmt)
        return result

    def blob(self):
        size = self.number()
        result = self.data[self.pos:self.pos + size]
        self.pos += size
        assert len(result) == size
        return result

    def skip_vector(self, stride):
        count = self.number()
        self.pos += count * stride


def blob(data):
    return struct.pack("<Q", len(data)) + data


def codec(data, decompress=False, output_size=0):
    cabinet = ct.WinDLL("cabinet", use_last_error=True)
    handle = ct.c_void_p()
    size = ct.c_size_t()
    create = cabinet.CreateDecompressor if decompress else cabinet.CreateCompressor
    close = cabinet.CloseDecompressor if decompress else cabinet.CloseCompressor
    operation = cabinet.Decompress if decompress else cabinet.Compress
    operation.argtypes = [ct.c_void_p, ct.c_void_p, ct.c_size_t,
                         ct.c_void_p, ct.c_size_t, ct.POINTER(ct.c_size_t)]
    assert create(4, None, ct.byref(handle)), ct.get_last_error()
    try:
        if not decompress:
            operation(handle, data, len(data), None, 0, ct.byref(size))
            assert ct.get_last_error() == 122 and size.value
            output_size = size.value
        output = ct.create_string_buffer(output_size)
        assert operation(handle, data, len(data), output, output_size,
                         ct.byref(size)), ct.get_last_error()
        return output.raw[:size.value]
    finally:
        close(handle)


def unpack_model(data):
    assert data[:8] == b"VMDLCMP\0"
    version, size, compressed = struct.unpack_from("<IQQ", data, 8)
    assert version == 13 and len(data) == compressed + 28
    raw = codec(data[28:], True, size)
    assert len(raw) == size
    reader = Reader(raw)
    files = [(reader.blob(), reader.blob()) for _ in range(reader.number())]
    assert reader.pos == len(raw)
    return files


def read_clips(data):
    reader = Reader(data)
    names = []
    for _ in range(reader.number()):
        names.append(reader.blob().decode())
        reader.pos += 44  # Parent index, position, quaternion, scale.
    for _ in range(reader.number()):
        for _ in range(11):
            reader.blob()  # Six names and five DDS byte arrays.
        reader.pos += 52
    for _ in range(reader.number()):
        for stride in (80, 4, 68):
            reader.skip_vector(stride)  # Vertices, indices, bones.
        reader.pos += 8
    count_offset = reader.pos
    clips = []
    for _ in range(reader.number()):
        start = reader.pos
        name = reader.blob().decode()
        duration = reader.number("f")
        tracks = []
        for _ in range(reader.number()):
            channels = []
            for dim in (3, 4, 3):
                count = reader.number()
                fmt = "<" + "f" * (dim + 1)
                stride = (dim + 1) * 4
                channels.append([struct.unpack_from(fmt, data, reader.pos + i * stride)
                                 for i in range(count)])
                reader.pos += count * stride
            tracks.append(channels)
        for _ in range(reader.number()):
            reader.blob()
            reader.pos += 24
        clips.append((name, duration, tracks, data[start:reader.pos]))
    assert reader.pos == len(data)
    return names, clips, count_offset


def lerp(a, b, weight, quaternion=False):
    if quaternion and sum(x * y for x, y in zip(a, b)) < 0:
        b = [-x for x in b]
    value = [x + (y - x) * weight for x, y in zip(a, b)]
    if quaternion:
        length = math.sqrt(sum(x * x for x in value))
        value = [x / length for x in value]
    return value


def sample(keys, time, quaternion=False):
    if time <= keys[0][0]:
        return list(keys[0][1:])
    for a, b in zip(keys, keys[1:]):
        if time <= b[0]:
            return lerp(a[1:], b[1:], (time - a[0]) / (b[0] - a[0]), quaternion)
    return list(keys[-1][1:])


def smooth(value):
    value = max(0.0, min(value, 1.0))
    return value * value * (3.0 - 2.0 * value)


def load_weight(time):
    return smooth(time / .055) * (1 - smooth((time - .09) / .33))


def brake_weight(time):
    return smooth((time - .22) / .16) * (1 - smooth((time - .40) / .30))


def slide_positions():
    # Integrate a brief push followed by a long, continuously fading velocity.
    # Both endpoint velocities are zero, without a stationary tail in the clip.
    velocities = []
    for frame in range(85):
        time = DURATION * frame / 84
        velocities.append(smooth(time / .045) * (1 - time / DURATION) ** 3)
    positions = [0.0]
    for a, b in zip(velocities, velocities[1:]):
        positions.append(positions[-1] + (a + b) * .5)
    return [value / positions[-1] for value in positions]


def make_clip(names, clips):
    idle = next(c for c in clips if c[0] == "idle")[2]
    jump = next(c for c in clips if c[0] == "jump")[2]
    positions = slide_positions()
    leg_groups = {name: leg % 4 for leg, chain in enumerate(LEG_CHAINS) for name in chain}
    tracks = [[[], [], []] for _ in names]
    for frame in range(85):
        time = DURATION * frame / 84
        load = load_weight(time)
        brake = brake_weight(time)
        for node, channels in enumerate(tracks):
            for channel in range(3):
                base = sample(idle[node][channel], 0, channel == 1)
                value = base[:]
                if channel == 1 and names[node] not in ("Box01", "Armature"):
                    # A small crouch and softer braking pose show the force
                    # without cycling the feet or tucking them into a jump.
                    loaded = sample(jump[node][channel], .25, True)
                    settled = sample(jump[node][channel], 1.17, True)
                    value = lerp(base, loaded, .28 * load, True)
                    value = lerp(value, settled, .12 * brake, True)
                    if names[node] in leg_groups:
                        # Stronger leg-only poses: load, push through the joints,
                        # then absorb the retreat. Paired legs share their timing;
                        # a slight front-to-back delay keeps the force readable.
                        leg_time = time - .012 * leg_groups[names[node]]
                        push = smooth((leg_time - .06) / .08) * (
                            1 - smooth((leg_time - .17) / .22))
                        extended = sample(jump[node][channel], .5, True)
                        braking = sample(jump[node][channel], 1.0, True)
                        value = lerp(base, loaded, .72 * load_weight(leg_time), True)
                        value = lerp(value, extended, .38 * push, True)
                        value = lerp(value, braking, .45 * brake_weight(time), True)
                if names[node] == "Box01" and channel == 0:
                    # Never reset Z at the end: root motion must retain the retreat.
                    value = base[:]
                    value[2] -= DISTANCE * positions[frame]
                    value[1] -= 5.0 * load + 2.0 * brake
                channels[channel].append((time, *value))
    result = blob(b"just_dodge") + struct.pack("<fQ", DURATION, len(tracks))
    for channels in tracks:
        for channel in channels:
            result += struct.pack("<Q", len(channel))
            result += b"".join(struct.pack("<" + "f" * len(k), *k) for k in channel)
    result += struct.pack("<Q", 0)
    return result


def paint_foot_weights(data):
    # The editor vector sits inside model.vmdldata. Locate its first existing
    # named track, then validate the whole vector before replacing that span.
    match = re.search(rb"[a-zA-Z_0-9]+::FootWeight:[0-9]+", data)
    assert match, "Expected existing editor FootWeight tracks"
    start = match.start() - 16
    reader = Reader(data)
    reader.pos = start
    count = reader.number()
    assert 0 < count < 10000
    tracks = []
    for _ in range(count):
        track_start = reader.pos
        key = reader.blob()
        assert re.fullmatch(rb"[a-zA-Z_0-9]+::FootWeight:[0-9]+", key)
        rate = reader.number("f")
        assert 0 < rate <= 1000
        reader.skip_vector(4)
        assert reader.pos <= len(data)
        if not key.startswith(b"just_dodge::FootWeight:"):
            tracks.append(data[track_start:reader.pos])
    # Ease the IK correction during the fast push so it does not overpower
    # the leg loading. Restore full terrain contact gradually during recovery.
    for foot in range(8):
        key = f"just_dodge::FootWeight:{foot}".encode()
        samples = math.ceil(DURATION * 120) + 1
        weights = [1.0 - .4 * load_weight(DURATION * i / (samples - 1))
                   for i in range(samples)]
        tracks.append(blob(key) + struct.pack("<fQ", 120.0, samples) +
                      struct.pack("<" + "f" * samples, *weights))
    return (data[:start] + struct.pack("<Q", len(tracks)) +
            b"".join(tracks) + data[reader.pos:])


def main():
    files = unpack_model(MODEL.read_bytes())
    data = dict(files)[b"model.glbcache"]
    names, clips, offset = read_clips(data)
    authored = make_clip(names, clips)
    originals = [c[3] for c in clips if c[0] != "just_dodge"]
    updated = data[:offset] + struct.pack("<Q", len(originals) + 1)
    updated += b"".join(originals) + authored
    _, verified, _ = read_clips(updated)
    assert all(a[3] == b[3] for a, b in zip(clips, verified) if a[0] != "just_dodge")
    files = [(name, updated if name == b"model.glbcache" else
              paint_foot_weights(value) if name == b"model.vmdldata" else value)
             for name, value in files]
    raw = struct.pack("<Q", len(files)) + b"".join(blob(k) + blob(v) for k, v in files)
    compressed = codec(raw)
    output = b"VMDLCMP\0" + struct.pack("<IQQ", 13, len(raw), len(compressed)) + compressed
    assert unpack_model(output) == files
    MODEL.write_bytes(output)
    print(f"just_dodge: clip {len(originals)}, {DURATION}s, local -Z {DISTANCE} units")
    print("Verified original clips; painted all eight FootWeight rows at 120 Hz.")


if __name__ == "__main__":
    main()
