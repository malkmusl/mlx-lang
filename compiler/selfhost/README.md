# Canonical self-hosted Zin compiler

This directory contains the canonical Zin compiler written entirely in Zin.
`zin0` compiles it into `zin1`; a complete `zin1` will then compile the same
sources into `zin2`. Do not add Zig dependencies here.

The executable scaffold is intentionally small, but its complete runtime
foundation is now available without libc or Zig dependencies:

- `token.zin` defines the stable token representation.
- `lexer/` implements the complete normative token set, longest-match operator
  algebra, nested comments, literals, newline filtering, and growing token
  storage. `lexer.zin` remains its compact public facade.
- `ast/` owns the compact indexed syntax-node store.
- `parser/` implements declarations, types, Pratt expressions, statements,
  control flow, and bounded error recovery. `parser.zin` is its public facade.
- `sema/types/` owns canonical type interning, layouts, coercion checks and
  syntax-type resolution; `types.zin` is its public facade.
- `sema/symbols/` owns declaration symbols, nested lexical scopes, duplicate
  detection and move states; `symbols.zin` is its public facade.
- `sema/declarations.zin` resolves top-level binding and function signatures
  before body analysis, including native tuple return types.
- `sema/functions.zin` and `sema/control_flow.zin` bind parameters and locals,
  validate calls/returns, and analyze `if`, `while`, `for`, `break` and
  `continue` paths without expanding the declaration facade.
- `sema/builtins/` registers the complete normative `@` builtin namespace and
  centralizes arity/result typing; `builtins.zin` is its public facade.
- `sema/comptime/` evaluates bootstrap integer/boolean constants, branches and
  layout predicates under a bounded evaluation quota; `comptime.zin` is its
  public facade.
- `source.zin` owns complete source-file loading through an explicit allocator.
- `diagnostic.zin` provides stable codes, phases, severity, source spans, causes,
  messages, and terminal rendering.
- `main.zin` accepts a source path, loads, tokenizes, and parses it, and is the
  current `zin1` entry point and lexer/parser/declaration-sema bootstrap gate.

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
