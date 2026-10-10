# mlx-sh

The shell of MLX/Linux, written in Mlx without libc: a POSIX shell in
the shape of dash (scripts written for `/bin/sh` run as they do there)
with a line editor for the terminal. It is what `mlx-terminal` and
`mlx-console` start, what `mlx-init` services run their `exec = /bin/sh
-c` lines with once installed as `/bin/sh`, and what a script's
`#!/bin/sh` reaches.

```sh
mlx-out/bin/compiler/mlx4 projects/shell/main.mlx -o mlx-sh
./mlx-sh                             # interactive on a terminal
./mlx-sh script.sh arg...            # a script (also by #! and from stdin)
./mlx-sh -c 'for f in *.md; do echo "$f"; done'
./mlx-sh -s arg... < script.sh       # stdin with positional parameters
./mlx-sh --parse 'if true; then echo x; fi'   # the tree, for debugging
tools/check_shell.sh                 # against dash, the editor on a pty
```

Options: `-c TEXT`, `-s`, `-i` (interactive even without a terminal),
`-l` or `--login` (a login shell), `-e` (exit on a failing command),
`-x` (trace what runs), `--`. Interactive shells read `~/.mlx_shrc`,
keep `~/.mlx_sh_history` (1000 lines) and take the prompt from `PS1`
(`\u` `\h` `\H` `\w` `\W` `\$` `\n`, default `\u@\h:\w\$ `).

## The language

- Words, quotes (`'...'`, `"..."`, `\`), comments, line joining; `~` and
  `~/path`.
- Parameters: `$NAME`, `${NAME}`, the positionals `$1..$9` and `${10}`,
  `$#` `$@` `$*` `$?` `$$` `$!` `$0` `$-`; `${NAME:-word}` `${NAME-word}`
  `${NAME:=word}` `${NAME:+word}` `${NAME:?word}`; `${#NAME}`;
  `${NAME#pattern}` `${NAME##pattern}` `${NAME%pattern}` `${NAME%%pattern}`;
  `${NAME:offset:length}`. Variables are exported with `export`, kept
  with `readonly`, scoped with `local`, imported from the environment
  and passed on to what runs.
- Arithmetic `$(( ... ))` on 64-bit signed integers: `+ - * / %`, `**`
  is not, `<< >>`, comparisons, `! ~ && || & | ^`, `? :`, assignments
  (`=` `+=` `-=` `*=` `/=` `%=` `<<=` `>>=` `&=` `|=` `^=`), decimal,
  `0x` and `0` octal numbers, variables by name.
- Command substitution `$(...)` and `` `...` `` (the child runs in a
  fork, trailing newlines dropped), field splitting on `IFS`, pathname
  expansion (`*` `?` `[...]`, `set -f` turns it off; a pattern without a
  match stays), quote removal.
- Redirections `<` `>` `>|` `>>` `<>` `<&` `>&` `<<` `<<-` with quoted
  or unquoted delimiters, on any descriptor, before or after the words;
  `2>&1`, `&>` is not.
- Pipelines (every command in a child, the status of the last), `!`,
  `&&` `||`, `;`, `&` (background jobs; `wait`, `jobs`, `$!`),
  newlines.
- `if`/`elif`/`else`, `for ... in`, `for` over the positionals, `while`,
  `until`, `case` with `|` alternatives and `;;`, `{ ...; }`, `( ... )`
  subshells, `name() { ...; }` and `name() ( ... )` functions with
  `return`, `break` and `continue` with a level.
- Builtins: `:` `.`/`source` `[`/`test` (strings, numbers, files, `-a`
  `-o` `!` `( )`) `alias` (says it has none) `break` `cd` (`-`, `CDPATH`
  not) `command` `continue` `echo` (`-n`; backslash escapes as dash interprets them,
  `-E` leaves them, `-e` is accepted) `eval` `exec`
  `exit` `export` `false` `hash` `help` `history` `jobs` `kill` `local`
  `printf` (`%d %i %u %o %x %X %c %s %b %%`, widths and precision,
  `\n`-style escapes) `pwd` `read` (`-r`, several names, `IFS`) `readonly`
  `return` `set` (`-e -f -x -u --`) `shift` `trap` (`EXIT` only) `true`
  `type` `umask` `unset` `wait`. External programs are found on `PATH`;
  a script without `#!` runs in a new mlx-sh.
- `set -e` leaves on a failing command (not inside conditions, `!`, the
  left sides of `&&` `||`), `set -x` traces each command on stderr,
  `set -u` is accepted and does nothing yet.

## The line editor

On a terminal (both stdin and stdout) the shell reads lines itself,
with the terminal in raw mode only while a line is being edited: cursor
keys and Home/End, Ctrl-A/E (line ends), Ctrl-B/F (a character),
Alt-B/F (a word), Backspace and Delete, Ctrl-W (the word before),
Ctrl-U (to the start), Ctrl-K (to the end), Ctrl-L (clear), Up/Down and
Ctrl-P/N (the history), Ctrl-R is not, Tab (completion of a command
name from the builtins, functions and `PATH`, of a path, of a `$name`),
Ctrl-C (drop the line, `^C`), Ctrl-D on an empty line (leave). A line
whose parse is incomplete gets `> ` for its next line. The editor only
needs cursor movement (`CSI D`, `CSI K`, `\r`), so any terminal works,
`mlx-terminal` and `mlx-console` as the Linux console and other
terminals.

## Inside

One page-allocated `Shell` holds everything in fixed arenas (no malloc):
the source and what heredocs add to it (1 MiB), the parse tree as nodes
(32768), the variables (512 KiB), the functions' bodies (256 KiB), the
positional parameters, a work stack (8 MiB, with marks: expansions and
argument vectors are built on it and released after the command), the
history and the `local` saves.

- `sys.mlx`: the syscalls, termios, signals.
- `state.mlx`: the `Shell`, the arenas, variables, functions,
  positionals, messages.
- `parser.mlx`: a tokenizer and recursive descent into nodes (words are
  kept raw and expanded when they run; heredoc bodies are gathered after
  their line); says `incomplete` when more input would make a program.
- `expand.mlx`: the expansions into fields with their quoting flags, on
  a growing builder; `glob.mlx` patterns and directories; `arith.mlx`.
- `exec.mlx`: running the tree (lists, and-or, pipelines, redirections
  with save and restore, compound commands, functions); `programs.mlx`
  finds and starts external programs; `builtins.mlx` the builtins.
- `editor.mlx`: the line editor and the history; `main.mlx`: the
  options, the prompt, the interactive loop, scripts.

Not yet: job control (`fg`, `bg`, `Ctrl-Z`), `trap` for signals other
than EXIT, aliases, `set -u`, `getopts`, `times`, `ulimit`, here-strings
(`<<<`, not POSIX), `Ctrl-R` search, multi-line editing of a previous
command.
