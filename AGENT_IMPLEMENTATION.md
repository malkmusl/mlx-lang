# Zin 1.0 implementation contract

The files under `spec/` are normative. Implement Zin; do not design it.

## Non-negotiable rules

Do not invent syntax, semantics, ABI behavior, layout, implicit conversions, allocation behavior, standard-library behavior, protocol behavior or compiler extensions. Record unresolved normative conflicts in `SPEC_CONFLICTS.md` and continue unrelated work.

The complete operator algebra in `spec/00-language/operators.xml` is normative; do not simplify or reinterpret composite tokens such as `^<<`, `|<<`, `<<|`, `>>|`, or their `=` forms.

Every implemented feature requires positive tests and negative compile-error tests where applicable. Runtime semantics require runtime tests. ABI features require ABI tests. Every user-facing compiler error must use the structured diagnostic model and stable diagnostic code defined under `spec/02-compiler/diagnostics*`.

Do not add `@protocol`, `@importSchema`, `Build.addProtocol`, or another Wayland/XML compiler special case. JSON and XML are Zin stdlib modules. Wayland XML is consumed during Stage-0/1 standard-library construction so users later import only `std.wayland`.

Do not reinterpret `@nocopy` as a keyword/modifier. Required syntax includes:

```zin
const File = @nocopy(struct) {
    handle: usize
}

const OwnedByte = @nocopy(u8)
var next = @move(file)
```

## Bootstrap stages

### Stage 0 — zin0

Linux x86_64 bootstrap compiler in Zig. Implement enough Zin plus direct ELF64/syscalls to build the Stage-1 foundation. No LLVM, GCC, C compiler, libc, external assembler or external linker is required.

### Stage 1 — bootstrap std and zin1

Build the initial Zin std foundation. Implement `std.xml`, `std.json`, required OS/POSIX primitives and materialize `std.wayland` client+server declarations/runtime from canonical Wayland XML. Then compile the canonical Zin compiler (`zin1`) written in Zin.

### Stage 2 — self hosting

Use `zin1` to compile the canonical compiler into `zin2`; both pass the same conformance suite.

### Stage 3 — full std/toolchain

Rebuild full std and tooling using self-hosted Zin. Zig is no longer needed.

### Stage 4 — brixOS

Use self-hosted Zin to build brixOS. brixOS may use native IPC/syscalls; `std.posix` provides compatibility and `std.wayland` maps its transport to brixOS-native primitives.

## Required implementation order

1. source manager and structured diagnostics substrate
2. lexer/newline filter
3. parser + error recovery
4. AST
5. type representation
6. symbol/module resolution
7. semantic analysis including initialization, `@nocopy` and `@move`
8. comptime evaluator/reflection
9. LIR
10. x86_64 encoder/register allocation
11. zincc ABI
12. ELF64 writer/linker
13. Linux raw/OS layer
14. bootstrap std
15. std.xml/std.json
16. std.wayland protocol materialization + client/server + Linux transport
17. canonical compiler in Zin
18. self-hosting
19. full std
20. BSD + Wayland transport
21. Windows/PE32+
22. brixOS + Wayland transport
23. optimizer/debug/tooling maturation

## Required developer commands

```sh
zin build
zin run
zin test
zin fmt
zin check
zin explain ZIN-E6001
zin check --diagnostic-format=json
```
