# Mlx

Mlx is a minimalist manual-memory systems programming language with a self-hosting toolchain target. The bootstrap compiler (`mlx0`) is written in Zig; the canonical compiler is written in Mlx and must compile itself. Mlx emits native machine code directly and does not require LLVM, GCC, a C compiler, libc, an external assembler, or an external linker for its reference path.

## Repository purpose

This repository is the normative Mlx 1.0 specification suite and implementation workspace. An implementation agent implements the specification; it does not invent language semantics.

## Bootstrap chain

```text
Zig on Linux
   -> mlx0
   -> Stage-1 compiler std foundation
   -> mlx1 (canonical compiler written in Mlx)
      +-> mlx2 (self-compiled compiler)
      +-> Stage-1 extensions: std.xml + std.json + broader std.posix/std.os
          +-> Stage-1 protocol extensions such as std.wayland
   -> full std + tools + brixOS
```

Zig is Stage-0 only. The canonical compiler, standard library, normal Mlx programs and brixOS must not depend on Zig.

## Core rules

- no hidden heap allocation, GC or implicit reference counting
- no exceptions, inheritance, operator overloading, copy/move constructors or implicit destructors
- explicit allocators and explicit cleanup
- `@nocopy(T)` makes a type non-copyable; aggregate syntax is `@nocopy(struct) { ... }`, `@nocopy(enum(u8)) { ... }`, etc.
- `@move(value)` explicitly transfers a tracked value and invalidates its source
- structured compile diagnostics with stable `MLX-E####` / `MLX-W####` codes and JSON output
- deterministic newline statement termination
- mandatory enum backing types
- untagged unions have no hidden tag; tagged unions use enum discriminators
- comptime-first generics/reflection and native multiple returns
- explicit `unsafe {}` boundary for raw safety-bypassing operations
- arbitrary-width `uN`/`iN` integers (1..4096 bits) and a complete composable arithmetic/bitwise/shift operator algebra
- direct x86_64 machine-code generation
- XML/JSON are normal stdlib modules, not compiler schema features
- Wayland client+server are provided by `std.wayland`; canonical XML is consumed during stdlib bootstrap, never required in ordinary project builds

Read `AGENT_IMPLEMENTATION.md` and `SPEC_INDEX.md` before implementation.

## Documentation

- [`docs/guide/`](docs/guide/README.md) — a test-driven language guide: every
  example is taken from a fixture in `tests/` and is guaranteed to compile
  (or fail) exactly as described
- [`spec/`](spec/) — the normative Mlx 1.0 specification (see `SPEC_INDEX.md`)
- [`docs/BOOTSTRAP.md`](docs/BOOTSTRAP.md) / [`docs/WAYLAND.md`](docs/WAYLAND.md) — bootstrap chain and Wayland architecture notes
