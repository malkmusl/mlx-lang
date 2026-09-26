# Mlx terminal

A small terminal emulator written in Mlx with `std.wayland`. It runs a
shell on a pseudo-terminal and draws its output with a built-in 8x8 bitmap
font, in an 80x24 window to start with. The window follows the size its
compositor configures, in whole cells (10x2 up to 256x96), and the shell
learns each new size (`TIOCSWINSZ`, so `stty size` and `$COLUMNS` follow).

```sh
mlx-out/bin/compiler/mlx4 examples/wayland-terminal/main.mlx -o mlx-terminal
./mlx-terminal                 # /bin/sh
./mlx-terminal /bin/bash -l    # another program (absolute path)
```

- Keyboard input uses a US layout with Shift, Ctrl (control characters),
  Caps Lock, arrows, Home, End and Delete. Key repeat follows the
  compositor's `repeat_info`.
- Output is treated as `TERM=dumb`. CR, LF, BS, HT and a small ECMA-48
  subset work: cursor movement, erase in display or line, and insert or
  delete characters. Other escape sequences are ignored.
- Rendering uses two shared-memory buffers paced by frame callbacks.

Files: `main.mlx` (Wayland and the event loop), `screen.mlx` (grid, escape
parser, rendering), `keys.mlx` (key codes to bytes), `pty.mlx` (pty and
process syscalls), `font.mlx` (glyphs from the public-domain font8x8).
It is the default terminal of `examples/wayland-compositor` (Alt+Enter).
