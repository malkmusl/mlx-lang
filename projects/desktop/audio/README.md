# MLX Audio

MLX Audio is the desktop's sound server, the replacement for PipeWire,
and it runs on MLXIPC (`projects/desktop/ipc`): apps find it on the
session bus, talk to it over a direct MLXIPC channel, and what each app
may do with sound is an MLXIPC permission. `mlx-audiod` is the server,
`std.audio` the client library, `mlx-audio` the command line. All of it
is Mlx: no libc, no alsa-lib, no PipeWire. PulseAudio apps reach it on
PulseAudio's own socket (below).

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
   play (`audio.play`), record (`audio.record`) or record what plays (a
   sink's monitor, `audio.monitor`):
   `org.mlx.IPC.Check`. The bus remembers who opened each channel, so it
   answers for the app even after the app left the bus.
4. The stream plays: the server says how much it wants (REQUEST), the app
   sends that much (DATA), never more, so the server's buffer is the
   stream's latency. Recording streams get the input as it comes
   (RECORDED).

The protocol (in `std/src/audio.mlx`): messages of an 8-byte header
(kind, length) and a payload, little-endian; OPEN, DATA, CLOSE, VOLUME,
DRAIN, LIST, STATUS, SET from the app; OPENED, REFUSED, REQUEST,
RECORDED, DRAINED, STREAMS, ENDED, STATE from the server. STATUS and SET
are the controls (they take `audio.control`): STATE is the server's state
as `key = value` lines (output and input, why one is not playing,
volumes, mute, the sound cards' devices, the streams, the apps that asked
for sound and whether they may), SET changes one thing (the volume, mute,
the input's volume or mute, a stream's volume, the output or the input,
or "check every stream's permission again").

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

An app with no rule for the microphone or the monitors is asked about,
sandboxed or not: the stream waits while the user answers in
mlx-permissions' dialog
(`projects/desktop/permissions`). When the rules change the bus says so
(`org.mlx.IPC.PermissionsChanged`) and the server checks every stream
again: a recording whose permission went ends.

By default an unsandboxed app may play and control the server
(`audio.control`: switch outputs, set volumes, route apps; as with
PipeWire), and is asked before it records or records what plays; a
sandboxed one (Flatpak) may play, nothing else. A refused
stream gets REFUSED with the reason, and `mlx-audio` says so. When a rule
takes recording from an app that records, the server ends its stream
(ENDED) once it is told to check again (SET's recheck; mlx-settings sends
it after writing `ipc.conf`).

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

A card another sound server holds (PipeWire, PulseAudio) cannot be
opened: the server then plays to nobody, says who holds it (`holder`,
from `/proc/*/fd`) and tries again every 2 seconds, taking the card once
it is free. The desktop session (`projects/desktop/compositor/session`)
stops PipeWire, WirePlumber and PulseAudio for its time, so the card is
MLX Audio's (`MLX_SESSION_AUDIO=pipewire` keeps them instead). Outputs
and inputs switch while streams play; the choice, the volumes and mute
are kept in `~/.config/mlx/audio.conf` ([`config.mlx`](config.mlx);
`--output` and `--input` win over it).

The sound card path is tested without a sound card:
[`tools/fake_alsa.py`](../../../tools/fake_alsa.py) plays one. When a
device path is a Unix socket the server speaks frames over it instead of
ioctls (hello, ioctl with its number and argument bytes, wake), so the
emulated card sees every PCM ioctl, checks it against the kernel's
structure layouts (`snd_pcm_hw_params`, `sw_params`, `xferi`) and answers
as the kernel would; it takes only S32_LE for playback and only 44100 Hz
for capture, so the server must negotiate and convert, and it counts
underruns. `MLX_SND_DIR` and `MLX_ASOUND_DIR` point the server at it.

## PulseAudio apps

Programs that speak only PulseAudio (Firefox, Chromium and Electron apps,
SDL games, mpv, VLC, GStreamer, Wine, pactl, pavucontrol) play and record
through MLX Audio as they would through pipewire-pulse: mlx-audiod
answers PulseAudio's native protocol at `$XDG_RUNTIME_DIR/pulse/native`
([`pulse.mlx`](pulse.mlx), [`pulse_info.mlx`](pulse_info.mlx),
[`tagstruct.mlx`](tagstruct.mlx); version 35, as libpulse 15 and later,
samples in the packets, no shared memory). Flatpak hands the same socket
to apps with `--socket=pulseaudio`. Their streams are the server's like
any other: mixed, routed, in the Sound settings, and allowed by MLXIPC.
The app is the process at the socket's other end (SO_PEERCRED, named by
`std.appid` as the bus names apps: a Flatpak's id, else the program's
name), and the bus is asked about it by process
(`org.mlx.IPC.CheckProcess`): a sandboxed app may play, is asked before
it records, and may not change the server; a recording ends when its
permission goes.

