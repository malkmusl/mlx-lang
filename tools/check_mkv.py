#!/usr/bin/env python3
"""Checks a Matroska file mlx-capture recorded (std.matroska): the EBML
structure (sizes that add up, the Segment's size, Info's duration, the
SeekHead pointing at Info, Tracks and Cues, the Cues at clusters), the
video track's frames (each a JPEG that decodes to the track's size, in
time order) and each sound track's samples (16-bit stereo PCM: how long,
and how loud each frequency asked about is, by Goertzel).

Usage: check_mkv.py FILE.mkv [--frames MIN] [--duration MS] [--track NAME:HZ=AMPLITUDE,...]...
       an amplitude of 0 means the frequency must be absent.
Prints what it found; exits 1 (saying why) when something is wrong.
"""
import io
import math
import struct
import sys

from PIL import Image


def fail(message):
    print(f"FAIL {message}")
    sys.exit(1)


def vint(data, at, size=False):
    first = data[at]
    length = 1
    mask = 0x80
    while length <= 8 and not first & mask:
        length += 1
        mask >>= 1
    if length > 8:
        fail(f"bad variable-length integer at {at}")
    value = first & (mask - 1) if size else first
    for index in range(1, length):
        value = (value << 8) | data[at + index]
    if size and value == (1 << (7 * length)) - 1:
        value = None
    return value, at + length


def elements(data, start, end, starts=None):
    at = start
    while at < end:
        if starts is not None:
            starts.append(at)
        element, at = vint(data, at)
        size, at = vint(data, at, True)
        if size is None or at + size > end:
            fail(f"element {element:#x} at {at} runs past its parent ({size})")
        yield element, at, size
        at += size


def unsigned(data, at, size):
    return int.from_bytes(data[at:at + size], "big")


