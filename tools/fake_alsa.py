#!/usr/bin/env python3
"""An emulated ALSA sound card for MLX Audio's checks (tools/check_audio.sh).

There is no sound card here, so this plays one at the level mlx-audiod
talks to it: the kernel's PCM ioctls (projects/desktop/audio/alsa.mlx).
Its devices are Unix sockets at DIR/snd/pcmC0D0p (playback) and pcmC0D0c
(capture), with DIR/asound/card0/... for their names (MLX_SND_DIR and
MLX_ASOUND_DIR point mlx-audiod there), and the ioctls arrive as frames
(alsa.mlx describes them): HW_PARAMS, SW_PARAMS, PREPARE, START, DROP,
WRITEI_FRAMES, READI_FRAMES. The structures are checked against
<sound/asound.h>'s layouts (snd_pcm_hw_params 608 bytes, snd_pcm_sw_params
136, snd_xferi 24) as the kernel would, and the card behaves like one:

- playback takes only S32_LE (so the server must fall back from S16_LE) at
  48000 or 44100 Hz, stereo; it plays in real time a period at a time from
  a buffer of 4 periods, starts once start_threshold frames are in, says
  when a period fits (wake frames: what poll(2) would report), runs dry
  into an underrun (-EPIPE until PREPARE) when nobody writes, and writes
  what it played into DIR/played.wav (16-bit stereo);
- capture takes only S16_LE at 44100 Hz (the server must resample),
  recording a 523 Hz tone in real time;
- a device is exclusive (a second open is -EBUSY), and --busy SECONDS keeps
  playback busy at first, as when another sound server has it.

Usage: fake_alsa.py DIR [--busy SECONDS]
It writes DIR/ready once listening, and DIR/played.wav's header on SIGTERM
(and every second).
"""
import math
import os
import signal
import socket
import struct
import sys
import threading
import time

HW_PARAMS, SW_PARAMS = 3261088017, 3230155027
PREPARE, START, DROP = 16704, 16706, 16707
WRITEI, READI = 1075331408, 2149073233
EAGAIN, EBUSY, EINVAL, EPIPE = 11, 16, 22, 32
ACCESS_RW_INTERLEAVED, FORMAT_S16_LE, FORMAT_S32_LE = 3, 2, 10
PARAM_SAMPLE_BITS, PARAM_FRAME_BITS, PARAM_CHANNELS, PARAM_RATE = 8, 9, 10, 11
PARAM_PERIOD_TIME, PARAM_PERIOD_SIZE, PARAM_PERIOD_BYTES, PARAM_PERIODS = 12, 13, 14, 15
PARAM_BUFFER_TIME, PARAM_BUFFER_SIZE, PARAM_BUFFER_BYTES = 16, 17, 18
PCM_VERSION = 0x20011
FRAME_IOCTL, FRAME_WAKE, FRAME_HELLO = 1, 2, 3


def interval(params, param):
    at = 260 + (param - 8) * 12
    low, high, flags = struct.unpack_from('<III', params, at)
    return low, high, flags


def set_interval(params, param, value):
    struct.pack_into('<III', params, 260 + (param - 8) * 12, value, value, 4)


def mask_bits(params, index):
    return int.from_bytes(params[4 + index * 32:4 + index * 32 + 32], 'little')


