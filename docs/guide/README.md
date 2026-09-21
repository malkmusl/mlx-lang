# Mlx language guide

This guide is a practical, example-driven introduction to Mlx. Every code
sample here is drawn directly from a fixture in [`tests/`](../../tests), the
project's conformance suite, so the examples are guaranteed to compile (or to
fail exactly the way the text says) against the bootstrap compiler (`mlx0`).

This guide is descriptive, not normative. Where the guide and the
specification disagree, the specification wins:

- [`spec/00-language/`](../../spec/00-language) — grammar and language
  semantics (the source of truth)
- [`spec/02-compiler/diagnostics.xml`](../../spec/02-compiler/diagnostics.xml)
  — diagnostic model
- [`SPEC_CONFLICTS.md`](../../SPEC_CONFLICTS.md) — open normative gaps
  ("Stage-0 limitations") that are called out inline below wherever a test
  documents one

Every chapter names the tests it draws on under a **Source tests** line so
you can jump straight to the fixture and, if you want, run it yourself:

```sh
zig build
./zig-out/bin/mlx0 tests/06_control_flow.mlx -o /tmp/out && /tmp/out; echo $?
```

For a compile-error fixture, use `tests/run_error.sh`:

```sh
./tests/run_error.sh tests/09_type_errors.mlx MLX-E4001
```

## Chapters

1. [Basics](01-basics.md) — comments, `const`/`var`, primitive types, casts
2. [Operators](02-operators.md) — arithmetic, bitwise, shift, overflow behavior
3. [Control flow](03-control-flow.md) — `if`, `while`, `for`, `match`, labels
4. [Functions](04-functions.md) — declarations, multiple returns, function pointers
5. [Types and aggregates](05-types-and-aggregates.md) — structs, enums, unions, arrays, slices, optionals, pointers
6. [Ownership](06-ownership.md) — `@nocopy`/`@noncopy`, `@move`, automatic drop
7. [Errors](07-errors.md) — error sets, error unions, `try`, `defer`/`errdefer`
8. [Comptime and generics](08-comptime-and-generics.md) — compile-time parameters, reflection builtins
9. [Unsafe and safety](09-unsafe-and-safety.md) — the `unsafe` boundary, pointers, runtime safety checks
10. [Modules](10-modules.md) — `@import`, visibility
11. [Diagnostics](11-diagnostics.md) — selected `MLX-E`/`MLX-W` codes with fixtures

## Scope

This guide covers the core surface language as exercised by the low-numbered,
single-feature fixtures in `tests/` (roughly `01_*` through `232_*`). It
deliberately stays at the language-usage level and doesn't dive into ABI
register allocation, object-file layout, or compiler-internal data
structures — those live one level deeper, in
[`../reference/`](../reference/) and [`../internals/`](../internals/), which
cover the ABI, object formats, the build system, the standard library,
Wayland, the memory/concurrency model, and the compiler's own pipeline and
self-hosted implementation (`compiler/selfhost/`), each grounded the same
way this guide is: in the normative `spec/`, in real test fixtures
(including the `*_selfhost_*`/`*_bootstrap_*` runtime tests this guide
scopes out), and in the actual source. See [`../README.md`](../README.md)
for the full documentation index.
