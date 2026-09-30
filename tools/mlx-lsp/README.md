# mlx-lsp

The Mlx language server, written in Mlx. It replaces the Zig server
(`compiler/bootstrap/lsp.zig`), which ran the bootstrap frontend and flagged
valid code.

- **What it knows.** It reads every `.mlx` file of the workspace through
  the code index ([`tools/codemap/index.mlx`](../codemap/index.mlx)). That
  covers `std`, the compiler, `examples`, `tools`, `tests` and anything
  else, so a name in one example resolves to its declaration in another
  example's file. The index uses the compiler's own lexer and parser.
- **Unsaved changes.** The editor's unsaved text replaces the file on
  disk, and the workspace is read again 300 ms after a change (about 0.6 s
  for this repository).
- **Errors.** They come from the compiler itself: `mlx4` without `-o` only
  analyzes. It runs in the background when a file is opened or saved. If
  a line has an error and a name on it that nothing declares, the message
  names it: `(not declared: \`totl\`)`.

| Request | What it gives |
|---|---|
| definition | where the name is declared (an import's name: the file) |
| references | every use in the workspace |
| hover | signature, type (when not written down), doc comment, place |
| documentSymbol | the file's outline, structs with their members |
| workspace/symbol | declarations whose name contains the query |
| documentHighlight | the name's uses in the file (writes marked) |
| prepareRename, rename | the declaration and every use, in every file |
| completion | members after `.` (a module's public ones, a struct's fields and functions, through pointers), what is in scope, keywords, builtins after `@` |
| signatureHelp | the called function's signature and the parameter the cursor is at |
| semanticTokens/full | kinds of names for highlighting (function, method, type, field, enum member, parameter, variable, namespace) |
| publishDiagnostics | the compiler's errors |

The workspace is the folder the editor opens. It moves up to the folder
that holds `std/src`, because that is where the compiler finds
`@import("std")`. The compiler comes from:

1. `MLX_COMPILER`;
2. else the workspace's `mlx-out/bin/compiler/mlx4`;
3. else `mlx4` on the `PATH`.

## Build and install

```
tools/build_vscode_extension.sh      # vscode-extension/bin/mlx-lsp and the .vsix
code --install-extension vscode-extension/mlx-vscode-extension-0.2.0.vsix
tools/check_lsp.sh                   # a scripted editor session, every request
```

## Files

- `main.mlx`: the loop, the documents, reading the workspace, and the
  requests.
- `features.mlx`: the answers, built from the index.
- `check.mlx`: the compiler as a child process, and its output turned into
  diagnostics.
- `protocol.mlx`: framing, JSON, URIs, and UTF-16 positions.

## What the code map knows

The server also tells what MLX Codemap knows
([`insights.mlx`](insights.mlx), on `tools/codemap`):

- **Hover** of a function: the crashes that stopped in it or went through
  it (from `~/.local/state/mlx/crashes.log`), its workarounds and what is
  missing for them, its complexity, and which programs reach it and
  whether a check does (or that nothing reaches it).
- **Code lens** above each function: how often it is used, and what is
  notable: crashes, workarounds, complex, unreachable, no check.
- **Diagnostics** (source `mlx-codemap`): a warning on the line a crash
  stopped on and on each call it went through (the compiler's line table
  says the line); a hint on every workaround.

It is worked out again after the workspace is read again, when first
asked for. Workarounds are found per file: the whole code base takes a
while.
