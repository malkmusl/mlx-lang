# mlx0 bootstrap compiler

Implement in Zig. Linux x86_64 first. `mlx0` exists only to bootstrap the canonical Mlx compiler. Keep dependencies minimal and follow `spec/` exactly.

Suggested Zig modules:

```text
source.zig
lexer.zig
newline.zig
token.zig
parser.zig
ast.zig
types.zig
sema.zig
comptime.zig
lir.zig
x86_64.zig
regalloc.zig
elf64.zig
link.zig
main.zig
```

## Language server

mlx-lsp is the bootstrap Language Server Protocol implementation. Build it
with the normal project build:

    zig build
    ./zig-out/bin/mlx-lsp

An editor starts the binary over stdio; do not run it through a terminal
wrapper that writes non-protocol text to stdout. The current server supports
full-document synchronization, lexer/parser/sema diagnostics, completion,
document symbols, hover, and go-to-definition for open Mlx documents.
