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

Options: `--socket NAME`, `--size WxH`, `--terminal PROGRAM`, `--run PROGRAM`
(repeatable), `--screenshot FILE`, `--timeout SECONDS`, `--verbose`.

## Supported protocol

`wl_compositor` (surfaces and regions), `wl_shm` (ARGB8888 and XRGB8888),
`wl_output`, `wl_seat` with pointer and keyboard, and `xdg_wm_base` with
toplevels, popups and positioners. There are no subsurfaces, data devices
(clipboard) or GPU buffers. Composition is done in software.

## Files

- `main.mlx`: options, the child environment and the event loop over the
  session connection and the server.
- `host.mlx`: the window on the session compositor and its seat input.
- `shell.mlx`: the server side, with globals, surfaces, shared memory,
  xdg-shell, focus and input delivery, and launching programs.
- `scene.mlx`: stacking, hit-testing and composition.
- `state.mlx`: shared records and list helpers.

`tools/check_wayland_compositor.sh` exercises all of this with scripted
input (`tools/wayland-test-host`).
