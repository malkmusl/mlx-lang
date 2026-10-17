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
- Selecting: a drag selects by cells, a double click a word (a path or a
  URL is one), a triple click whole lines, Shift and a click move the
  selection's far end. The selection follows lines that scroll into the
  history. Ctrl+Shift+C or Ctrl+Insert copies it to the clipboard (blanks
  at line ends dropped, a line the program wrapped kept as one);
  Ctrl+Shift+V or Shift+Insert pastes, from any Wayland or X program.
- Pastes are filtered: escape and the other control characters go, so a
  pasted text cannot end bracketed paste early, move the cursor or change
  the terminal's state; line ends become carriage returns. What the
  program does not take at once waits in a queue (16 MiB at most) that
  is written as it reads, so a big paste into a busy program never stops
  the window.
- Files dropped on the window (from the file manager, the dock's stacks,
  any program that drags `text/uri-list`) are typed as their paths, each
  quoted for a shell (`'/tmp/it'\''s'`) and followed by a space.
- Programs that ask for the mouse (`DECSET 9`, `1000`, `1002`, `1003`, in
  X10 or SGR `1006` encoding: vim, htop, mc, tmux) hear its buttons, the
  wheel and the motion they asked for; Shift still selects. On the
  alternate screen without that the wheel sends arrow keys (less, man).

Files: `main.mlx` (the window, the pty and the program, the options). The
emulation is `std/src/terminal.mlx` (the screen), `std/src/terminal/keys.mlx`
(keys to bytes) and `std/src/terminal/pty.mlx` (the pseudo-terminal); the
drawing is `std/src/ui/terminal.mlx`, usable as a pane in any `std.ui`
app. It is the default terminal of `projects/desktop/compositor`
(Alt+Enter); `projects/console` (mlx-console) is the same terminal on the
Linux framebuffer without a compositor. The pointer, the selection's text,
the paste filter and dropped paths are `std/src/ui/terminal_input.mlx`
(`tests/support/terminal_input.mlx` tests them). Not yet: the primary
selection (middle click), blinking.
