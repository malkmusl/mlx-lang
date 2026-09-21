# Diagnostics

**Normative source:** [`spec/02-compiler/diagnostics.xml`](../../spec/02-compiler/diagnostics.xml)
(the diagnostic model) and
[`spec/02-compiler/diagnostics/codes.xml`](../../spec/02-compiler/diagnostics/codes.xml)
(the stable code catalog). This page cross-references that catalog with the
fixtures in `tests/` that exercise each code, where one exists in the guide's
scope (see [README](README.md) for what's out of scope).

Every diagnostic has a stable code (`MLX-E####` for errors, `MLX-W####` for
warnings), a compiler phase, and a severity. Codes are stable for the Mlx 1.x
line; only their human-readable message text may improve. `mlx check
--diagnostic-format=json` emits the machine-readable form, and `mlx explain
CODE` prints a long-form explanation.

A compile-error fixture is checked with `tests/run_error.sh`:

```sh
./tests/run_error.sh tests/09_type_errors.mlx MLX-E4001
```

## Source and lexer (`MLX-E1xxx`)

| Code | Name |
| --- | --- |
| `MLX-E1001` | `InvalidUtf8` |
| `MLX-E1002` | `InvalidCharacter` |
| `MLX-E1003` | `UnterminatedString` |
| `MLX-E1004` | `InvalidEscape` |

## Parser and grammar (`MLX-E2xxx`)

| Code | Name | Example fixture |
| --- | --- | --- |
| `MLX-E2001` | `UnexpectedToken` | — |
| `MLX-E2002` | `ExpectedExpression` | — |
| `MLX-E2003` | `ExpectedClosingDelimiter` | — |
| `MLX-E2004` | `InvalidStatementTermination` | `tests/230_trace_syntax_error.mlx` (invalid statement terminator) |

## Name/module resolution (`MLX-E3xxx`)

| Code | Name | Example fixture |
| --- | --- | --- |
| `MLX-E3001` | `UnknownIdentifier` | — |
| `MLX-E3002` | `DuplicateDeclaration` | — |
| `MLX-E3003` | `PrivateDeclaration` | `tests/76_import_private_error.mlx` — see [Modules](10-modules.md) |
| `MLX-E3004` | `ImportNotFound` | `tests/77_import_not_found_error.mlx` — see [Modules](10-modules.md) |
| `MLX-E3005` | `CyclicComptimeImport` | `tests/modules/157_cycle_a.mlx` / `tests/modules/157_cycle_b.mlx`, exercised by `tests/158_selfhost_module_errors_runtime.mlx` |

## Types, sema, control flow (`MLX-E4xxx`)

| Code | Name | Example fixture |
| --- | --- | --- |
| `MLX-E4001` | `TypeMismatch` | `tests/09_type_errors.mlx` — see [Basics](01-basics.md) |
| `MLX-E4002` | `IntegerOutOfRange` | `tests/19_cast_out_of_range.mlx` — see [Basics](01-basics.md) |
| `MLX-E4003` | `InvalidCast` | — |
| `MLX-E4004` | `MissingReturn` | `tests/32_function_missing_return.mlx`, `tests/41_error_nonvoid_missing_return.mlx` — see [Functions](04-functions.md), [Errors](07-errors.md) |
| `MLX-E4005` | `NonExhaustiveMatch` | `tests/84_match_non_exhaustive.mlx`, `tests/98_nonexhaustive_enum_requires_else.mlx` — see [Control flow](03-control-flow.md) |
| `MLX-E4006` | `InvalidEnumValue` | — |
| `MLX-E4007` | `DuplicateEnumValue` | `tests/99_enum_duplicate_value.mlx` — see [Types and aggregates](05-types-and-aggregates.md) |
| `MLX-E4008` | `UnhandledErrorUnion` | `tests/36_try_requires_error_function.mlx`, `tests/39_unhandled_error_union.mlx` — see [Errors](07-errors.md) |
| `MLX-E4009` | `InvalidDiscardedError` | — |
| `MLX-E4010` | `InvalidNoreturnPath` | — |
| `MLX-E4011` | `InvalidIntegerWidth` | — |
| `MLX-E4012` | `InvalidShiftCount` | `tests/90_invalid_shift_count.mlx` |
| `MLX-E4013` | `ShiftOverflow` | — |
| `MLX-E4014` | `InvalidOperatorOperands` | — |
| `MLX-E4015` | `InvalidCompoundAssignmentTarget` | — |

## Comptime and reflection (`MLX-E5xxx`)

| Code | Name | Example fixture |
| --- | --- | --- |
| `MLX-E5001` | `ComptimeRuntimeDependency` | `tests/74_comptime_runtime_dependency.mlx`, `tests/109_comptime_argument_runtime_dependency.mlx` — see [Comptime and generics](08-comptime-and-generics.md) |
| `MLX-E5002` | `ComptimeBranchQuotaExceeded` | `tests/233_selfhost_comptime_diagnostic_runtime.mlx` |
| `MLX-E5003` | `CompileErrorBuiltin` | `tests/13_compile_error_builtin.mlx` — see [Comptime and generics](08-comptime-and-generics.md) |
| `MLX-E5004` | `ComptimeCycle` | — |
| `MLX-E5005` | `InvalidTypeReflection` | `tests/24_reflection_schema_gap.mlx` (`@languageVersion`) — see [Comptime and generics](08-comptime-and-generics.md) |

## Copyability, `@move`, initialization state (`MLX-E6xxx`)

| Code | Name | Example fixture |
| --- | --- | --- |
| `MLX-E6001` | `UseAfterMove` | `tests/03_move.mlx` — see [Ownership](06-ownership.md) |
| `MLX-E6002` | `CopyOfNoCopyType` | `tests/02_nocopy_error.mlx`, `tests/223_noncopy_local_copy_error.mlx`, `tests/224_noncopy_representation_copy_error.mlx` — see [Ownership](06-ownership.md) |
| `MLX-E6003` | `MoveFromInvalidSource` | — |
| `MLX-E6004` | `UseOfUninitializedValue` | — |
| `MLX-E6005` | `UseOfPartiallyMovedValue` | — |
| `MLX-E6006` | `InvalidAutomaticDeinitializer` | `tests/217_autodrop_signature_error.mlx` — see [Ownership](06-ownership.md) |

## Pointer, memory, safety, atomics (`MLX-E7xxx`)

| Code | Name | Example fixture |
| --- | --- | --- |
| `MLX-E7001` | `InvalidDereference` | — |
| `MLX-E7002` | `AlignmentViolation` | `tests/18_invalid_pointer_alignment.mlx` — see [Unsafe and safety](09-unsafe-and-safety.md) |
| `MLX-E7003` | `InvalidUnionAccess` | — |
| `MLX-E7004` | `UnsafeOperationOutsideUnsafeBlock` | `tests/14_unsafe_ptr_builtin.mlx` — see [Unsafe and safety](09-unsafe-and-safety.md) |
| `MLX-E7005` | `InvalidAtomicOrdering` | — |

## ABI, extern, target legality (`MLX-E8xxx`)

| Code | Name | Example fixture |
| --- | --- | --- |
| `MLX-E8001` | `UnsupportedAbiType` | — |
| `MLX-E8002` | `InvalidExternSignature` | — |
| `MLX-E8003` | `MissingCpuFeature` | — |

## Lowering, codegen, object writer, linker (`MLX-E9xxx`)

| Code | Name | Example fixture |
| --- | --- | --- |
| `MLX-E9001` | `UnsupportedInstructionLowering` | `tests/22_float_builtin_lowering.mlx`, `tests/25_atomic_signature_gap.mlx`, `tests/186_threadlocal_abi_gap.mlx` — documented Stage-0 gaps, confirmed by name in `SPEC_CONFLICTS.md`. `tests/26_vector_signature_gap.mlx` documents the same kind of gap for vector builtins, but `SPEC_CONFLICTS.md` doesn't name its exact code. |
| `MLX-E9002` | `RelocationOverflow` | — |
| `MLX-E9003` | `ObjectFormatViolation` | — |

## Warnings (`MLX-Wxxxx`)

| Code | Name | Example fixture |
| --- | --- | --- |
| `MLX-W3001` | `UnusedLocal` | — |
| `MLX-W3002` | `UnusedImport` | — |
| `MLX-W4001` | `UnreachableCode` | — |
| `MLX-W6001` | `RedundantMove` | `tests/21_redundant_move_warning.mlx` — see [Ownership](06-ownership.md) |

`-Werror` promotes every warning to an error without changing its code.

## A note on completeness

A handful of these codes (marked `—` above) don't yet have a fixture in the
guide's scope, or their fixture lives in the compiler-internals/self-hosting
suite rather than a single-feature test. Following this repository's own
rule ("the implementation must never silently invent behavior"), this guide
lists the code and its normative name from `codes.xml` without inventing a
usage example for it — check the corresponding `spec/00-language/*.xml`
chapter for the rule it enforces.
