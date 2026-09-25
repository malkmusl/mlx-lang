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
- Right-click menus and other `xdg_popup` windows are placed with
  `xdg_positioner` and dismissed by clicking elsewhere.

![Two Mlx terminals, the second moved with Alt+drag](screenshots/mlx-terminals.png)

![weston-terminal with its right-click menu (an xdg_popup)](screenshots/weston-terminal.png)

The screenshots are frames the scripted test host received, taken during
`tools/check_wayland_compositor.sh`-style runs.

| Shortcut | Action |
| --- | --- |
| Alt+Enter | open a terminal (`--terminal`, default `mlx-terminal`) |
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
`wl_output`, `wl_seat` with pointer and keyboard, and `xdg_wm_base` with
toplevels, popups and positioners. There are no subsurfaces or data devices
(clipboard). Composition is done in software, or with Vulkan
(`--renderer vulkan`).

## Files

- `main.mlx`: options, the child environment and the event loop over the
  session connection and the server.
- `host.mlx`: the window on the session compositor and its seat input.
- `shell.mlx`: the server side, with globals, surfaces, shared memory,
  xdg-shell, focus and input delivery, and launching programs.
- `dmabuf.mlx`: linux-dmabuf; each dma-buf becomes a one-buffer pool.
- `vulkan.mlx`: the Vulkan renderer.
- `scene.mlx`: stacking, hit-testing, title bars and software composition.
- `state.mlx`: shared records and list helpers.

`tools/check_wayland_compositor.sh [compiler] [cpu|vulkan]` exercises all
of this with scripted input (`tools/wayland-test-host`) on either renderer;
`tools/check_vulkan_wayland.sh` checks the Vulkan client in both renderers
over `wl_shm` and linux-dmabuf, pixel by pixel.
