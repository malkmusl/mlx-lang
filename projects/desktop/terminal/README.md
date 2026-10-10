# Mlx terminal

The terminal emulator of the Mlx desktop, written in Mlx: a window
(`std.ui.host`) around `std.terminal`, drawn with the monospaced TrueType
font `std.ui.text` finds (DejaVu Sans Mono, Liberation Mono or Noto Sans
Mono, through `std.truetype`, any size), taking its keys from the
compositor's keymap (`std.xkb` through the host, so any layout types what
its keys say) and encoding them as xterm does.

```sh
mlx-out/bin/compiler/mlx4 projects/desktop/terminal/main.mlx -o mlx-terminal
./mlx-terminal                       # $SHELL, else mlx-sh beside it or installed, else /bin/sh
./mlx-terminal /bin/bash -l          # another program (absolute path)
./mlx-terminal --font-size 18        # or MLX_TERMINAL_FONT_SIZE
./mlx-terminal --columns 120 --rows 40
tools/check_terminal.sh              # the emulation, the drawing, the pty
```

- `TERM=xterm-256color`, `COLORTERM=truecolor`: the 16 colours, the
  256-colour table and direct colours, bold, dim, italic, underline,
  inverse, strike-through; cursor movement, erasing, inserting and
  deleting, scrolling regions, the alternate screen (vim, less, htop),
  bracketed paste, the title (OSC 0 and 2) in the window's title bar.
- UTF-8 with wide characters (CJK, emoji) taking two cells; the DEC
  line-drawing set.
- 5000 lines of history: the wheel and Shift+Page Up/Down scroll through
  it, any key comes back to the live screen.
- The window follows the size its compositor configures, in whole cells,
  and the shell learns each new size (`TIOCSWINSZ`, so `stty size` and
  `$COLUMNS` follow). It starts at 80x24.
- Keys: Ctrl with any key as the keymap names it (Ctrl+C on a German
  keyboard is Ctrl+C), Alt as an ESC prefix, cursor and function keys
  with their modifiers, Backspace as DEL, Shift+Tab, AltGr and Ctrl+Alt
  third levels.

Files: `main.mlx` (the window, the pty and the program, the options). The
emulation is `std/src/terminal.mlx` (the screen), `std/src/terminal/keys.mlx`
(keys to bytes) and `std/src/terminal/pty.mlx` (the pseudo-terminal); the
drawing is `std/src/ui/terminal.mlx`, usable as a pane in any `std.ui`
app. It is the default terminal of `projects/desktop/compositor`
(Alt+Enter). Not yet: selecting text and the clipboard, mouse reporting
to programs, blinking.
