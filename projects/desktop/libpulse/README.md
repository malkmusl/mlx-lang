# MLX Audio's client libraries

PulseAudio's client libraries and alsa-plugins' pulse plugin, written in
Mlx: what programs load to reach a PulseAudio server, here MLX Audio's
PulseAudio socket (`projects/desktop/audio/pulse.mlx`). With them neither
PulseAudio's `libpulse0` and `libpulse-mainloop-glib0` nor
`libasound2-plugins` is needed.

```sh
tools/build_libpulse.sh                # into mlx-out/libpulse
tools/build_libpulse.sh --deb          # and the package mlx-audio-libs
tools/check_libpulse.sh                # the libraries' own check
tools/check_pulse.sh                   # PulseAudio and ALSA programs on MLX Audio, with them
```

| Library | What it is | Built from |
| --- | --- | --- |
| `libpulse.so.0` | PulseAudio's client library: every function of libpulse 16.1 (378), symbol version `PULSE_0` | `libpulse.mlx` |
| `libpulse-simple.so.0` | the blocking API (`pa_simple_*`), on libpulse's threaded loop | `simple.mlx` |
| `libpulse-mainloop-glib.so.0` | libpulse's loop on a GLib main context (`pa_glib_mainloop_*`) | `glib.mlx` |
| `libasound_module_pcm_pulse.so`, `libasound_module_ctl_pulse.so` | ALSA's `pulse` PCM and control plugin (the same file), libpulse built in | `alsa.mlx` |

They are `--plugin` shared objects (see
[Shared objects](../../../docs/reference/formats.md#shared-objects)): each
exported function runs on an arena of its own, so the C programs that call
them on any of their threads need nothing from Mlx. The libraries carry
PulseAudio's sonames and symbol version (`--soname`,
`--symbol-version=PULSE_0`): programs linked against PulseAudio's load
them in its place.

## How it is made

- **Memory and calls** (`c.mlx`): the objects (contexts, streams,
  operations, property lists, loops) live in C memory (`calloc`), since
  a call's arena is gone when it returns; the program's callbacks are
  called through function pointers (`call1` ... `call5`). Locks are
  recursive priority-inheriting futexes, wakeups eventfds.
- **Values** (`sample.mlx`, `channels.mlx`, `volume.mlx`, `format.mlx`,
  `proplist.mlx`, `util.mlx`, `printf.mlx`): sample specs, channel maps,
  volumes and their dB and text forms, format infos, property lists, the
  error strings, `pa_proplist_setf`'s printf. `tools/libpulse_values.py`
  compares 1952 of their results with PulseAudio's library.
- **Loops** (`mainloop.mlx`, `threaded.mlx`, `glib.mlx`): `pa_mainloop`
  with its io, time and defer events and its poll function,
  `pa_threaded_mainloop` (a thread of its own, signals blocked),
  `pa_signal_*`, and the GLib loop as a GSource around a `pa_mainloop`.
- **The protocol** (`wire.mlx`, `core.mlx`, `context.mlx`, `stream.mlx`,
  `introspect.mlx`): protocol 35 without shared memory (the data goes in
  packets of at most 64 KiB), on `$PULSE_SERVER`'s unix sockets or
  `$XDG_RUNTIME_DIR/pulse/native`; the context's and streams' states,
  their buffer attributes, prebuffering, corking, draining, flushing,
  underflow and overflow notices, the timing (`pa_stream_get_time`
  interpolated between the server's figures, as PulseAudio's), the
  introspection and control requests and the subscription events.
- **ALSA** (`alsa.mlx`): alsa-lib's ioplug and ctl_ext interfaces
  (alsa-lib 1.2's layouts): a PCM whose ring buffer is a PulseAudio
  stream's, every format and rate ALSA asks for that PulseAudio knows,
  and a mixer with the default sink's and source's volume and mute.

`libpulse.mlx` is the root: it imports the export files
(`export_values.mlx`, `export_loops.mlx`, `export_context.mlx`,
`export_stream.mlx`), each exported function a thin call into the
modules above.

## Installed

`tools/install_compositor_session.sh` installs them to
`PREFIX/lib/mlx-audio`; with `--replace-sound-servers` ALSA's `pulse`
type comes from there (`projects/desktop/audio/asound.conf.in`), so
`libasound2-plugins` may be removed. `--replace-libpulse` installs the
package `mlx-audio-libs` (built by `tools/build_libpulse.sh --deb`): it
puts the libraries where Debian and Ubuntu keep PulseAudio's
(`/usr/lib/MULTIARCH`, the plugin in `alsa-lib/`), provides `libpulse0`,
`libpulse-mainloop-glib0` and `libasound2-plugins` and replaces them, so
every program (Firefox, Chromium and Electron apps such as Discord, SDL
games, mpv, GStreamer, pavucontrol) loads MLX Audio's. `pulseaudio-utils`
(`pactl`, `pacat`) depends on PulseAudio's own library and goes with it;
`mlx-audio` does their work. `apt install libpulse0
libpulse-mainloop-glib0 libasound2-plugins` brings PulseAudio's back.

## Not there

- The extensions (`pa_ext_stream_restore_*`, `pa_ext_device_restore_*`,
  `pa_ext_device_manager_*`) answer that the server has no such module,
  as PulseAudio's library does with a server without it; MLX Audio keeps
  the volumes itself.
- No shared memory (`shm`, `memfd`) and no autospawn: MLX Audio's socket
  takes the data in packets, and the session starts the server.
