# Canonical self-hosted Zin compiler

This directory contains the canonical Zin compiler written entirely in Zin.
`zin0` compiles it into `zin1`; a complete `zin1` will then compile the same
sources into `zin2`. Do not add Zig dependencies here.

The executable scaffold is intentionally small, but its complete runtime
foundation is now available without libc or Zig dependencies:

- `token.zin` defines the stable token representation.
- `lexer.zin` tokenizes identifiers, integers, newlines, and core punctuation.
- `source.zin` owns complete source-file loading through an explicit allocator.
- `diagnostic.zin` provides stable codes, phases, severity, source spans, causes,
  messages, and terminal rendering.
- `main.zin` accepts a source path, loads and tokenizes it, and is the current
  `zin1` entry point and end-to-end bootstrap gate.

The bootstrap std also provides growing byte and record vectors, string symbol
maps, arena/fixed/page allocators, Linux files and process arguments, and direct
ELF64 executable output. This is the foundation needed to implement the full
Zin1 lexer, parser, AST, semantic pipeline, LIR, and backend. This minimal
compiler std is Stage-1 Core. `std.xml`, `std.json`, broader POSIX support,
Wayland, and other protocols are non-blocking Stage-1 Extensions rather than
dependencies of the compiler or self-hosting path.

Build the current scaffold with:

```sh
zig build zin1
zig-out/bin/zin1 tests/01_basic.zin
```