What PulseAudio apps see: one sink, `mlx.output` (the output's mix), and
each virtual sink by its name; the sources `mlx.input` (the input),
`mlx.output.monitor` and `NAME.monitor`; every stream as a sink input or
source output, whichever protocol its app speaks; S16, S24, S24 in 32
bits, S32, float and U8 samples, 1 to 32 channels, 8000 to 192000 Hz.
Volumes are PulseAudio's (cubic), so pavucontrol's 50% is a quarter's
loudness, as with PulseAudio. The buffer's sizes, prebuffering, corking,
draining, flushing, underflow notices and the latency figures behind an
app's clock (`pa_stream_get_time`) are PulseAudio's. Another server at
the socket (PulseAudio, pipewire-pulse) keeps it; `--no-pulse` leaves it
alone.

Since PulseAudio apps do not come through the bus, the session starts
mlx-audiod with it (`mlx-ipcd --start org.mlx.Audio`). The libraries
they load to speak the protocol are Mlx too
([`projects/desktop/libpulse`](../libpulse/README.md): libpulse.so.0,
libpulse-simple.so.0, libpulse-mainloop-glib.so.0). ALSA programs
(aplay, arecord, games that open `default`) reach it through ALSA's
pulse plugin, MLX Audio's own (`libpulse/alsa.mlx`):
[`asound.conf.in`](asound.conf.in) makes it ALSA's default device.

### Replacing PipeWire and PulseAudio

`tools/install_compositor_session.sh --replace-sound-servers` makes MLX
Audio the system's sound server for good: PipeWire, WirePlumber and
PulseAudio are masked for every user (`systemctl --global mask`, from
the next login; the session otherwise stops them only for its time),
libpulse starts no PulseAudio of its own
(`/etc/pulse/client.conf.d/50-mlx-audio.conf`: `autospawn = no`), and
`asound.conf.in` goes to `/etc/alsa/conf.d/99-zz-mlx-audio.conf` (after
pipewire-alsa's default, which it overrides) with MLX Audio's plugin in
`PREFIX/lib/mlx-audio`. Their packages may then be removed, and
alsa-plugins' (`libasound2-plugins`) too. `--replace-libpulse` puts MLX
Audio's libpulse in the place of PulseAudio's (the package
`mlx-audio-libs` replaces `libpulse0`, `libpulse-mainloop-glib0` and
`libasound2-plugins`). Other desktops (GNOME, KDE) have no sound server
then.
`--restore-sound-servers` (and `--uninstall`) undoes it; PulseAudio's
libpulse comes back with `apt install libpulse0 libpulse-mainloop-glib0
libasound2-plugins`. PipeWire's other
part, video, is mlx-capture's: browsers share the screen through its
ScreenCast portal and PipeWire connections of its own (see the
compositor's README, "Screen sharing").
`tools/check_pulse.sh` plays every sample format through pacat, records
the input and the monitor with parec, changes volumes with pactl, checks
a sandboxed app's permissions, plays and records through ALSA's plugin,
and plays an `<audio>` element in Firefox when one is installed, all of
them with MLX Audio's libpulse (`SYSTEM_LIBPULSE=1`: PulseAudio's).

## Sinks and routing

Every playing stream plays to a sink ([`routing.mlx`](routing.mlx)). The
default sink is the output's mix. Virtual sinks mix their streams apart,
at a volume of their own, and pass the mix on to the default sink or to
nothing (then only their monitor has it). Each sink has a monitor: a
recording stream opened from `monitor` (the default sink's: the
desktop's sound) or `monitor:NAME` gets what plays to it, before the
sink's volume, so turning the speakers down does not change a recording.
That is how mlx-capture records the desktop's sound apart from the
microphone, and how one app's sound can be recorded alone: route it to a
sink of its own.

Each app (by its MLXIPC name) has a volume and mute over all its streams
and a route, the sink its streams play to unless a stream named one when
it opened (std.audio's `openStreamOn`); changing the route moves its
streams at once. Sinks and the apps' settings are kept in `audio.conf`.

```sh
mlx-audio sink add music              # into the output
mlx-audio sink add obs none           # only recordable
mlx-audio app firefox sink obs        # firefox plays there from now on
mlx-audio app firefox volume 60
mlx-audio record mix.wav 10 --source monitor:obs
```

## mlx-audio

```
mlx-audio play FILE.wav              8, 16, 24, 32-bit or float WAVE files
mlx-audio tone HZ SECONDS
mlx-audio record FILE.wav SECONDS    --rate HZ, --channels N
mlx-audio streams                    app, direction, rate, channels, volume, name
mlx-audio volume PERCENT             the output's volume
mlx-audio mute on|off                the output muted
mlx-audio output DEVICE              auto, alsa:pcmC1D0p, null (remembered)
mlx-audio input DEVICE               auto, alsa:NAME, none
mlx-audio status                     the server's state (STATE)
mlx-audio recheck                    every stream's permission asked again
mlx-audio sinks | apps               the virtual sinks; the apps' settings
mlx-audio sink add NAME [none]       a virtual sink (none: to nothing)
mlx-audio sink remove|volume|mute NAME [VALUE]
mlx-audio app NAME volume|mute|sink VALUE
mlx-audio move STREAM SINK           one stream (its id in status)
  --sink NAME                        the sink to play to (play, tone)
  --source input|monitor|monitor:SINK  what record records
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
| `alsa.mlx` | the kernel's PCM interface (and the emulated card's frames) |
| `routing.mlx` | sinks, monitors, each app's volume, mute and route |
| `config.mlx` | audio.conf |
| `pulse.mlx` | the PulseAudio socket: clients, packets, AUTH, streams, latency |
| `pulse_info.mlx` | what PulseAudio apps ask about (server, sinks, sources, streams, clients) and change (volumes, mute, moving) |
| `tagstruct.mlx` | PulseAudio's tagstructs, read and written |
| `asound.conf.in` | ALSA's default device through MLX Audio's pulse plugin (`@ALSA_PLUGIN@`: its path) |
| `tool.mlx` | mlx-audio |

## In mlx-settings

The settings app's Sound page
([`projects/desktop/settings/sound.mlx`](../settings/sound.mlx)) shows
STATE and asks for it twice a second while it is shown: the output and
the input (automatic, each card's device, none) with a note when the card
is busy or missing, their volumes and mute, a test tone, the apps with a
volume each and the sink each plays to (a click takes the next), the
virtual sinks' volumes, and the apps that wanted to record with
a switch each: it writes `allow APP use audio.record` (or `deny`) into
`ipc.conf` and sends the recheck.

## Next

- Shared-memory streams over the channel (memfd), for low latency; for
  PulseAudio apps too (its memfd blocks).
- Hotplug: a card plugged in is found only by the 2-second retry while
  there is none to play to; watching `/dev/snd` would find it at once.
- More than stereo: the mix has two channels; a 5.1 stream plays its
  front left and right.