def main():
    arguments = sys.argv[1:]
    path = arguments.pop(0)
    minimum_frames, duration, expected = 1, None, {}
    while arguments:
        option = arguments.pop(0)
        if option == "--frames":
            minimum_frames = int(arguments.pop(0))
        elif option == "--duration":
            duration = int(arguments.pop(0))
        elif option == "--track":
            name, wanted = arguments.pop(0).split(":", 1)
            expected[name] = [(float(f), float(a)) for f, a in (pair.split("=") for pair in wanted.split(","))]
    data = open(path, "rb").read()
    top = list(elements(data, 0, len(data)))
    if [e for e, _, _ in top] != [0x1A45DFA3, 0x18538067]:
        fail(f"top level: {[hex(e) for e, _, _ in top]}")
    header = {e: (a, s) for e, a, s in elements(data, top[0][1], top[0][1] + top[0][2])}
    if data[header[0x4282][0]:header[0x4282][0] + header[0x4282][1]] != b"matroska":
        fail("doc type")
    _, segment, segment_size = top[1]
    if segment + segment_size != len(data):
        fail(f"the Segment's size {segment_size} does not reach the file's end")
    element_starts = []
    children = list(elements(data, segment, segment + segment_size, element_starts))
    # SeekHead.
    seek = [c for c in children if c[0] == 0x114D9B74]
    if not seek:
        fail("no SeekHead")
    for entry, at, size in elements(data, seek[0][1], seek[0][1] + seek[0][2]):
        fields = {e: (a, s) for e, a, s in elements(data, at, at + size)}
        target = unsigned(data, *fields[0x53AB])
        place = unsigned(data, *fields[0x53AC])
        element, _ = vint(data, segment + place)
        if element != target:
            fail(f"SeekHead says {target:#x} is at {place}, there is {element:#x}")
    # Info.
    info = {}
    for element, at, size in children:
        if element == 0x1549A966:
            info = {e: (a, s) for e, a, s in elements(data, at, at + size)}
    scale = unsigned(data, *info[0x2AD7B1])
    length_ms = struct.unpack(">f", data[info[0x4489][0]:info[0x4489][0] + 4])[0] * scale / 1e6
    # Tracks.
    tracks = {}
    for element, at, size in children:
        if element == 0x1654AE6B:
            for _, entry, entry_size in elements(data, at, at + size):
                fields = {e: (a, s) for e, a, s in elements(data, entry, entry + entry_size)}
                number = unsigned(data, *fields[0xD7])
                codec = data[fields[0x86][0]:fields[0x86][0] + fields[0x86][1]].decode()
                name = data[fields[0x536E][0]:fields[0x536E][0] + fields[0x536E][1]].decode()
                track = {"codec": codec, "name": name, "blocks": [], "default": unsigned(data, *fields[0x88]) if 0x88 in fields else 1}
                if 0xE0 in fields:
                    video = {e: (a, s) for e, a, s in elements(data, fields[0xE0][0], sum(fields[0xE0]))}
                    track["size"] = (unsigned(data, *video[0xB0]), unsigned(data, *video[0xBA]))
                if 0xE1 in fields:
                    sound = {e: (a, s) for e, a, s in elements(data, fields[0xE1][0], sum(fields[0xE1]))}
                    track["rate"] = struct.unpack(">f", data[sound[0xB5][0]:sound[0xB5][0] + 4])[0]
                    track["channels"] = unsigned(data, *sound[0x9F])
                tracks[number] = track
    # Clusters and their blocks.
    clusters = []
    for (element, at, size), element_start in zip(children, element_starts):
        if element == 0x1F43B675:
            clusters.append(element_start - segment)
            time = None
            for child, child_at, child_size in elements(data, at, at + size):
                if child == 0xE7:
                    time = unsigned(data, child_at, child_size)
                elif child == 0xA3:
                    number, block_at = vint(data, child_at, True)
                    relative = struct.unpack(">h", data[block_at:block_at + 2])[0]
                    tracks[number]["blocks"].append((time + relative, data[block_at + 3:child_at + child_size]))
    # Cues point at clusters.
    cues = [c for c in children if c[0] == 0x1C53BB6B]
    if not cues:
        fail("no Cues")
    for _, point, point_size in elements(data, cues[0][1], cues[0][1] + cues[0][2]):
        fields = {e: (a, s) for e, a, s in elements(data, point, point + point_size)}
        positions = {e: (a, s) for e, a, s in elements(data, fields[0xB7][0], sum(fields[0xB7]))}
        if unsigned(data, *positions[0xF1]) not in clusters:
            fail("a cue points at no cluster")
    print(f"{path}: {len(data)} bytes, {length_ms:.0f} ms, {len(clusters)} clusters")
    if duration is not None and abs(length_ms - duration) > 400:
        fail(f"duration {length_ms:.0f} ms, wanted about {duration}")
    names = {}
    for number, track in sorted(tracks.items()):
        names[track["name"]] = track
        blocks = track["blocks"]
        times = [t for t, _ in blocks]
        if times != sorted(times):
            fail(f"track {track['name']}: blocks out of time order")
        if track["codec"] == "V_MJPEG":
            if len(blocks) < minimum_frames:
                fail(f"{len(blocks)} frames, wanted at least {minimum_frames}")
            for _, frame in (blocks[0], blocks[-1]):
                image = Image.open(io.BytesIO(frame))
                image.load()
                if image.size != track["size"]:
                    fail(f"a frame of {image.size}, the track says {track['size']}")
            last = Image.open(io.BytesIO(blocks[-1][1])).convert("RGB")
            print(f"  {track['name']}: V_MJPEG {track['size'][0]}x{track['size'][1]}, {len(blocks)} frames, "
                  f"{times[0]}..{times[-1]} ms, average {sum(len(f) for _, f in blocks) // len(blocks)} bytes; a pixel {last.getpixel((5, 5))}")
            track["last"] = last
        else:
            samples = b"".join(chunk for _, chunk in blocks)
            rate, channels = int(track["rate"]), track["channels"]
            frames = len(samples) // (2 * channels)
            left = struct.unpack(f"<{frames * channels}h", samples[:frames * channels * 2])[::channels]
            seconds = frames / rate
            # The middle second (away from the edges).
            middle = left[max(0, frames // 2 - rate // 2):frames // 2 + rate // 2]

            def amplitude(frequency):
                w = 2 * math.pi * frequency / rate
                c = 2 * math.cos(w)
                s1 = s2 = 0.0
                for v in middle:
                    s1, s2 = v + c * s1 - s2, s1
                return math.sqrt(max(s1 * s1 + s2 * s2 - c * s1 * s2, 0)) / max(len(middle), 1) * 2
            found = {f: round(amplitude(f)) for f, _ in expected.get(track["name"], [])}
            print(f"  {track['name']}: {track['codec']} {rate} Hz {channels} ch, {seconds:.2f} s, default {track['default']}; amplitudes {found}")
            if duration is not None and abs(seconds * 1000 - duration) > 500:
                fail(f"track {track['name']}: {seconds:.2f} s of sound, wanted about {duration} ms")
            for frequency, wanted in expected.get(track["name"], []):
                value = found[frequency]
                if wanted == 0 and value > 300:
                    fail(f"track {track['name']}: {frequency} Hz at {value}, it should not be there")
                if wanted > 0 and abs(value - wanted) > wanted * 0.15:
                    fail(f"track {track['name']}: {frequency} Hz at {value}, wanted {wanted}")
    for name in expected:
        if name not in names:
            fail(f"no track {name} (there are {list(names)})")
    print("ok")


if __name__ == "__main__":
    main()
