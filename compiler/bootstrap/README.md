# zin0 bootstrap compiler

Implement in Zig. Linux x86_64 first. `zin0` exists only to bootstrap the canonical Zin compiler. Keep dependencies minimal and follow `spec/` exactly.

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
