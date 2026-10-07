# MLX Audio

MLX Audio is the desktop's sound server, the replacement for PipeWire,
and it runs on MLXIPC (`projects/desktop/ipc`): apps find it on the
session bus, talk to it over a direct MLXIPC channel, and what each app
may do with sound is an MLXIPC permission. `mlx-audiod` is the server,
`std.audio` the client library, `mlx-audio` the command line. All of it
is Mlx: no libc, no alsa-lib, no PipeWire.

```sh
mlx4 projects/desktop/audio/main.mlx -o mlx-audiod
mlx4 projects/desktop/audio/tool.mlx -o mlx-audio
mlx-audio play song.wav         # in the desktop session: the server starts on first use
tools/check_audio.sh            # the end-to-end check
```

## How an app gets sound

1. `std.audio.connect` connects to the session bus (mlx-ipcd) and calls
   `org.mlx.IPC.Connect("org.mlx.Audio")`. The bus allows it when the app
   may talk to org.mlx.Audio (every app may, sandboxed ones included,
   unless `ipc.conf` says otherwise). When the server is not running it
   is started first (its service file).
2. The bus makes a socket pair: one end goes back to the app, the other
   to the server with who the app is (`org.mlx.IPC.Peer.Connected`). The
   app leaves the bus; from here on it talks to the server directly.
3. For each stream it opens, the server asks the bus whether the app may
   play (`audio.play`) or record (`audio.record`):
   `org.mlx.IPC.Check`. The bus remembers who opened each channel, so it
   answers for the app even after the app left the bus.
4. The stream plays: the server says how much it wants (REQUEST), the app
   sends that much (DATA), never more, so the server's buffer is the
   stream's latency. Recording streams get the input as it comes
   (RECORDED).

The protocol (in `std/src/audio.mlx`): messages of an 8-byte header
(kind, length) and a payload, little-endian; OPEN, DATA, CLOSE, VOLUME,
DRAIN, LIST from the app; OPENED, REFUSED, REQUEST, RECORDED, DRAINED,
STREAMS, ENDED from the server.

## Permissions

They are MLXIPC's (`~/.config/mlx/ipc.conf`, read again when it changes;
see `projects/desktop/ipc/README.md`):

```
# MLX Audio
allow org.example.Recorder use audio.record    # a sandboxed recorder
deny game use audio.record                     # a program that never should
deny org.example.Noisy use audio.play
deny org.example.Spy talk org.mlx.Audio        # no channel at all
```

By default an unsandboxed app may play and record (as with PipeWire); a
sandboxed one (Flatpak) may play, not record. A refused stream gets
REFUSED with the reason, and `mlx-audio` says so.

## The server

- Streams of any rate (8000 to 192000 Hz), 1 to 8 channels, 16-bit,
  32-bit or float samples, converted to the mix: linear resampling, down-
  and upmixing, each stream at its own volume, then the output's volume.
- The output: the sound card through the kernel's ALSA interface
  (`/dev/snd/pcmC*D*p`, ioctls, 16 or 32 bits, the card pacing the mix),
  a WAVE file (`--output file:mix.wav`) or nothing (`null`, paced by a
  timer). The input: the card's capture, a tone (`sine:440`), a looped
  WAVE file, or silence.
- An app that leaves (or is killed) takes its streams along; the server
  keeps its memory constant however long it runs.

```
mlx-audiod [--output auto|alsa[:pcmC1D0p]|file:PATH.wav|null]
           [--input auto|alsa[:NAME]|sine:HZ|file:PATH.wav|none]
           [--rate HZ] [--period MS] [--verbose]
```

The sound card path follows the kernel's interface (checked against its
headers), but this repository's checks run without a sound card: they
play into a file and record from a tone or a file. A card another sound
server holds (PipeWire, PulseAudio) cannot be opened; `auto` then plays
to nobody and says so.

## mlx-audio

```
mlx-audio play FILE.wav              8, 16, 24, 32-bit or float WAVE files
mlx-audio tone HZ SECONDS
mlx-audio record FILE.wav SECONDS    --rate HZ, --channels N
mlx-audio streams                    app, direction, rate, channels, volume, name
mlx-audio volume PERCENT             the output's volume
  --volume PERCENT                   the stream's volume
  --latency MS                       the server's buffer for it (default 100)
```

## Modules

| File | What it does |
| --- | --- |
| `main.mlx` | mlx-audiod: options, the event loop (bus, channels, timer or sound card, signals) |
| `state.mlx` | the records: server, peers, streams, devices |
| `protocol.mlx` | the bus (org.mlx.Audio, channels, Check), the protocol on the channels |
| `mixer.mlx` | rings, mixing, requests, recording |
| `samples.mlx` | sample formats, resampling, a tone |
| `devices.mlx` | outputs and inputs: sound card, file, tone, nothing |
| `alsa.mlx` | the kernel's PCM interface |
| `tool.mlx` | mlx-audio |

## Next

- PulseAudio's protocol on top (as pipewire-pulse does), for the programs
  that only speak it, each of them an app to MLXIPC's permissions.
- Shared-memory streams over the channel (memfd), for low latency.
- Devices: several cards, choosing and switching them, hotplug; volume
  and streams in mlx-settings; a permission prompt the first time an app
  wants to record.
