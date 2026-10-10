# mlx-console

The Mlx terminal on the Linux console, without a compositor: the kernel's
framebuffer is the screen, the keyboards are read from their evdev nodes
and go through the xkb keymap (`std.xkb`), the shell runs on a pty, and
everything is drawn with the monospaced TrueType font (`std.truetype`
through `std.ui.text`). The emulation and the drawing are the same code
as `mlx-terminal`'s (`std.terminal`, `std.ui.terminal`); only the window
and the input differ. It is what a system without a desktop boots into,
and a fallback when the compositor is not running.

```sh
mlx-out/bin/compiler/mlx4 projects/console/main.mlx -o mlx-console
sudo ./mlx-console                   # /dev/fb0, /dev/input/event*, /dev/tty0 in graphics mode
sudo ./mlx-console --layout de --variant nodeadkeys --font-size 20
./mlx-console --framebuffer screen.argb --size 640x400 --keyboard keys.fifo -- /bin/sh
tools/check_console.sh               # the end-to-end check (no device needed)
```

Options: `--framebuffer PATH` (default `/dev/fb0`; with `--size WxH` a
plain file of ARGB pixels is made instead, for tests and headless
drawing), `--keyboard PATH` (one file of `struct input_event` records,
such as a FIFO, instead of the keyboards), `--tty PATH` (the console to
put into graphics mode, default `/dev/tty0` when a device is used) or
`--no-tty`, `--font-size N` (default 16, or `MLX_CONSOLE_FONT_SIZE`),
`--font FILE` (a TrueType file; the default is the monospaced font
`std.ui.text` finds: DejaVu Sans Mono, Liberation Mono, Noto Sans Mono),
`--layout L` with `--variant`, `--model`, `--options` (default
`XKB_DEFAULT_LAYOUT` and friends, else `us`), `--history N` (default
5000), `-- PROGRAM ARGS...` (default `$SHELL`, else `mlx-sh` beside the
program or installed, else `/bin/sh`). The console ends with the
program's status, and with 0 on SIGTERM, SIGINT or SIGHUP after putting
the console back into text mode.

## How it works

- `framebuffer.mlx`: `/dev/fb0` is read with `FBIOGET_VSCREENINFO` and
  `FBIOGET_FSCREENINFO` (32 bits per pixel; red in the third or the first
  byte) and mapped; the terminal is drawn on an ARGB canvas of the
  screen's size and copied over row by row whenever the screen changed.
  A file framebuffer is the same without the device.
- `input.mlx`: every `/dev/input/eventN` whose capabilities include
  letters and a space bar is a keyboard (mice and buttons are left
  alone); they are read without blocking, and `/dev/input` is looked at
  again every two seconds for a keyboard plugged in. Key events (press,
  release, the kernel's auto-repeat) update the keymap's state
  (`std.xkb.state`); the text a key types now and the text it types
  without Ctrl and Alt come from `std.ui.wayland.keyboard`'s logic (the
  same the desktop apps use), so Ctrl+C works on any layout and AltGr
  types the third level. The keymap is compiled by `std.xkb.compile` from
  the XKB data (`xkb-data`); without it keys follow a US layout.
- `main.mlx`: the console is put into `KD_GRAPHICS` with its keyboard in
  `K_OFF` (so the kernel neither draws text over the screen nor acts on
  the keys) and restored at the end; the program runs on a pty of as many
  cells as fit; one `poll` waits on a signal descriptor, the pty and the
  keyboards. Keys are encoded as xterm does (`std.terminal.keys`),
  Shift+Page Up and Shift+Page Down scroll through the history.

`tools/check_console.sh` runs the console on a file framebuffer with a
FIFO of key events, types `hello` and Enter into `sh -c 'read x; echo
"got $x"'`, and compares the file's pixels with
`tests/support/console_expected.mlx`, an independent rendering of the same
bytes; then SIGTERM, the program's exit status, and the options.

Not yet: switching virtual terminals (`VT_SETMODE`/`VT_RELDISP`, the
console keeps the VT it was started on), a mouse for selecting text, 16
and 24 bits per pixel, a cursor that blinks, the bell, DRM/KMS as the
screen (the framebuffer device is enough for a console; the compositor
drives DRM).
