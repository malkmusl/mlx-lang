# Compiler pipeline

**Normative source:** `spec/02-compiler/pipeline.xml`, `spec/02-compiler/sema.xml`,
`spec/02-compiler/lir.xml`, `spec/02-compiler/LIR_SEMANTICS.md`,
`spec/02-compiler/x86_64.xml`, `spec/02-compiler/diagnostics.xml`,
`spec/02-compiler/diagnostics/codes.xml`,
`spec/02-compiler/diagnostics/error-classes.xml`,
`spec/02-compiler/diagnostics/json-format.xml`

This page describes the compiler pipeline, LIR, x86_64 backend, and
diagnostic model as they are specified normatively — i.e. the contract any
conforming Mlx compiler (the Zig bootstrap `mlx0` or the self-hosted `mlx1`
and later) must meet. It does not describe a particular implementation's
internal module layout; for that, see
[`docs/internals/selfhost-compiler.md`](selfhost-compiler.md) (the
self-hosted compiler's actual architecture) or the Zig sources under
`compiler/bootstrap/`. For the full catalog of diagnostic codes with test
fixtures, see [`docs/guide/11-diagnostics.md`](../guide/11-diagnostics.md) —
this page covers the diagnostic *model* (phases, determinism, recovery,
families), not the code-by-code table.

## Pipeline stages

`spec/02-compiler/pipeline.xml` defines twelve normative stages, in order:

| # | Stage | Responsibility |
| --- | --- | --- |
| 1 | `SourceManager` | owns source file identity used later for diagnostic spans |
| 2 | `Lexer` | UTF-8 decoding, tokenization, comments, physical newlines |
| 3 | `NewlineFilter` | turns physical newlines into `STATEMENT_END` tokens |
| 4 | `Parser` | recursive-declaration parser plus a Pratt expression parser |
| 5 | `AST` | the parsed tree the rest of the pipeline consumes |
| 6 | `Sema` | symbols, types, coercions, layouts, exhaustiveness, errors, comptime |
| 7 | `LIRLowering` | AST/sema output lowered to the typed IR described below |
| 8 | `Optimization` | minimal mandatory canonicalization; advanced optimization is optional for a bootstrap-grade compiler |
| 9 | `RegisterAllocation` | LIR values assigned to physical registers/stack slots |
| 10 | `x86_64Encoding` | direct machine-code byte emission |
| 11 | `ObjectWriter` | encoded instructions written to a relocatable object |
| 12 | `IntegratedLink` | the compiler links the final executable itself — no external linker is required |

The diagnostics model (`spec/02-compiler/diagnostics.xml`) uses a
slightly coarser-grained set of nine named phases for attributing a
diagnostic to where it was raised: `source, lexer, parser, resolve, sema,
comptime, lowering, codegen, linker, stdlib` (ten entries — `stdlib`
covers diagnostics raised by standard-library comptime code rather than the
compiler itself). Name resolution (`resolve`) and comptime evaluation
(`comptime`) are broken out of `Sema`/`LIRLowering` here because they can
each independently be the origin phase of an error even though the pipeline
table above treats them as part of stage 6/7's work. Every diagnostic code
in `codes.xml` is tagged with exactly one of these phases, e.g.:

```xml
<Code id="MLX-E4001" name="TypeMismatch" phase="sema"/>
<Code id="MLX-E9001" name="UnsupportedInstructionLowering" phase="codegen"/>
<Code id="MLX-E9002" name="RelocationOverflow" phase="linker"/>
```

(`spec/02-compiler/diagnostics/codes.xml`)

### Runtime safety is orthogonal to optimization level

`RuntimeSafety` is specified as its own cross-cutting switch
(`--safety=on` / `--safety=off`, default on), explicitly independent of
optimization level:

> Safety-enabled lowering emits defined traps for checked integer overflow,
> invalid shift counts, null optional-pointer unwraps, and out-of-bounds
> array or slice operations. Disabling runtime safety removes checks that
> are not inherently enforced by the target instruction set and does not
> change static semantic validation.

(`spec/02-compiler/pipeline.xml`)

In other words, turning safety off never changes what compiles — it only
removes the runtime traps that a checked operation would otherwise lower
to; anything that is a compile-time error stays a compile-time error
regardless of `--safety`.

## Sema responsibilities

`spec/02-compiler/sema.xml` enumerates what semantic analysis is
responsible for in one list: name resolution; visibility; type inference;
comptime evaluation; conversion validation; aggregate layout; enum
validation; tagged union tag validation; match exhaustiveness; definite
assignment where required; function return analysis; `noreturn`
validation; generic instantiation; error-set resolution.

Three rules are called out explicitly as attributes rather than prose,
which makes them easy to miss but load-bearing:

- `NoImplicitRuntimeNarrowing = true` — sema never inserts a narrowing
  integer conversion implicitly; a narrowing must be an explicit
  `@intCast` (or equivalent), checked against the source value's range
  when that range is comptime-known.
- `FunctionArgsEvaluation = left-to-right` — call argument evaluation
  order is fixed, not unspecified, which matters once arguments can have
  side effects.
- `RuntimeReflection = false` — all of the `@typeOf`/`@sizeOf`/`@hasField`/…
  reflection surface (cataloged in
  [`docs/guide/08-comptime-and-generics.md`](../guide/08-comptime-and-generics.md))
  is resolved entirely at compile time; there is no type-info value that
  survives into the running program.

`Monomorphization` states the dedup rule for generic instantiation:
"every distinct comptime generic instantiation produces a specialized
semantic instance subject to deduplication by identical canonical comptime
arguments" — i.e. calling `choose(u8, 10)` twice with the same `T` does not
produce two specializations.

## LIR (Low-level Intermediate Representation)

`spec/02-compiler/lir.xml` defines LIR's model as a "typed SSA-capable
linear intermediate representation with explicit basic blocks." The
bootstrap compiler is explicitly permitted to lower phi nodes through
stack temporaries instead of true SSA registers, but the *canonical*
semantics (what an optimizer or another backend may assume) remain
SSA-compatible — this is a bootstrap-implementation latitude, not a
semantic escape hatch.

### Types and instruction set

LIR's value types are: `iN f32 f64 ptr array slice tuple struct enum union
vector void` — `iN` covers all integer widths (both signed and unsigned;
see below), not a fixed set of machine widths.

The instruction set, as listed in `lir.xml`, spans constant/copy,
arithmetic (checked, wrapping, and saturating variants), bitwise, compare,
memory, cast, aggregate access, vector, call, control flow, optional/union
error-union unwrap, atomics, and low-level escape instructions:

```text
const.i const.f copy
add sub mul div rem add_wrap sub_wrap mul_wrap add_sat sub_sat mul_sat
and or xor not shl_checked shr_checked shl_count_wrap shr_count_wrap shl_sat shl_count_wrap_sat
icmp fcmp
load store addr gep
cast bitcast ptrtoint inttoptr
alloca
extract insert
vector_splat vector_extract vector_insert vector_shuffle vector_reduce vector_select
call call_indirect
ret ret_error
br condbr switch phi
optional_pack optional_test optional_unwrap
union_tag union_payload
error_test error_payload
atomic_load atomic_store atomic_rmw cmpxchg fence
asm trap unreachable
shift_combine
```

(`spec/02-compiler/lir.xml`)

Calls carry explicit ABI metadata after lowering — "calling convention,
argument classes, return classes, and error-union status" — rather than
leaving ABI classification implicit in the instruction shape. Loads and
stores carry alignment and volatile flags; atomics carry memory order.

`defer`/`errdefer` are surface-language constructs only: "Defer/errdefer
do not survive into final LIR; Sema/lowering expands cleanup control flow
before backend lowering" (`spec/02-compiler/lir.xml`) — by the time LIR
exists, every deferred cleanup has already been inlined as ordinary
control flow on every exit path.

### Integer arithmetic semantics

`LIR_SEMANTICS.md` is normative together with `lir.xml` and pins down
exactly what each arithmetic family means:

| Family | Meaning |
| --- | --- |
| `add/sub/mul` | checked arithmetic — a statically known overflow is `MLX-E4002`; overflow only detectable at runtime traps in safe mode |
| `add_wrap/sub_wrap/mul_wrap` | modulo 2^N |
| `add_sat/sub_sat/mul_sat` | clamp to the exact `iN`/`uN` numeric bounds |
| `div/rem` | signedness explicit; division by zero traps at runtime in safe modes, and is a compile error when the divisor is a compile-time constant zero |
| `and/or/xor/not` | exact-width bitwise operations |

Every integer instruction carries its exact bit width and signedness —
"No backend-native width silently changes source semantics" — which is
what lets a program declare and rely on unusual widths (the spec commits
to preserving `iN`/`uN` up to 4096 bits through LIR; see below).

### Shifts and shift-combine

Each shift instruction carries four independent axes: direction
(left/right), count policy (checked/wrapping), result policy
(checked/saturating where applicable), and the exact integer width N.
`shl_checked`/`shr_checked` require the count to fall in `0..N-1`, with
`shr_checked` distinguishing logical (unsigned) from arithmetic (signed)
shift; `shl_count_wrap`/`shr_count_wrap` instead reduce the count mod N.

`shift_combine` is a fused instruction for the common "shift, then combine
with the original value" idiom (e.g. rotate-like patterns). It records
`position` (`prefix`/`suffix`), `direction`, `count_policy`,
`shift_result_policy`, a `combiner` (`and|or|xor|add|sub|mul`), and — for
an arithmetic combiner — `combiner_overflow`
(`checked|wrapping|saturating`). Its evaluation order is pinned down
explicitly: operands are evaluated exactly once, the shift runs against
the *original* (pre-shift) left value, and only then are the original and
shifted values combined at the specified position
(`spec/02-compiler/LIR_SEMANTICS.md`).

### Compound assignment lowering

Compound assignment (`x += y` and friends) is specified as a fixed
five-step lowering through an addressable destination temporary:

1. evaluate the destination address once
2. load the old value once
3. evaluate the RHS once
4. execute the operator
5. store once

The point of pinning this down is aliasing/side-effect safety: "No
source-visible getter/index/call used to locate the destination may
execute twice" — e.g. `arr[i()] += f()` must call `i()` exactly once even
though the destination is read and then written.

### Arbitrary-width integers

LIR is specified to preserve `iN`/`uN` widths up to 4096 bits end to end.
Backend lowering picks a representation by size: one native register for
widths ≤64 bits, register pairs or target 128-bit sequences for ≤128
bits, and multi-limb stack/register sequences above that. Whatever the
representation, "every operation masks or sign-normalizes high unused
bits as required by its exact source width," and packed loads/stores must
respect the exact bit range without touching adjacent fields
(`spec/02-compiler/LIR_SEMANTICS.md`).

## x86_64 backend

`spec/02-compiler/x86_64.xml` is a short, five-element normative
description of what the backend must do, not a full ISA reference:

- **Encoder** — "Direct machine-code byte emission. No external assembler
  is required." The compiler is its own assembler.
- **BaselineCPU** — x86_64 with SSE2 is the floor every build must
  support.
- **OptionalFeatures** — `sse3 ssse3 sse4_1 sse4_2 avx avx2 bmi1 bmi2
  popcnt` may be used when enabled/detected.
- **VectorLowering** — use whatever native feature set is enabled;
  otherwise scalarize when legal (i.e. a vector operation on a target
  without the matching ISA extension decomposes into scalar
  instructions rather than failing to compile, when a legal scalarization
  exists).
- **Relocations** — the backend emits symbolic relocations, consumed by
  the integrated object writer/linker in pipeline stages 11–12, rather
  than resolving all addresses itself.
- **RegisterAllocator** — "Bootstrap may use linear scan. Spill slots are
  stack allocated and aligned to value requirements." This is explicitly
  scoped as a bootstrap-adequate strategy, not a mandate that every
  conforming compiler use linear scan forever.

Because relocations and the object/link stages are backend-owned but
consumed downstream, `MLX-E9002 RelocationOverflow` is tagged with phase
`linker` even though the relocation itself originates in the x86_64
encoder — see the phase table below.

## Diagnostic model

### Required shape of a diagnostic

`spec/02-compiler/diagnostics.xml`'s `Model` fixes the fields every
diagnostic must carry: `code, phase, severity, primarySpan, message,
cause` are required; `notes, relatedSpans, fixIts, instantiationTrace,
comptimeTrace` are optional. A source span is "internally represented as
file identity plus UTF-8 byte start/end offsets," with human line/column
positions derived only for presentation — offsets, not line/column, are
the source of truth internally.

`NoStringOnlyErrors` makes this a hard requirement rather than a style
preference: "A compiler phase must not communicate a user-facing failure
solely as an unstructured string internally." Every user-facing failure
must be a structured diagnostic with a stable code, not an ad hoc string
thrown up from deep in the pipeline.

### Phases, families, severities

The nine (ten, counting the `resolve`/`comptime` split noted above)
`Phases` and the nine `MLX-E` code-range `Families` are two different
groupings of the same diagnostic space — phase is a compiler-pipeline
concept (which stage raised this), family is a code-numbering convention
(which thousand-block a stable code lives in):

| Family range | Topic |
| --- | --- |
| `MLX-E1000..1999` | source and lexer |
| `MLX-E2000..2999` | parser and grammar |
| `MLX-E3000..3999` | name/module resolution |
| `MLX-E4000..4999` | types, semantic analysis, control flow |
| `MLX-E5000..5999` | comptime and reflection |
| `MLX-E6000..6999` | copyability, `@move`, initialization state |
| `MLX-E7000..7999` | pointer, memory, safety, atomics |
| `MLX-E8000..8999` | ABI, extern, target legality |
| `MLX-E9000..9999` | lowering, codegen, object writer, linker |

(`spec/02-compiler/diagnostics.xml`)

`Severity` is one of `error, warning, note, help`. `StableCodes` commits
to code stability across the Mlx 1.x line: "Human prose may improve
without changing the code when the semantic condition is unchanged" — so
tooling can match on `MLX-E4001` indefinitely even as the message text
around it is reworded. `Warnings` (`MLX-Wxxxx`) follow the same stability
rule, and `-Werror` "promotes warnings to errors without changing their
diagnostic identity" — the code stays a `W` code even once it fails the
build.

See [`docs/guide/11-diagnostics.md`](../guide/11-diagnostics.md) for the
full `codes.xml` catalog cross-referenced to `tests/*.mlx` fixtures; this
page does not repeat that table.

### Determinism

The `Determinism` rule is exact enough to test against: "For identical
source, target, language version and compiler version, diagnostics are
emitted deterministically by canonical file identity, primary byte
offset, phase order, then diagnostic code. Parallel compilation must not
randomize ordering." That is a fully specified four-key sort — a
compiler that parallelizes sema across files still owes the caller output
sorted as if it hadn't.

### Recovery

Diagnostic recovery is scoped per phase:

- **Parser** — "After a recoverable parser error, synchronize at
  `STATEMENT_END`, closing brace, or a valid top-level declaration start
  while preserving nesting depth." This is what lets the parser keep
  producing diagnostics for the rest of a file instead of stopping at the
  first syntax error.
- **Sema** — "Independent declarations should continue semantic analysis
  after recoverable errors," so one broken function doesn't suppress
  diagnostics in unrelated ones.
- **Limit** — a default cap of 100 primary errors, after which the
  compiler "emit[s] a final `TooManyDiagnostics` error and stop[s]
  further error discovery" — a circuit breaker against runaway error
  cascades.

### Rendering and machine-readable output

Human-facing rendering has a required minimum (error code, concise
message, source path, line/column, highlighted primary span) and a
recommended-but-optional layer on top (related source spans, notes
explaining the type/range/ownership reason, actionable help when
unambiguous).

Machine consumers use `mlx check --diagnostic-format=json`, specified by
`spec/02-compiler/diagnostics/json-format.xml`. Its top-level required
fields are `severity, code, phase, message, primary`, where `primary` (and
any `related` entry) carries span fields `file, byte_start, byte_end,
line_start, column_start, line_end, column_end`. Optional top-level fields
mirror the human model: `cause, notes, related, fixes,
instantiation_trace, comptime_trace`. The spec gives a worked example:

```json
{
  "severity": "error",
  "code": "MLX-E6001",
  "phase": "sema",
  "message": "use of moved value `file`",
  "primary": {
    "file": "src/main.mlx",
    "byte_start": 82,
    "byte_end": 86,
    "line_start": 5,
    "column_start": 1,
    "line_end": 5,
    "column_end": 5
  },
  "related": [
    { "message": "value moved here", "file": "src/main.mlx", "byte_start": 41, "byte_end": 52 }
  ]
}
```

(`spec/02-compiler/diagnostics/json-format.xml`)

`mlx explain CODE` is the long-form counterpart: "The compiler
distribution provides long-form explanations for stable core
diagnostics" (`spec/02-compiler/diagnostics.xml`).

### `@compileError` and failure classes

`@compileError("message")` has a dedicated normative rule pinning down
both its code and its default span: it "produces `MLX-E5003` and uses the
builtin call span as primary location unless an explicit library
diagnostic object supplies a more relevant user source span"
(`spec/02-compiler/diagnostics.xml`). See
[`docs/guide/08-comptime-and-generics.md`](../guide/08-comptime-and-generics.md)
for a worked example against `tests/13_compile_error_builtin.mlx`.

`spec/02-compiler/diagnostics/error-classes.xml` separates four kinds of
program failure that are easy to conflate:

| Class | Meaning |
| --- | --- |
| `CompileError` | the program is not valid for the selected language version/target; no executable is produced for the affected root artifact |
| `RuntimeSafetyTrap` | the program is valid, but a checked runtime condition (bounds, overflow, invalid optional unwrap, invalid tag) fails — a defined trap/panic path in safety-enabled modes |
| `Panic` | an explicit unrecoverable runtime failure; not exception unwinding, not a cleanup mechanism |
| `UndefinedBehavior` | behavior the language imposes no requirements on, generally reachable only through lifetime violations, data races, invalid raw memory operations, or unsafe contracts |

The `MLX-E####`/`MLX-W####` diagnostic codes covered above are all
`CompileError`s (or their warning-severity siblings); `RuntimeSafetyTrap`
and `Panic` are the two *defined* runtime failure modes (see the
`RuntimeSafety` note in the pipeline section above — this is what those
traps are), and `UndefinedBehavior` is the one category with no
diagnostic and no defined trap at all, reachable only by stepping outside
an `unsafe` contract.
