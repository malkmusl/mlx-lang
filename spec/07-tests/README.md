# Mlx conformance tests

A conforming implementation maintains independent suites for lexer, parser, resolution, sema, comptime, ownership/move, unsafe, ABI, LIR/codegen, runtime, stdlib, Wayland wire behavior and diagnostics.

Every compile-error fixture records at minimum the expected stable diagnostic code and primary span. Human-readable prose should be snapshot-tested selectively; JSON diagnostics are the canonical machine-consumable form.

Required diagnostic fixtures include:
- MLX-E4002 integer out of range
- MLX-E4005 non-exhaustive match
- MLX-E4008 unhandled error union
- MLX-E6001 use after @move
- MLX-E6002 copy of @noncopy/@nocopy type
- MLX-E6005 partially moved aggregate misuse
- MLX-E6006 invalid automatic deinitializer signature
- MLX-E7004 unsafe operation outside unsafe block
