# Zin conformance tests

A conforming implementation maintains independent suites for lexer, parser, resolution, sema, comptime, ownership/move, unsafe, ABI, LIR/codegen, runtime, stdlib, Wayland wire behavior and diagnostics.

Every compile-error fixture records at minimum the expected stable diagnostic code and primary span. Human-readable prose should be snapshot-tested selectively; JSON diagnostics are the canonical machine-consumable form.

Required diagnostic fixtures include:
- ZIN-E4002 integer out of range
- ZIN-E4005 non-exhaustive match
- ZIN-E4008 unhandled error union
- ZIN-E6001 use after @move
- ZIN-E6002 copy of @nocopy type
- ZIN-E6005 partially moved aggregate misuse
- ZIN-E7004 unsafe operation outside unsafe block
