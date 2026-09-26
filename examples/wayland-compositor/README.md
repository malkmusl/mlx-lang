# Nested Wayland compositor

A Wayland compositor written in Mlx with `std.wayland`. It runs as a window
inside your desktop session and is a compositor in its own right: programs
started with its `WAYLAND_DISPLAY` appear as windows inside it.

- The session's keyboard and pointer are routed to the nested clients. The
  pointer goes to the window under it, a click focuses and raises a window,
  and keys go to the focused window. Clients receive the session's xkb
  keymap and key repeat settings.
- Windows move by dragging their title bar (`xdg_toplevel.move`) or with
  Alt+drag anywhere.
- Windows resize by dragging their border (the cursor shows the
  direction; near a corner, the corner), with Alt+right-drag from the
  nearest corner, or from the client's own edges (`xdg_toplevel.resize`,
  GTK). The opposite edges stay where they are.
- Right-click menus and other `xdg_popup` windows are placed with
  `xdg_positioner` and dismissed by clicking elsewhere.

![Two Mlx terminals, the second moved with Alt+drag](screenshots/mlx-terminals.png)

![weston-terminal with its right-click menu (an xdg_popup)](screenshots/weston-terminal.png)

The screenshots are frames the scripted test host received, taken during
`tools/check_wayland_compositor.sh`-style runs.

| Shortcut | Action |
| --- | --- |
| Alt+Enter | open a terminal (`--terminal`, default `mlx-terminal`) |
| Alt+drag | move the window under the pointer |
| Alt+right-drag | resize the window under the pointer from its nearest corner |
| Alt+Tab | switch windows |
| Alt+F4 | close the focused window |
| Alt+Shift+Q | quit |

## Run it

From the repository root, with the self-hosted compiler built (see
`docs/internals/selfhost-compiler.md`):

```sh
mlx-out/bin/compiler/mlx4 examples/wayland-compositor/main.mlx -o mlx-compositor
mlx-out/bin/compiler/mlx4 examples/wayland-terminal/main.mlx -o mlx-terminal
./mlx-compositor --run ./mlx-terminal
```

Press Alt+Enter for more terminals. Any Wayland program works as well, for
example `WAYLAND_DISPLAY=wayland-mlx weston-terminal` or `--run foot`. The
compositor needs a Wayland session (GNOME, KDE Plasma, sway, ...). On X11,
start `weston` first and run it inside weston.

GTK 3 and GTK 4 applications run too, Nautilus for example:

![Nautilus (GTK 4) in the compositor](screenshots/nautilus.png)

Programs that run as a single D-Bus application (Nautilus, gnome-text-editor,
most GNOME apps) hand a new window to the copy already running in your
session, which opens it on your desktop instead. Start them on a D-Bus bus
of their own to get a new copy inside the compositor:

```sh
WAYLAND_DISPLAY=wayland-mlx dbus-run-session nautilus
```

Options: `--socket NAME`, `--size WxH`, `--renderer cpu|vulkan`,
`--font PATH|none`, `--terminal PROGRAM`, `--run PROGRAM` (repeatable),
`--screenshot FILE`, `--timeout SECONDS`, `--verbose`.

## Title bars

Every window gets a title bar above its frame (the focus color when it has
the keyboard) showing its `xdg_toplevel` title, drawn with
[`std.truetype`](../../docs/reference/truetype.md) from `--font` (default
DejaVu Sans; titles are left out when it cannot be read). Dragging a title
bar moves the window. Both renderers draw titles identically: the CPU with
`std.truetype.drawRun`, Vulkan with the `text` compute shader.

## Resizing

During a resize the window gets its new size in `xdg_toplevel.configure`
with the `resizing` state, one size at a time: the next goes out once the
client has acknowledged the last one and committed a buffer for it, so a
slow client is never flooded. Clients may snap to their own steps (the
terminals to whole cells); when a window grows from its left or top edge,
it is moved as its size arrives so that its right and bottom edges stay
put. Releasing the button sends the final size without `resizing`. Sizes
stay within the client's `set_min_size`/`set_max_size` and never go below
64 x 32. With `--verbose` the log shows each new window size.

![gtk4-demo shrunk by its border, then with Alt+right-drag](screenshots/gtk4-resized.png)

## Vulkan

With `--renderer vulkan` the output is composed on the GPU by the blit
compute shader of `examples/vulkan-shared`, through `std.vulkan` (no C
loader; `VK_DRIVER_FILES` picks a driver). Client buffers are read where
they are: `wl_shm` pools are imported as host memory
(`VK_EXT_external_memory_host`), linux-dmabuf buffers as dma-bufs, and the
frame is written straight into the buffer shown in the host window. A
buffer is therefore kept until the client commits the next one, then
released. The result is pixel-identical to the CPU renderer.

![The Vulkan client (examples/vulkan-wayland-client) in the Vulkan renderer](screenshots/vulkan-client.png)

```sh
mlx4 examples/vulkan-wayland-client/main.mlx -o vulkan-wayland-client
./mlx-compositor --renderer vulkan --run ./vulkan-wayland-client
```

## Supported protocol

`wl_compositor` (surfaces and regions), `wl_shm` (ARGB8888 and XRGB8888),
`zwp_linux_dmabuf_v1` version 3 (ARGB8888 and XRGB8888, linear, one plane),
`wl_output`, `wl_seat` with pointer and keyboard, `wl_data_device_manager`
version 3 and `xdg_wm_base` with toplevels (with `configure_bounds`: the
output's size), popups and positioners. There are no subsurfaces.
Composition is done in software, or with Vulkan (`--renderer vulkan`).

## Copy and paste

`wl_data_device_manager` carries the clipboard between clients (GTK 4
refuses a display without it): the selection is the data source a client
set last, the focused client receives it as a data offer (when it gains
focus and when the selection changes), and reading an offer passes the
reader's pipe to the source's client, which writes the data into it.
Drag and drop is not supported: the source of a drag is cancelled at
once.

## Files

- `main.mlx`: options, the child environment and the event loop over the
  session connection and the server.
- `host.mlx`: the window on the session compositor and its seat input.
- `shell.mlx`: the server side, with globals, surfaces, shared memory,
  xdg-shell, focus and input delivery, and launching programs.
- `dmabuf.mlx`: linux-dmabuf; each dma-buf becomes a one-buffer pool.
- `data.mlx`: `wl_data_device_manager`, copy and paste between clients.
- `vulkan.mlx`: the Vulkan renderer.
- `scene.mlx`: stacking, hit-testing, title bars and software composition.
- `state.mlx`: shared records and list helpers.

`tools/check_wayland_compositor.sh [compiler] [cpu|vulkan]` exercises all
of this with scripted input (`tools/wayland-test-host`) on either renderer,
with copy and paste between two clients (`wl-copy`, `wl-paste`) and a GTK 4
window (`gtk4-widget-factory`) when those are installed;
`tools/check_vulkan_wayland.sh` checks the Vulkan client in both renderers
over `wl_shm` and linux-dmabuf, pixel by pixel.
