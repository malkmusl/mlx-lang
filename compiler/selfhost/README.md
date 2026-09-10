# Canonical self-hosted Mlx compiler

This directory contains the canonical Mlx compiler written entirely in Mlx.
`mlx0` compiles it into `mlx1`; a complete `mlx1` will then compile the same
sources into `mlx2`. Do not add Zig dependencies here.

The executable entry point is intentionally small, while the compiler and its
runtime foundation remain independent of libc and Zig dependencies:

- `token.mlx` defines the stable token representation.
- `lexer/` implements the complete normative token set, longest-match operator
  algebra, nested comments, literals, newline filtering, and growing token
  storage. `lexer.mlx` remains its compact public facade.
- `ast/` owns the compact indexed syntax-node store.
- `parser/` implements declarations, types, Pratt expressions, statements,
  control flow, and bounded error recovery. `parser.mlx` is its public facade.
- `sema/types/` owns canonical type interning, layouts, coercion checks and
  syntax-type resolution. Struct, enum, union and tuple layouts retain field
  metadata for layout/reflection builtins; `types.mlx` is its public facade.
- `sema/symbols/` owns declaration symbols, nested lexical scopes, duplicate
  detection and move states; `symbols.mlx` is its public facade.
- `sema/declarations.mlx` resolves top-level binding and function signatures
  before body analysis, including native tuple return types.
- `sema/functions.mlx` and `sema/control_flow.mlx` bind parameters and locals,
  validate calls/returns, and analyze `if`, `while`, `for`, `break` and
  `continue` paths without expanding the declaration facade.
- `sema/builtins/` registers the complete normative `@` builtin namespace and
  centralizes arity/result typing; `builtins.mlx` is its public facade.
- `sema/comptime/` evaluates bootstrap integer/boolean constants, branches and
  layout predicates under a bounded evaluation quota; `comptime.mlx` is its
  public facade.
- `modules/` classifies and normalizes every normative import form, recursively
  loads a deduplicated cycle-safe source graph, owns per-file scopes, and binds
  public module namespaces into semantic field lookup; `modules.mlx` is its
  public facade.
- `source.mlx` owns complete source-file loading through an explicit allocator.
- `diagnostic.mlx` provides stable codes, phases, severity, source spans, causes,
  messages, and terminal rendering.
- `driver/` owns command-line options, progress/trace reporting, frontend
  analysis, lowering, backend orchestration and cleanup; `main.mlx` is the thin
  executable entry point.
- `backend/x86_64/codegen/` separates mutable backend state from label,
  memory, arithmetic, value, call and control-flow instruction emission;
  `backend/x86_64/codegen.mlx` is the public `Codegen` facade.

The bootstrap std also provides growing byte and record vectors, string symbol
maps, arena/fixed/page allocators, Linux files and process arguments, and direct
ELF64 executable output. This is the foundation needed to implement the full
Mlx1 lexer, parser, AST, semantic pipeline, LIR, and backend. This minimal
compiler std is Stage-1 Core. `std.xml`, `std.json`, broader POSIX support,
Wayland, and other protocols are non-blocking Stage-1 Extensions rather than
dependencies of the compiler or self-hosting path.

Build the self-hosted compiler with:

```sh
zig build mlx1
zig-out/bin/mlx1 tests/01_basic.mlx
```

Verify deterministic self-hosting with:

```sh
zig-out/bin/mlx1 compiler/selfhost/main.mlx -o /tmp/mlx2
/tmp/mlx2 compiler/selfhost/main.mlx -o /tmp/mlx3
/tmp/mlx3 compiler/selfhost/main.mlx -o /tmp/mlx4
cmp /tmp/mlx3 /tmp/mlx4
```
