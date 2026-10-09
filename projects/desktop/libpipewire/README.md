# MLX Capture's client library (libpipewire)

PipeWire's client library, written in Mlx: what programs load to take a
screen share or a camera over PipeWire. In the MLX session the socket
they get from the ScreenCast portal is mlx-capture's
(`projects/desktop/capture/pipewire.mlx` answers as the PipeWire daemon
would), and with this library PipeWire's `libpipewire-0.3-0` is not
needed either.

```sh
tools/build_libpipewire.sh             # into mlx-out/libpipewire
tools/build_libpipewire.sh --deb       # and the package mlx-capture-libs
tools/check_libpipewire.sh             # the library's own check
tools/check_screencast.sh              # sharing with the system's libpipewire, and Firefox
```

| Library | What it is | Built from |
| --- | --- | --- |
| `libpipewire-0.3.so.0` | PipeWire's client library: every function of libpipewire 1.0.5 (443) and its two data symbols (`pw_log_level`, `PW_LOG_TOPIC_DEFAULT`) | `libpipewire.mlx` |

It is a `--plugin` shared object (see
[Shared objects](../../../docs/reference/formats.md#shared-objects)): each
exported function runs on an arena of its own, so the C programs that call
it on any of their threads need nothing from Mlx. It carries PipeWire's
soname (`--soname=libpipewire-0.3.so.0`) and needs libc alone, so programs
linked against PipeWire's library load it in its place: GStreamer's
`pipewiresrc`, Firefox and Chromium (WebRTC's screen capture), OBS.

## How it is made

- **Memory and calls** (`../libpulse/c.mlx`, shared with libpulse): the
  objects (loops, contexts, cores, proxies, streams, properties) live in C
  memory (`calloc`), since a call's arena is gone when it returns; the
  program's callbacks are called through function pointers. Locks are
  futexes, wakeups eventfds.
- **Hooks and interfaces** (`hooks.mlx`): `spa_hook_list` with the
  cursor-safe iteration PipeWire's inline functions do, `spa_interface`
  method tables (version, then the function pointers) for the objects
  programs call through the headers' inline wrappers.
- **Properties** (`properties.mlx`, `export_properties.mlx`): `struct
  pw_properties` as C reads it (`props->dict`, the items in an array of
  C strings), building, lookup, the parsing of numbers and booleans,
  copying, updating by keys, the `{ key = value }` strings,
  `pw_properties_setf`'s printf (`../libpulse/printf.mlx`) and the JSON
  serialization. `tools/libpipewire_values.py` compares 106 results with
  PipeWire's library.
- **Loops** (`loop.mlx`, `thread_loop.mlx`, `export_loops.mlx`): `pw_loop`
  on epoll with its io, idle, event, timer and signal sources and its
  `spa_loop` invoke queue; `pw_thread_loop` (a thread of its own, the
  recursive lock, `wait`, `timed_wait`, `signal`, `accept`); `pw_main_loop`
  and `pw_data_loop`.
- **The protocol** (`core.mlx`, `proxy.mlx`, `context.mlx`, `objects.mlx`,
  `infos.mlx`): PipeWire's native protocol, version 3, on the socket the
  program hands over (`pw_context_connect_fd`, the portal's descriptor) or
  `$PIPEWIRE_REMOTE` in `$XDG_RUNTIME_DIR`: the core (`Hello`, `Sync`,
  `Pong`, the registry, `CreateObject`), proxies with their ids, bound ids
  and listeners, the registry's globals, the nodes', ports', devices',
  links', factories' and modules' infos and params, metadata, and the
  `pw_*_info_update` merges programs keep their copies with.
- **Streams** (`stream.mlx`, `export_stream.mlx`): `pw_stream` as PipeWire's
  stream.c does it, a client node of the daemon's: the node and port
  params and infos, the format negotiation (`param_changed`), the
  buffers the daemon shares (memfd, mapped, `pw_buffer` with its
  `spa_buffer`, metas and datas), the activation records and the io area,
  the cycle (`process`, `dequeue_buffer`, `queue_buffer`, the driver's
  activation counted down), `pw_stream_get_time`, draining, flushing,
  trigger_process for driving streams.
- **Logging and the rest** (`export_misc.mlx`, `util.mlx`, `process.mlx`):
  `pw_init` (`PIPEWIRE_DEBUG` sets the level), `pw_log_*` to stderr,
  `pw_log_level` and `PW_LOG_TOPIC_DEFAULT` as writable data symbols
  (`export const`), versions and names, the state strings, `pw_strip` and
  the string lists, `pw_getrandom`.

`libpipewire.mlx` is the root: it imports the export files, each exported
function a thin call into the modules above. `export_infos.mlx` has the
info merges, `export_stubs.mlx` what the library does not do (below).

## Installed

`tools/install_compositor_session.sh` installs it to
`PREFIX/lib/mlx-capture`. `--replace-libpipewire` installs the package
`mlx-capture-libs` (built by `tools/build_libpipewire.sh --deb`): it puts
the library where Debian and Ubuntu keep PipeWire's (`/usr/lib/MULTIARCH`),
provides `libpipewire-0.3-0` (`libpipewire-0.3-0t64` on Ubuntu 24.04 and
later) and replaces it, so every program loads MLX Capture's. The PipeWire
daemon's own packages (`pipewire`, `pipewire-pulse`, `pipewire-alsa`,
`wireplumber`, `libpipewire-0.3-modules`) need that library and go with
it; in the MLX session mlx-capture and MLX Audio do their work, and
`--replace-sound-servers` has already masked them. `apt install
libpipewire-0.3-0t64` (or `libpipewire-0.3-0`) brings PipeWire's back.

## Not there

- The daemon's half of the API (`pw_impl_*`, `pw_global_*`,
  `pw_resource_*`, `pw_protocol_*`, `pw_control_*`, `pw_buffers_*`,
  `pw_work_queue_*`): a program that embeds the daemon would need it;
  these answer NULL or `-ENOTSUP`. The context's module loading
  (`pw_context_load_module`) and `pw_context_connect` to a daemon's
  socket when there is none answer the same way.
- `pw_filter` (the DSP-style port API JACK-like programs use):
  `pw_filter_new` answers NULL and a filter's state is error, with the
  text saying so. Screen sharing, cameras and audio use `pw_stream`.
- Memory pools of the program's own (`pw_mempool_*`, `pw_memblock_*`):
  the library maps the daemon's memory itself.
- The configuration files (`pw_conf_*`): there is no `pipewire.conf` to
  load; they answer `-ENOENT`.