class Device:
    def __init__(self, harness, path, capture):
        self.harness = harness
        self.path = path
        self.capture = capture
        self.lock = threading.Lock()
        self.connection = None
        self.reset()
        self.listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.listener.bind(path)
        self.listener.listen(4)
        threading.Thread(target=self.accept, daemon=True).start()
        threading.Thread(target=self.clock, daemon=True).start()

    def reset(self):
        self.configured = False
        self.prepared = False
        self.running = False
        self.xrun = False
        self.queue = bytearray()
        self.format = None
        self.rate = 0
        self.period = 0
        self.periods = 0
        self.avail_min = 0
        self.start_threshold = 0
        self.phase = 0.0

    @property
    def frame_bytes(self):
        return 4 if self.format == FORMAT_S16_LE else 8

    @property
    def buffer(self):
        return self.period * self.periods

    def send(self, kind, payload=b''):
        connection = self.connection
        if connection is None:
            return
        try:
            connection.sendall(struct.pack('<II', kind, len(payload)) + payload)
        except OSError:
            pass

    def accept(self):
        while True:
            connection, _ = self.listener.accept()
            busy = self.connection is not None or (not self.capture and time.monotonic() < self.harness.busy_until)
            status = -EBUSY if busy else 0
            connection.sendall(struct.pack('<IIq', FRAME_HELLO, 8, status))
            if busy:
                self.harness.log(f'{os.path.basename(self.path)}: busy')
                connection.close()
                continue
            with self.lock:
                self.reset()
                self.connection = connection
            self.harness.log(f'{os.path.basename(self.path)}: opened')
            threading.Thread(target=self.serve, args=(connection,), daemon=True).start()

    def receive(self, connection, length):
        data = b''
        while len(data) < length:
            chunk = connection.recv(length - len(data))
            if not chunk:
                raise EOFError
            data += chunk
        return data

    def serve(self, connection):
        try:
            while True:
                kind, length = struct.unpack('<II', self.receive(connection, 8))
                payload = bytearray(self.receive(connection, length))
                if kind != FRAME_IOCTL or length < 8:
                    self.harness.fail(f'{self.path}: a frame of kind {kind}')
                request = struct.unpack_from('<Q', payload, 0)[0]
                argument = payload[8:]
                with self.lock:
                    result, back, extra = self.ioctl(request, argument)
                reply = struct.pack('<q', result) + bytes(back) + bytes(extra)
                connection.sendall(struct.pack('<II', FRAME_IOCTL, len(reply)) + reply)
        except (EOFError, OSError):
            pass
        with self.lock:
            self.connection = None
            self.reset()
        self.harness.log(f'{os.path.basename(self.path)}: closed')

    def ioctl(self, request, argument):
        if request == HW_PARAMS:
            return self.hw_params(argument)
        if request == SW_PARAMS:
            return self.sw_params(argument)
        if request == PREPARE:
            if not self.configured:
                return -EINVAL, argument, b''
            self.prepared, self.running, self.xrun = True, False, False
            self.queue = bytearray()
            return 0, argument, b''
        if request == START:
            if not self.prepared:
                return -EINVAL, argument, b''
            self.running = True
            return 0, argument, b''
        if request == DROP:
            self.running, self.prepared = False, False
            self.queue = bytearray()
            return 0, argument, b''
        if request in (WRITEI, READI):
            return self.transfer(request, argument)
        self.harness.fail(f'{self.path}: unknown ioctl {request}')
        return -EINVAL, argument, b''

    def hw_params(self, argument):
        params = bytearray(argument[:608])
        if len(argument) != 608:
            self.harness.fail(f'HW_PARAMS of {len(argument)} bytes')
        if struct.unpack_from('<I', params, 512)[0] != 0xffffffff:
            self.harness.fail('HW_PARAMS without rmask set')
        access, formats, subformat = mask_bits(params, 0), mask_bits(params, 1), mask_bits(params, 2)
        supported = FORMAT_S16_LE if self.capture else FORMAT_S32_LE
        rates = (44100,) if self.capture else (48000, 44100)
        if not access & (1 << ACCESS_RW_INTERLEAVED) or not subformat & 1:
            return -EINVAL, argument, b''
        if formats != 1 << supported:
            return -EINVAL, argument, b''
        bits = 16 if supported == FORMAT_S16_LE else 32
        sample_bits, frame_bits, channels = interval(params, PARAM_SAMPLE_BITS), interval(params, PARAM_FRAME_BITS), interval(params, PARAM_CHANNELS)
        if sample_bits[:2] != (bits, bits) or frame_bits[:2] != (bits * 2, bits * 2) or channels[:2] != (2, 2):
            return -EINVAL, argument, b''
        rate = interval(params, PARAM_RATE)
        if rate[0] != rate[1] or rate[0] not in rates:
            return -EINVAL, argument, b''
        low, high, _ = interval(params, PARAM_PERIOD_SIZE)
        period = low if low == high else 512
        if not 64 <= period <= 4096 or (low != high and not low <= period <= high):
            return -EINVAL, argument, b''
        plow, phigh, _ = interval(params, PARAM_PERIODS)
        periods = max(plow, 4) if phigh >= 4 else 0
        if periods == 0 or periods > phigh:
            return -EINVAL, argument, b''
        self.format, self.rate, self.period, self.periods = supported, rate[0], period, periods
        for param, value in ((PARAM_RATE, rate[0]), (PARAM_PERIOD_SIZE, period), (PARAM_PERIODS, periods),
                             (PARAM_BUFFER_SIZE, period * periods), (PARAM_PERIOD_BYTES, period * self.frame_bytes),
                             (PARAM_BUFFER_BYTES, period * periods * self.frame_bytes),
                             (PARAM_PERIOD_TIME, period * 1000000 // rate[0]), (PARAM_BUFFER_TIME, period * periods * 1000000 // rate[0])):
            set_interval(params, param, value)
        self.configured = True
        self.harness.log(f'{os.path.basename(self.path)}: {"S16_LE" if supported == FORMAT_S16_LE else "S32_LE"} {rate[0]} Hz, periods of {period} x {periods}')
        return 0, params, b''

    def sw_params(self, argument):
        if len(argument) != 136:
            self.harness.fail(f'SW_PARAMS of {len(argument)} bytes')
        tstamp_mode, period_step = struct.unpack_from('<iI', argument, 0)
        avail_min, xfer_align, start, stop, silence_threshold, silence_size, boundary = struct.unpack_from('<7Q', argument, 16)
        proto = struct.unpack_from('<I', argument, 72)[0]
        problems = []
        if not self.configured:
            problems.append('before HW_PARAMS')
        if tstamp_mode != 0 or period_step != 1:
            problems.append('tstamp_mode/period_step')
        if not 1 <= avail_min <= self.buffer:
            problems.append(f'avail_min {avail_min}')
        if not 1 <= start <= self.buffer or stop != self.buffer:
            problems.append(f'thresholds {start} {stop}')
        if boundary < self.buffer or boundary % self.buffer:
            problems.append(f'boundary {boundary}')
        if proto != PCM_VERSION:
            problems.append(f'proto {proto:#x}')
        if problems:
            self.harness.fail('SW_PARAMS: ' + ', '.join(problems))
            return -EINVAL, argument, b''
        self.avail_min, self.start_threshold = avail_min, start
        return 0, argument, b''

    def transfer(self, request, argument):
        if len(argument) < 24:
            self.harness.fail('a transfer without its snd_xferi')
        frames = struct.unpack_from('<Q', argument, 16)[0]
        if not self.prepared:
            return -EINVAL, argument[:24], b''
        if self.xrun:
            return -EPIPE, argument[:24], b''
        back = bytearray(argument[:24])
        if request == WRITEI:
            data = argument[24:]
            if self.capture or len(data) != frames * self.frame_bytes:
                self.harness.fail(f'WRITEI_FRAMES of {frames} frames with {len(data)} bytes')
            room = self.buffer - len(self.queue) // self.frame_bytes
            taken = min(frames, room)
            if taken == 0:
                return -EAGAIN, back, b''
            self.queue += data[:taken * self.frame_bytes]
            if not self.running and len(self.queue) // self.frame_bytes >= self.start_threshold:
                self.running = True
            struct.pack_into('<q', back, 0, taken)
            return 0, back, b''
        have = len(self.queue) // self.frame_bytes
        taken = min(frames, have)
        if taken == 0:
            return -EAGAIN, back, b''
        data = bytes(self.queue[:taken * self.frame_bytes])
        del self.queue[:taken * self.frame_bytes]
        struct.pack_into('<q', back, 0, taken)
        return 0, back, data

    def clock(self):
        # A period at a time, at the card's pace.
        while True:
            with self.lock:
                period_time = self.period / self.rate if self.rate else 0.01
            time.sleep(period_time)
            with self.lock:
                if not self.running or self.connection is None:
                    continue
                size = self.period * self.frame_bytes
                if self.capture:
                    tone = bytearray()
                    for _ in range(self.period):
                        value = int(12000 * math.sin(self.phase))
                        self.phase += 2 * math.pi * 523 / self.rate
                        tone += struct.pack('<hh', value, value)
                    self.queue += tone
                    if len(self.queue) > self.buffer * self.frame_bytes:
                        self.xrun, self.running = True, False
                        self.harness.log('capture: overrun')
                    self.send(FRAME_WAKE)
                else:
                    played = bytes(self.queue[:size])
                    del self.queue[:size]
                    self.harness.played(played, self.frame_bytes)
                    if len(played) < size:
                        # poll(2) reports the underrun (POLLERR).
                        self.xrun, self.running = True, False
                        self.harness.underruns += 1
                        self.send(FRAME_WAKE)
                        continue
                    room = self.buffer - len(self.queue) // self.frame_bytes
                    if room >= self.avail_min:
                        self.send(FRAME_WAKE)


class Harness:
    def __init__(self, directory, busy):
        self.directory = directory
        self.busy_until = time.monotonic() + busy
        self.underruns = 0
        self.failures = []
        self.lock = threading.Lock()
        os.makedirs(f'{directory}/snd', exist_ok=True)
        for kind, name in (('p', 'Fake Speakers'), ('c', 'Fake Microphone')):
            os.makedirs(f'{directory}/asound/card0/pcm0{kind}', exist_ok=True)
            with open(f'{directory}/asound/card0/pcm0{kind}/info', 'w') as info:
                info.write(f'card: 0\ndevice: 0\nsubdevice: 0\nstream: {"PLAYBACK" if kind == "p" else "CAPTURE"}\nid: Fake\nname: {name}\n')
        with open(f'{directory}/asound/card0/id', 'w') as card:
            card.write('FakeCard\n')
        self.wav = open(f'{directory}/played.wav', 'wb')
        self.wav.write(b'\0' * 44)
        self.played_bytes = 0
        self.rate = 48000
        self.devices = [Device(self, f'{directory}/snd/pcmC0D0p', False), Device(self, f'{directory}/snd/pcmC0D0c', True)]
        threading.Thread(target=self.headers, daemon=True).start()
        with open(f'{directory}/ready', 'w') as ready:
            ready.write('ready\n')

    def log(self, line):
        print(f'fake_alsa: {line}', flush=True)

    def fail(self, line):
        self.failures.append(line)
        self.log('FAIL ' + line)

    def played(self, data, frame_bytes):
        # 16-bit stereo, whatever the card's format.
        if frame_bytes == 8:
            words = struct.unpack(f'<{len(data) // 4}i', data)
            data = struct.pack(f'<{len(words)}h', *(w >> 16 for w in words))
        with self.lock:
            self.rate = self.devices[0].rate or self.rate
            self.wav.write(data)
            self.played_bytes += len(data)

    def header(self):
        with self.lock:
            self.wav.seek(0)
            self.wav.write(b'RIFF' + struct.pack('<I', 36 + self.played_bytes) + b'WAVEfmt ' + struct.pack('<IHHIIHH', 16, 1, 2, self.rate, self.rate * 4, 4, 16) + b'data' + struct.pack('<I', self.played_bytes))
            self.wav.seek(0, 2)
            self.wav.flush()

    def headers(self):
        while True:
            time.sleep(1)
            self.header()

    def finish(self, *_):
        self.header()
        self.log(f'underruns: {self.underruns}')
        with open(f'{self.directory}/result', 'w') as result:
            result.write(f'underruns {self.underruns}\n')
            for failure in self.failures:
                result.write(f'FAIL {failure}\n')
        os._exit(0)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(2)
    busy = 0.0
    if '--busy' in sys.argv:
        busy = float(sys.argv[sys.argv.index('--busy') + 1])
    harness = Harness(sys.argv[1], busy)
    signal.signal(signal.SIGTERM, harness.finish)
    signal.signal(signal.SIGINT, harness.finish)
    while True:
        time.sleep(3600)


if __name__ == '__main__':
    main()
