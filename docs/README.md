# Mlx documentation

This directory has three tiers, from tutorial to normative:

1. **[`guide/`](guide/README.md)** — a chapter-by-chapter introduction to the
   core surface language (basics, operators, control flow, functions, types,
   ownership, errors, comptime/generics, unsafe/safety, modules,
   diagnostics). Start here if you're learning Mlx. Every example is taken
   from a fixture in [`tests/`](../tests).
2. **[`reference/`](#reference)** and **[`internals/`](#internals)** —
   deeper, implementation-grounded references for a specific subsystem:
   the ABI, object formats, the build system, the standard library,
   Wayland, the memory/concurrency model, and the compiler itself
   (both the normative pipeline and the actual self-hosted implementation
   in `compiler/selfhost/`). Come here once you need to know exactly how
   something works, not just how to use it.
3. **[`spec/`](../spec/)** — the normative Mlx 1.0 specification (see
   [`SPEC_INDEX.md`](../SPEC_INDEX.md)). This is the source of truth; every
   page below is descriptive, not normative, and defers to `spec/` wherever
   the two disagree. [`SPEC_CONFLICTS.md`](../SPEC_CONFLICTS.md) tracks open
   normative gaps ("Stage-0 limitations") that recur throughout these pages.

All of the above follow the same rule this repository holds its own
implementation to: describe only what a spec file, test fixture, or source
file actually establishes, and call out gaps explicitly rather than
inventing behavior (see [`SPEC_READY.md`](../SPEC_READY.md)).

## Reference

Grounded in `spec/` plus the concrete implementation and runtime tests that
exercise it:

- [`reference/abi.md`](reference/abi.md) — the internal `mlxcc` x86_64
  calling convention and the foreign/`extern` ABI, including the raw Linux
  syscall path
- [`reference/android.md`](reference/android.md) — the AArch64 backend,
  `--target=aarch64-linux`/`aarch64-android`, APK packaging and signing,
  foreign/exported functions, and `std.android`
- [`reference/formats.md`](reference/formats.md) — ELF64 executable output,
  debug formats, and the (unimplemented) PE32+ target
- [`reference/build-system.md`](reference/build-system.md) — the normative
  build/package/target specs vs. today's Stage-0 Zig build driver
- [`reference/stdlib.md`](reference/stdlib.md) — the standard library, split
  into Stage-1 Core (`std/bootstrap/`) and Stage-1 Extensions (`std/src/`),
  module by module
- [`reference/wayland.md`](reference/wayland.md) — the Wayland protocol-AST
  pipeline and transport layer (expands [`WAYLAND.md`](WAYLAND.md))
- [`reference/vulkan.md`](reference/vulkan.md) — `std.vulkan` materialized
  from `vk.xml`, the driver loader and driver interface, SPIR-V and
  `std.json` (expands [`VULKAN.md`](VULKAN.md))
- [`reference/truetype.md`](reference/truetype.md) — `std.truetype`: TrueType
  parsing, anti-aliased rasterization, atlas, layout, and text on Vulkan
- [`reference/ui.md`](reference/ui.md) — `std.ui`: layout building blocks
  (rectangles, insets, alignment, containers, stacks, reserved bars)
- [`reference/memory-and-concurrency.md`](reference/memory-and-concurrency.md)
  — the memory model (lifetime, UB, overflow) and the atomics/TLS Stage-0 gap
- [`reference/diagnostics-selfhost.md`](reference/diagnostics-selfhost.md) —
  companion to [`guide/11-diagnostics.md`](guide/11-diagnostics.md), covering
  the diagnostic-shaped fixtures the guide scoped out (module-graph errors,
  comptime-quota diagnostics, syntax-error traces)

## Internals

How the compiler itself works, normatively and concretely:

- [`internals/compiler-pipeline.md`](internals/compiler-pipeline.md) — the
  normative pipeline stages, diagnostics model, LIR, and x86_64 backend
  contracts, per `spec/02-compiler/`
- [`internals/selfhost-compiler.md`](internals/selfhost-compiler.md) — an
  architecture tour of `compiler/selfhost/`, the canonical Mlx compiler
  written entirely in Mlx (lexer → parser → sema → comptime → LIR →
  x86_64 backend → ELF64), including where its current implementation is
  narrower than the normative spec

## Architecture notes

- [`BOOTSTRAP.md`](BOOTSTRAP.md) — the Zig-to-self-hosting bootstrap chain
- [`WAYLAND.md`](WAYLAND.md) — why Wayland is a stdlib-bootstrap protocol
  extension, not a compiler feature
- [`VULKAN.md`](VULKAN.md) — native Vulkan without a C loader, and the
  building blocks toward an Mlx Vulkan driver

## Keeping this documentation accurate

A GitHub Actions workflow
([`.github/workflows/docs-conformance.yml`](../.github/workflows/docs-conformance.yml))
runs [`tools/check_docs_coverage.py`](../tools/check_docs_coverage.py) on
every push/PR that touches `tests/`, `docs/`, or the diagnostic code
catalog, and fails the build if:

- any `*.mlx` file under `tests/` (including `tests/modules/` and
  `tests/support/`) isn't cited anywhere under `docs/` — a new fixture that
  lands without a documentation update breaks CI, not just this pass's
  one-time audit
- any relative link in a `docs/**/*.md` file (or the root `README.md`)
  doesn't resolve to a real file
- any ``` code fence is left unclosed
- any `MLX-E`/`MLX-W` code in
  [`spec/02-compiler/diagnostics/codes.xml`](../spec/02-compiler/diagnostics/codes.xml)
  is missing from [`guide/11-diagnostics.md`](guide/11-diagnostics.md)'s code
  table

Run it locally before pushing a docs or test change:

```sh
python3 tools/check_docs_coverage.py
```

It has no dependencies beyond a Python 3 standard library.
