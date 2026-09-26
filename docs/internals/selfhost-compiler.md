# The self-hosted compiler

**Source:** `compiler/selfhost/README.md` (authoritative architecture
summary, treated as ground truth here), `compiler/selfhost/main.mlx`,
`compiler/selfhost/driver/*.mlx`, `compiler/selfhost/lexer.mlx`,
`compiler/selfhost/parser.mlx`, `compiler/selfhost/ast/*.mlx`,
`compiler/selfhost/sema.mlx`, `compiler/selfhost/sema/**/*.mlx`,
`compiler/selfhost/ir/*.mlx`, `compiler/selfhost/backend/x86_64/**/*.mlx`,
`compiler/selfhost/object/elf64.mlx`, `compiler/selfhost/diagnostic.mlx`

**Source tests:** `tests/140_selfhost_source_runtime.mlx` through
`tests/233_selfhost_comptime_diagnostic_runtime.mlx` (the `*_selfhost_*_runtime.mlx`
family; see [Runtime test fixtures](#runtime-test-fixtures) below)

This page tours the actual architecture of `compiler/selfhost/` — the
canonical Mlx compiler, written entirely in Mlx, that the Zig bootstrap
compiler (`mlx0`) compiles into `mlx1`. It documents an implementation, not
the specification: `compiler/selfhost/` is Mlx's own compiler describing
itself, and where its actual behavior is narrower than what
`spec/02-compiler/` describes normatively (see
[docs/internals/compiler-pipeline.md](compiler-pipeline.md)), this page says
so explicitly rather than asserting spec-perfect conformance. See
[Where the self-hosted implementation is narrower than the spec](#where-the-self-hosted-implementation-is-narrower-than-the-spec)
below for a concrete example found by reading the source.

## The self-hosting story

`mlx0`, the Zig-implemented bootstrap compiler, compiles
`compiler/selfhost/main.mlx` (and everything it transitively imports) into
`mlx1` — a complete Mlx binary that is no longer Zig at all. Because `mlx1`
is a full Mlx compiler, it can compile the very same `compiler/selfhost/`
sources into `mlx2`; `mlx2` can compile them into `mlx3`; and so on. The
self-hosting claim is that this process reaches a fixed point: from `mlx2`
onward, every generation of the compiler should produce a byte-identical
next generation, because nothing about the compiler's own behavior is
changing between runs. The README gives the exact verification recipe:

```sh
zig-out/bin/mlx1 compiler/selfhost/main.mlx -o /tmp/mlx2
/tmp/mlx2 compiler/selfhost/main.mlx -o /tmp/mlx3
/tmp/mlx3 compiler/selfhost/main.mlx -o /tmp/mlx4
cmp /tmp/mlx3 /tmp/mlx4
```

(`compiler/selfhost/README.md`)

Note what is and isn't asserted: `mlx2` need not be identical to `mlx1`
(mlx1 is a Zig-vs-Mlx cross-compilation, so its code generation choices can
differ from what mlx1-compiling-itself produces), but from `mlx2` onward the
output is expected to stabilize — `mlx3` and `mlx4` (both produced by a full
Mlx-compiling-Mlx pass) should be byte-for-byte equal. `cmp` exiting `0` is
the pass/fail signal.

`compiler/selfhost/main.mlx` also runs a tiny internal `selfCheck()` before
doing any real work — a sanity check that the lexer and diagnostic modules
it was just compiled with still behave correctly:

```mlx
fn selfCheck() -> u8 {
    const source = "const answer = 42\n"
    if lexer.count(source) != 5 { return 1 }
    const first = lexer.next(source, 0)
    if first.kind != token.Kind.keyword_const || first.start != 0 || first.length != 5 { return 2 }
    if diagnostic.none().code != diagnostic.Code.none { return 3 }
    return 0
}

pub fn main(arguments: [][*]const u8) -> u8 {
    const check = selfCheck()
    if check != 0 { return check }
    return pipeline.Pipeline.compile(options.Options.parse(arguments))
}
```

(`compiler/selfhost/main.mlx`)

Every generation of the compiler is a normal Linux executable built with
`zig build mlx1` and run directly: `zig-out/bin/mlx1 tests/01_basic.mlx`.

## Module layout

Per the README, the executable entry point is deliberately small; the
compiler and its runtime foundation stay independent of libc and Zig. The
top-level `.mlx` files (`lexer.mlx`, `parser.mlx`, `types.mlx`,
`symbols.mlx`, `sema.mlx`, `modules.mlx`, `comptime.mlx`, `builtins.mlx`)
are public facades over a same-named subdirectory that holds the real
implementation, split by responsibility:

| Facade | Subdirectory owns |
| --- | --- |
| `token.mlx` | the stable token representation |
| `lexer.mlx` | `lexer/` — full normative token set, longest-match operator algebra, nested comments, literals, newline filtering, growing token storage |
| — | `ast/` — the compact indexed syntax-node store |
| `parser.mlx` | `parser/` — declarations, types, Pratt expressions, statements, control flow, bounded error recovery |
| `types.mlx` | `sema/types/` — canonical type interning, layouts, coercion checks, syntax-type resolution (struct/enum/union/tuple layouts retain field metadata for reflection builtins) |
| `symbols.mlx` | `sema/symbols/` — declaration symbols, nested lexical scopes, duplicate detection, move states |
| `sema.mlx` | `sema/declarations.mlx` (top-level binding/signatures) and `sema/functions.mlx` + `sema/control_flow.mlx` (parameter/local binding, call/return validation, `if`/`while`/`for`/`break`/`continue` analysis) |
| `builtins.mlx` | `sema/builtins/` — the complete normative `@` builtin namespace, arity/result typing |
| `comptime.mlx` | `sema/comptime/` — bounded CTFE for integer/boolean constants, branches, layout predicates |
| `modules.mlx` | `modules/` — import classification/normalization, cycle-safe recursive source-graph loading, per-file scopes, public module namespace binding |
| — | `source.mlx` — source-file loading through an explicit allocator |
| — | `diagnostic.mlx` — stable codes, phases, severity, source spans, causes, messages, terminal rendering |
| — | `driver/` — CLI options, progress/trace reporting, frontend analysis, lowering, backend orchestration, cleanup (`main.mlx` is the thin entry point) |
| — | `ir/inline.mlx`, `ir/optimize.mlx` — the inliner and mandatory LIR canonicalization/optimization passes |
| — | `backend/x86_64/codegen/` — mutable backend state, label/memory/arithmetic/value/call/control-flow instruction emission, faced by `backend/x86_64/codegen.mlx` |

(`compiler/selfhost/README.md`)

`ir/inline.mlx` recognizes safe direct-return inline candidates: explicit
`inline` is honored when eligible, `noinline` vetoes automatic expansion,
and `-O2`/`-O3` progressively widen a cost-limited automatic-inlining
budget, with bounded recursive expansion falling back to an ordinary call
when the bound is hit.

## Driver and orchestration (`driver/`)

`main.mlx` delegates everything to `driver/pipeline.mlx`'s `Pipeline.compile`,
which runs inside one `unsafe` block and drives the whole compilation
through an arena allocator (`arena_allocator.init(pages, 268435456)` — a
256&nbsp;MiB arena) that is freed in one shot at the end:

```mlx
pub const Pipeline = struct {
    pub fn compile(options: options_module.Options) -> u8 {
        unsafe {
            var reporter = reporter_module.Reporter.init(...)
            ...
            const loader = modules.initLoader(allocator, @ptrCast([*]const u8, "std/src"), 7, null)
            const root = modules.loadRoot(loader, options.inputPointer, options.inputLength)
            if root == modules.NONE || modules.totalErrors(loader) != 0 { ... return 4 }
            ...
            var status = Self.analyze(loader, &type_store, &symbol_store, &reporter)
            if status == 0 && options.hasOutput {
                status = Self.emitExecutable(loader, allocator, &type_store, &symbol_store, options, &reporter)
            }
            ...
            return status
        }
    }
```

(`compiler/selfhost/driver/pipeline.mlx`)

`compile` walks through numbered progress steps that map directly onto the
normative pipeline stages: (1) load and parse module graph, (2) resolve
modules and declarations, (3) analyze declaration types, (4) analyze
function bodies, and — only when an output path was given — (5) lower typed
AST to LIR, (6) optimize LIR, (7) generate x86_64 machine code, (8) resolve
backend symbols, (9) write the ELF64 executable. Each failure path returns a
distinct process exit status (4 through 10) and reports a structured
diagnostic before returning — e.g. a module-graph failure reports
`unexpected_byte`/`Phase.lexer` or `unexpected_token`/`Phase.parser`
depending on `modules.firstSyntaxIsLexer`, and an unsupported LIR-to-machine
lowering reports `unsupported_instruction_lowering`/`Phase.codegen`
(`MLX-E9001` in spec terms).

`driver/options.mlx`'s `Options.parse` hand-rolls argv parsing (no getopt):
`-o <path>`, `--quiet`/`--progress`, `--trace`/`--verbose`, `-O0`..`-O3`, and
`--safety=on`/`--safety=off`, defaulting to `-O2` and `runtimeSafety = true`
— matching the spec's stated default (`spec/02-compiler/pipeline.xml`'s
`RuntimeSafety default="on"`). Anything unrecognized is treated as the input
path.

`driver/reporter.mlx`'s `Reporter` times every step with
`time.monotonicNanoseconds()`, prints `[name N/total] message` progress
lines to stderr (suppressible with `--quiet`), and separately exposes
`metric`/`metricDuration` (always shown unless quiet) and
`traceValue`/`traceString`/`traceModules` (shown only with `--trace`). The
pipeline calls back into it heavily to report internal counters — module
count, token/AST-node counts, type-intern probe counts, LIR instruction
counts, optimization pass deltas (constants folded, branches simplified,
loads forwarded, locals promoted, dead code eliminated), and backend metrics
(machine-code bytes, backend symbols/fixups, aggregate allocation
sites/bytes) — giving a compile run a fairly detailed self-report even
without `--trace`.

## Lexer (`lexer.mlx`, `lexer/token.mlx`, `lexer/scanner.mlx`)

`lexer/token.mlx` defines `Kind` as a large `u16` enum covering every
keyword, literal kind, and operator (including compound forms like
`shift_left_percent_pipe_equal` for `<<%|=` and `ampersand_shift_left_equal`
for `&<<=`) plus `statement_end`. The public facade `lexer.mlx` re-exports
`scanner.Kind`/`Token`/`Scanner` and offers both a single-token API
(`next(source, start)`) and a whole-buffer API (`scanAll`, growable via
`lexer/buffer.mlx`'s `Buffer`).

Newline-to-`statement_end` filtering — the pipeline's `NewlineFilter` stage
— is implemented directly in the scanner rather than as a separate pass. On
a `\n` byte, `Scanner.nextInto` asks `shouldEmitStatementEnd`, which only
allows a `statement_end` when nesting depth is zero and the previous token
both `canTerminate` a statement and does not `requiresContinuation`
(`lexer/token.mlx`'s two predicate functions, table-driven by `Kind` ranges):

```mlx
fn shouldEmitStatementEnd(scanner: *Self) -> bool {
    if scanner.*.parenthesisDepth > 0 || scanner.*.bracketDepth > 0 || !scanner.*.hasLastKind { return false }
    if token.requiresContinuation(scanner.*.lastKind) { return false }
    return token.canTerminate(scanner.*.lastKind)
}
```

(`compiler/selfhost/lexer/scanner.mlx`)

Comments are skipped in `nextInto` directly: `//` to end of line, and `/*
*/` nested block comments via `skipBlockComment`, which tracks a `depth`
counter so `/* outer /* nested */ end */` correctly closes only at the
outermost `*/`. Parenthesis/bracket depth tracking (used by the
statement-end filter above) is also updated inline in `finishInto` whenever
a `(`/`)`/`[`/`]` token is finished, rather than by the parser.

## Parser (`parser.mlx`, `parser/expression.mlx`)

The parser is a hand-written recursive-descent + Pratt parser writing
directly into the flat `ast/store.mlx` `Store` (see below) rather than
building a boxed tree. `parser/expression.mlx` shows the pattern used
throughout: small `parseX` functions that call `state_module.Parser.add`
to append a `Node` and return its index. For example, `parseList` (used for
call arguments, tuple/array literals, etc.) walks a comma-separated list up
to a closing delimiter, threading a linked list of children through each
node's `next` field and recovering from a stalled parse by calling
`Parser.recoverStatement` — the spec's `STATEMENT_END`/closing-brace
synchronization is implemented as this explicit stall check (`if
parser.*.index == start_index { recordError; advance }`) rather than a
generic exception-based recovery mechanism.

`parseBuiltin` shows how `@name(args)` builtin calls are represented: a
`Tag.builtin_call` node whose `mainToken` is the builtin's identifier token
and whose `extra` field stores the `@` token index (used later, per the
diagnostics spec, as the default primary span for e.g. `@compileError`).

## AST (`ast/node.mlx`, `ast/store.mlx`)

The AST is not a tree of heap-allocated nodes; it is an indexed store. Every
`Node` is a fixed-size record:

```mlx
pub const Node = struct {
    tag: Tag
    mainToken: usize
    endToken: usize
    lhs: usize
    rhs: usize
    next: usize
    extra: usize
}
```

(`compiler/selfhost/ast/node.mlx`)

`lhs`/`rhs`/`extra` are generic slots whose meaning depends on `tag` — the
same three fields mean "condition/then/else" for an `if_statement`,
"callee/first-argument/argument-count" for a `call_expression`, and so on,
decided entirely by convention documented in a comment: "Relationships are
indices into Store; linked child lists use `next`. The semantic phase
interprets lhs/rhs/extra by tag." `Tag` is a flat `u16` enum of ~60
variants covering every declaration, statement, expression, and type form
(`function_declaration`, `match_statement`, `builtin_call`,
`error_union_type`, `nocopy_type`, ...). `NONE` is `usize`'s max value
(`18446744073709551615`), used as a universal null/absent-child sentinel
throughout the whole compiler (AST indices, type IDs, symbol table slots
all reuse this convention).

`ast/store.mlx`'s `Store` is a manually managed growable array of `Node`,
doubling capacity on overflow (`ensureCapacity`) and reading/writing nodes
by raw pointer arithmetic (`pointerAt`) rather than through a slice-indexed
Mlx array — consistent with the compiler being implemented without relying
on a fuller standard library than the bootstrap std provides. `link(store,
left, right)` is how the parser builds the `next` linked lists mentioned
above: it loads node `left`, sets its `next` to `right`, and stores it back.

## Sema (`sema.mlx`, `sema/types/`, `sema/symbols/`, `sema/declarations.mlx`)

`sema.mlx` itself is a two-function facade over the two-pass structure
described in the README and mirrored by the driver's step numbering:

```mlx
pub fn analyzeDeclarations(source: []const u8, tokens: Buffer, nodes: *NodeStore, root_index: usize, types: *TypeStore, symbols: *SymbolStore, scope: *Scope) -> usize {
    return declarations.root(source, tokens, nodes, root_index, types, symbols, scope)
}

pub fn analyzeFunctionBodies(source: []const u8, tokens: Buffer, nodes: *NodeStore, root_index: usize, types: *TypeStore, symbols: *SymbolStore, scope: *Scope) -> usize {
    return functions.root(source, tokens, nodes, root_index, types, symbols, scope) 
}
```

(`compiler/selfhost/sema.mlx`)

`sema/types/type.mlx`'s `Type` record mirrors the AST's indexed-store
design: a fixed-size record (`kind`, `flags`, `bits`, `lhs`/`rhs`/`backing`
IDs, an `extraStart`/`extraLength` range into a separate field-metadata
vector for aggregates, and a `copyability` tag) interned in a `TypeStore`
rather than allocated per-instance. `same(left, right)` is a flat
field-by-field equality check used by the interner to deduplicate
structurally identical types. Small helpers like `isInteger`, `isFloat`,
`isPointer`, `isComptime`, `isCopyable` centralize the kind-classification
logic the rest of sema depends on instead of re-testing `kind` inline
everywhere.

`sema/symbols/symbol.mlx`'s `Symbol` carries a `State` enum
(`declared`/`initializing`/`initialized`/`moved`/`partially_moved`) that is
the concrete representation of the move/ownership tracking described
normatively — `markMoved` transitions `initialized -> moved` and records
the byte span of the move site (`movedAtByte`/`movedAtLength`) specifically
so a later use-after-move diagnostic can point back at "value moved here"
as a related span, matching the JSON diagnostic example in
`spec/02-compiler/diagnostics/json-format.xml`. A bitset of `FLAG_*`
constants (`FLAG_CONSTANT`, `FLAG_FUNCTION`, `FLAG_PUBLIC`, `FLAG_EXPORT`,
`FLAG_EXTERN`, `FLAG_THREADLOCAL`, `FLAG_PARAMETER`, `FLAG_INLINE`,
`FLAG_NOINLINE`) covers declaration modifiers, and `GENERIC_RETURN_*`
constants distinguish how a generic function's return type depends on its
parameters (none / a parameter's own type / a `comptime T: type` parameter
/ a field of one). `ownerTypeId` attaches a method's `Symbol` to the
aggregate type that declares it — the comment notes this is "keeping
ownership on the declaration symbol" rather than pretending methods are
runtime struct fields, i.e. methods are resolved statically through the
symbol table, not stored as function-pointer fields.

`sema/declarations.mlx` resolves top-level bindings and function signatures
before any function body is analyzed (so mutually recursive top-level
functions/types can reference each other). It works directly against the
token buffer and AST store, e.g. `tokenPointer` recovers the raw source
bytes behind a token for identifier comparisons, and reports diagnostics
through the same `diagnostic.create`/`diagnostic.render` pair used
everywhere else in the compiler — there is no separate error-collection
object; a sema function renders its own diagnostic immediately and returns
an error count.

## Comptime evaluation (`sema/comptime/evaluate.mlx`)

Comptime evaluation is a small recursive tree-walking evaluator over a
`Value` type (`sema/comptime/value.mlx`, holding an integer/boolean/type
tag) rather than a bytecode VM. Crucially, it is bounded by an explicit
branch quota threaded through a `State`:

```mlx
pub const State = struct {
    branches: usize
    quota: usize
}
```

with the quota check applied wherever branch-like evaluation happens:

```mlx
if state.*.branches >= state.*.quota { return value_module.quotaExceeded() }
state.*.branches += 1
```

(`compiler/selfhost/sema/comptime/evaluate.mlx`)

This is the concrete mechanism behind `MLX-E5002 ComptimeBranchQuotaExceeded`
— reaching the quota produces a distinct `Reason.quota_exceeded` result,
kept separate from `Reason.unsupported` (a construct the evaluator simply
doesn't know how to fold) so the caller can report the right diagnostic
code for each. `tests/233_selfhost_comptime_diagnostic_runtime.mlx` exercises
exactly this distinction end to end: evaluating `1 + 2 + 3` against a quota
of `1` yields `quota_exceeded`, while evaluating a string literal (which the
evaluator has no integer/boolean folding rule for) against a quota of
`1000000` yields `unsupported` — proving the two failure reasons are
genuinely different code paths, not the same error under two names.

The evaluator also implements the reflection builtins listed in
[docs/guide/08-comptime-and-generics.md](../guide/08-comptime-and-generics.md)
directly against the live `TypeStore` — `evaluateBuiltin` matches on
`builtins.Kind` and calls straight into `types_module.sizeOf`/`alignOf`/
`bitSizeOf`/`fieldAt` etc. to produce the constant. `@languageVersion` is
hard-coded to return the integer `10000` here (a concrete Stage-1 answer to
a builtin the language spec leaves the result format for — see
`SPEC_CONFLICTS.md`'s "Language-version value" entry — though Stage 0's
`MLX-E5005` rejection described in the guide is what a *bootstrap-conforming*
answer looks like; this self-hosted evaluator instead commits to a specific
value).

## LIR (`ir/lir.mlx`, `ir/lower.mlx`, `ir/optimize.mlx`)

`ir/lir.mlx`'s `Opcode` enum is the self-hosted compiler's concrete
instruction set. It maps onto the normative instruction list in
`spec/02-compiler/lir.xml` but is not a 1:1 transcription — it adds
backend-shaped instructions the spec doesn't name directly (`func_sym`,
`label`, `icmp_br` as a fused compare-and-branch, `direct_call` vs `call`,
`syscall`, `byte_mask_64`, `dead` as an explicit tombstone opcode for
eliminated instructions, `aggregate_copy`, `mem_copy` for struct and array
value copies (one instruction per copy, whatever the size; `lower.mlx`'s
`copyAggregate` puts a `const` local's copy in the frame and a `var`'s in
the arena, and `returnedValue` copies frame storage out before a `return`
hands it back), and separate `udiv`/`urem` next
to signed `div`/`rem`) and folds some of the spec's saturating/wrapping
arithmetic variants down to flag bits on a smaller opcode set rather than
one opcode per variant. Every `Inst` is a fixed-size record — `opcode`,
`flags`, `typeId`, and three generic operand slots `arg0`/`arg1`/`arg2` —
stored in a flat growable vector exactly like AST nodes and types are, with
`NONE` again used as the absent-operand sentinel.

`ir/lower.mlx` is the AST/sema-to-LIR translator. Its `LirBuilder` carries
substantial per-function lowering state, including a `Binding` record per
local (source name, one-to-three backing addresses for multi-part values,
its type, and a `dropFlagAddress` for conditional-drop tracking), a
`Cleanup` stack distinguishing `explicit` (`defer`/`errdefer`) from
`auto_drop` cleanups, and `LoopTarget` records that pair a loop's break/
continue blocks with its optional label — this is the concrete lowering
machinery behind the spec's "Defer/errdefer do not survive into final LIR;
Sema/lowering expands cleanup control flow before backend lowering"
(`spec/02-compiler/lir.xml`). Generic function instantiation is memoized
through a `GenericInstance` table keyed by a `GenericArgument` list,
matching the monomorphization dedup rule in `spec/02-compiler/sema.xml`.

`ir/optimize.mlx`'s `optimize.run` is the mandatory-canonicalization pass
the README describes ("mandatory LIR canonicalization, constant and branch
folding, local load forwarding, stack-slot promotion and dead-code
elimination"). It runs a fixed sequence — function-level dead-code
elimination (`function_dce.mlx`), constant folding, local-load forwarding,
copy propagation, a second constant-folding pass (to catch constants
exposed by the first), post-terminator dead-code elimination
(`control_flow_dce.mlx`), comparison-branch fusion, stack-slot promotion,
and generic dead-instruction elimination — and returns a `Result` record
whose fields are exactly the counters the driver reports as metrics
(`constantsFolded`, `branchesSimplified`, `comparisonBranchesFused`,
`loadsForwarded`, `copiesPropagated`, `localsPromoted`,
`storesEliminated`, `functionsEliminated`, ...). `optimize.run` is called
unconditionally from the pipeline regardless of `-O` level — the
optimization-level distinction in this compiler lives in the inliner
(`ir/inline.mlx`), not in whether canonicalization runs at all, matching
the README's note that "the current bootstrap distinction is intentionally
narrow."

`tests/179_selfhost_lir_optimizer_runtime.mlx` demonstrates the whole
canonicalization pipeline on a hand-built six-instruction LIR sequence: an
`or` of two constants folds to a single `const_i 255`, a condition-true
`condbr` simplifies to an unconditional `br`, and the now-unreachable/unused
instructions are marked `dead` and excluded from the `instructionsAfter`
count.

**Known gap: a module-level `const` of non-integer/non-boolean type
miscompiles into a bogus "unresolved backend symbol" instead of a proper
diagnostic.** `sema/comptime/value.mlx`'s `Kind` enum only has
`integer`/`boolean`/`type_value`/`null_value` variants — there is no
representation for a comptime string, slice, or array. `ir/lower.mlx`'s
`lowerSymbolValue` (the function that decides how to lower a reference to a
resolved symbol) handles the `integer`/`boolean` comptime tags and function
symbols (`FLAG_FUNCTION`) explicitly, then falls through to `return
lir.NONE` for anything else — which is exactly what happens for, say, a
top-level `const ANDROID_NS_URI: []const u8 = "..."` or a top-level `const
IDS: [9]u32 = [9]u32{...}`. The caller (the general identifier-lowering
path around line 2489) only uses that `lowerSymbolValue` result if it isn't
`NONE`; when it is, the code falls all the way through to a "might be a
forward-reference function; add as symbol" fallback that unconditionally
treats the identifier as an as-yet-undefined function, emitting a
`lir.addModuleSymbol` reference (named `.m<module>.<NAME>`) and a
`func_sym` LIR instruction for it. No function body with that name is ever
lowered to satisfy the reference, so it reaches the backend as a fixup with
no matching label, and `encoder.applyFixups` fails at the "resolve backend
symbols" pipeline step with `MLX-E3004: unresolved backend symbol` — a
diagnostic that (per `driver/pipeline.mlx:206`) carries no source location,
so the actual cause (a harmless-looking top-level string or array constant
declared possibly hundreds of lines from where it's used) is easy to miss.
`driver/reporter.mlx:171`'s `Reporter.unresolvedSymbol` does print the
bogus symbol name (e.g. `.m0.SHORT_STR`) to stderr, but only when `mlx1` is
invoked with `--trace`/`--verbose` — without that flag the error is
reported with zero identifying information at all. Reproduced with a
minimal two-line repro (`const S: []const u8 = "hi"` at module scope,
referenced as `S.length` inside `main`) that fails the same way regardless
of string length or of whether the const is referenced once or multiple
times; a *local* (function-scope) `const` of the same type, or the string
literal used directly, both lower correctly, which is what makes this a
gap in top-level/module-scope constant handling specifically rather than in
slice/string lowering in general. Found and worked around (not fixed) while
porting `experiments/android-aarch64-bluescreen/tool/axml.zig` to
`experiments/android-aarch64-bluescreen/mlx/axml.mlx`: every top-level
`const` of a type other than a plain integer/boolean was rewritten as a
zero-argument function returning the value (`androidNsUri()`) or an
indexing function (`androidAttrId(i)`) instead — see the comments at
`experiments/android-aarch64-bluescreen/mlx/axml.mlx`'s `androidNsUri` and
`androidAttrId`. No regression test or fix exists for this in the compiler
itself yet; a real fix would need `sema/comptime/value.mlx`'s `Kind` to gain
a representation for these types (or, short of that, at minimum
`lowerSymbolValue` should report a proper location-carrying diagnostic
instead of silently falling through to the forward-function-reference path
when a resolved, non-function symbol can't be lowered).

**Fixed: `undefined` initializing a memory-typed field inside an aggregate
literal dereferenced address 0 and crashed.** `lowerNode`'s
`undefined_literal` case (line ~1192) lowers `undefined` to a single scalar
`lir.constI(lir.NONE, 0)` for every type uniformly — a reasonable
placeholder for a scalar field, but wrong for a `struct`/`union`/`array`
-typed one. `lowerAggregateLiteral`'s per-field loop always lowered the
initializer and called `storeAggregateValue`, which for a memory type
(`aggregate_copy.isMemoryType`) calls `aggregate_copy.emit(ir, typeStore,
address, value, type_id)` — and `aggregate_copy.emit` treats its `value`
argument as a *source address* to `load` from at each word offset
(`copyWidth`/`addressAt` in `ir/lowering/aggregate_copy.mlx`), not as an
already-materialized value. So `Type.{ .arrayField = undefined, ... }`
computed a GEP off address `0` and unconditionally dereferenced it — a
near-null-pointer read — crashing with `SIGSEGV` on every such literal,
independent of the struct's total size (this is a different, more precise
root cause than an earlier size-threshold guess made while porting the
Android experiment — see `experiments/android-aarch64-bluescreen/mlx/`'s
`StringPool`, whose out-parameter-constructor workaround predates this fix
and is left in place as a harmless, still-valid pattern rather than
unwound). Confirmed independent of return-by-value: a bare local `const b:
T = T.{ .words = undefined, .count = 7 }` crashed identically to returning
the same literal from a function, and a *scalar* field given `undefined` in
the same literal was unaffected either way. Notably, a whole-variable `var
x: T = undefined` was already correct — `lowerBindingInitializer` (line
~3542) special-cases exactly this situation by `alloca`-ing fresh,
intentionally-uninitialized storage instead of attempting a copy — so the
bug was specifically in the per-field path inside an aggregate literal,
which had no equivalent special case. Fixed by adding one: in
`lowerAggregateLiteral`, a field is now skipped entirely (no address
computed, no store emitted, matching `lowerBindingInitializer`'s "leave the
allocated memory as-is" semantics) exactly when its initializer AST node is
`ast.Tag.undefined_literal` *and* its type is a memory type; every other
field — including a scalar field set to `undefined`, and any field given a
real value — is unaffected and still lowers exactly as before.
`tests/236_undefined_memory_field_aggregate_literal_runtime.mlx` is the
regression test: it crashes the pre-fix compiler and exits `0` afterward,
covering both an array field and a nested-struct field, both as a local and
as a function return value, with a sibling field that gets a real value in
the same literal to prove the fix doesn't over-skip.

## x86_64 backend (`backend/x86_64/codegen.mlx`, `codegen/emitter.mlx`)

`backend/x86_64/codegen.mlx`'s `Codegen` is a thin public facade (matching
the README's description) around a `state_module.State` that
`backend/x86_64/codegen/` splits by concern: `state.mlx` (mutable backend
state), `labels.mlx`, `memory.mlx`, `arithmetic.mlx`, `values.mlx`,
`calls.mlx`, `control.mlx` (instruction-family emitters), all coordinated by
`emitter.mlx`'s `Emitter.generateBinary`:

```mlx
pub fn generateBinary(state: *state_module.State) -> void {
    Self.emitStartStub(state)
    var index: usize = 0
    const count = lir_module.instCount(state.*.lir)
    while index < count {
        Self.emitInstruction(state, index, lir_module.getInst(state.*.lir, index))
        index += 1
    }
    Self.emitStringLiterals(state)
}

fn emitInstruction(state: *state_module.State, index: usize, instruction: lir_module.Inst) -> void {
    if labels.Labels.emit(state, index, instruction) { return }
    if memory.Memory.emit(state, index, instruction) { return }
    if arithmetic.Arithmetic.emit(state, index, instruction) { return }
    if values.Values.emit(state, index, instruction) { return }
    if calls.Calls.emit(state, index, instruction) { return }
    const handled = control.Control.emit(state, index, instruction)
}
```

(`compiler/selfhost/backend/x86_64/codegen/emitter.mlx`)

Each family module gets first refusal on an instruction (a chain-of-
responsibility dispatch by trying each emitter in turn and returning as
soon as one claims the instruction), rather than a single giant switch —
directly matching the README's "separates mutable backend state from
label, memory, arithmetic, value, call and control-flow instruction
emission."

The aarch64 backend (`backend/aarch64/`) also keeps every value in a stack
slot below the frame pointer. Slots within 256 bytes use `ldur`/`stur`; the
rest are addressed from `sp` (`sp = fp - frameSize` after the prologue, lower
by the outgoing stack arguments while a call reserves them, which
`encoder.adjustSpBelow` tracks) with one `ldr`/`str`, and only frames beyond
that reach fall back to `sub x17, x29, #slot` first. A load of the slot just
stored, into the register it was stored from, is left out unless a symbol
(a branch target) was defined between them. Together with `mem_copy` this
made Android libraries about 40% smaller.

Every LIR value (`vreg`) lives at a fixed stack offset assigned once by
`State.allocateOp`, keyed by a per-function `slotEpoch` so vreg-index slots
can be reused across functions without clearing the whole table between
them (bumped once per function in `State.beginFunction`, not per basic
block). A single-word value that is used by exactly the LIR instruction
right after it skips that stack write as a peephole
(`State.storeRaxResult`'s `canRetainNext`/`state.pendingRaxVreg` path):
the value stays in `rax` only, on the assumption that its one consumer is
about to read it immediately. `State.getComponent` is a plain "has this
vreg already been written to memory?" peek that knows nothing about that
pending state; most component-0 readers instead go through `State.loadOp`,
which does. Two call sites didn't: `emitTupleElement` (`@field(value, N)`
codegen, `codegen/values.mlx`) and the call-argument loaders
(`codegen/calls.mlx`). Whenever the retained value's real consumer wasn't
literally the next instruction — one arm of a branch reading the field
while the other called anything, or the value being passed to a call and
then read again — those two silently treated the "not yet written" peek as
"the value is zero" (`emitTupleElement` had a literal `movRegImm64 rax, 0`
fallback) instead of resolving the still-pending value. Fixed by routing
component-0 reads through `State.loadOp` in both places;
`tests/235_component_zero_call_and_branch_runtime.mlx` reproduces all three
shapes directly (and asserts the fix), and `tests/170_standard_fmt_any_struct_runtime.mlx` /
`tests/173_standard_fmt_nested_any_runtime.mlx` (see `## fmt` in
`docs/reference/stdlib.md`) are real pre-existing tests that this same bug
crashed (`SIGSEGV`) before the fix, via `std.io.printFmt`'s single-argument
recursive `anytype`-tuple lookup.

**Known gap: `uN`/`iN` wider than 64 bits are not actually multi-word.**
`operators.xml` normatively allows `uN`/`iN` up to 4096 bits (see
`spec/00-language/operators.xml`'s `ArbitraryWidth`), but nothing in the
current pipeline represents or computes on a width above the native 64-bit
register: `ir/lower.mlx`'s `typeWordCount` — the function that decides how
many stack-slot components a value needs — returns `1` for every integer
type, `u128`/`u256`/`u4096` included, only special-casing `slice`,
`error_union`, and `tuple`. `backend/x86_64/codegen/arithmetic.mlx`
matches: every arithmetic opcode (`emitSimple`/`emitNumeric` for
add/sub/mul/bitwise, `emitDivision`, `emitShift`) emits exactly one native
64-bit instruction, with no width-based branching or carry/borrow chain
across additional words at all. In effect a `uN`/`iN` value with N > 64 is
silently truncated to its low 64 bits everywhere at runtime — comptime
constant folding (`sema/comptime/`) is unaffected and does compute the
full-width value, which is why a *literal* wide-integer expression can
look correct while the identical computation over runtime values (e.g.
behind a function call the optimizer can't fold through) silently loses
everything above bit 63. This is a real, reproducible gap (confirmed with
runtime — non-comptime-folded — `u128` left-shift-by-64 and division, both
wrong), not a small bug: closing it needs multi-word type layout, N/64-word
LIR lowering with real carry propagation for add/sub, a genuine
multiplication algorithm, long division, and cross-word shifts, for every
arithmetic operator. No fix or regression test exists for this yet.

`emitStartStub` hand-encodes the process entry point (`_start`) directly as
raw opcode bytes (`encoder.emit1(state.*.enc, 0x48) ...`) — reading argc off
the stack, computing argv, calling `main`, and exiting via the `syscall`
instruction with the return value in `rdi` and syscall number 60 (`exit`) in
`rax`. Before that, `emitAggregateArena` issues an `mmap` syscall (syscall
number 9, `PROT_READ|PROT_WRITE`, `MAP_PRIVATE|MAP_ANONYMOUS`) to reserve a
256&nbsp;MiB (`268435464`-byte) arena at program startup for aggregate
allocations, with two pointers (`r14`, `r15`) tracking the arena's base and
current bump-allocation cursor across the whole program's execution — the
generated executable does its own bump-pointer heap management rather than
depending on any libc allocator. This is a direct, concrete instance of the
x86_64 spec's "Direct machine-code byte emission. No external assembler is
required" (`spec/02-compiler/x86_64.xml`).

**Known gap, severe: every `var x: T = undefined` for a struct/union/array
`T` permanently consumes space from that same 256&nbsp;MiB arena, for the
life of the whole process, no matter how short-lived or tightly-scoped the
variable is — a loop that declares one is a hard, silent crash waiting to
happen.** `codegen/memory.mlx`'s `emitAlloca` has two paths: a sized
allocation (`instruction.arg0 != NONE`, which is what every aggregate-typed
local literal is lowered to, in both `lowerBindingInitializer`'s `undefined`
case and `lowerAggregateLiteral`) always calls `emitArenaAllocation`, an
atomic-XADD bump of the arena cursor (`r14`) checked against the arena limit
(`r15`) with `emitTrap` on overflow; an unsized allocation instead reserves a
plain function-frame-relative slot via `state.*.nextStackSlot`, exactly like
an ordinary scalar local, correctly reused across every dynamic execution of
that instruction (loop iterations, repeated calls) because it's a
compile-time-fixed offset, not a runtime-growing cursor. Nothing routes
aggregate locals through the second, correctly-scoped path — every one goes
through the arena, and the arena is bumped **every time the `alloca`
instruction executes at runtime**, not once per place it appears in the
source. A loop (directly, or one call away — the same declaration site
inside a function called repeatedly from a loop counts too) that declares
such a local exhausts the arena and hits the trap after
roughly `268435464 / sizeof(T)` dynamic executions — confirmed with a
minimal repro, `while i < 2000000 { var x: SomeStruct = undefined; ... }`
for a 328-byte struct crashes (`SIGILL`, the `emitTrap`) at ~800,000
iterations (~262&nbsp;MB, matching the arena size); hoisting the identical
declaration above the loop and reusing the variable removes the ceiling
entirely (2,000,000 iterations, no crash, since the `alloca` then executes
once). This was found and worked around (not fixed) porting
`experiments/android-aarch64-bluescreen/tool/bignum.zig` to
`experiments/android-aarch64-bluescreen/mlx/bignum.mlx`: an RSA keygen's
inner loops (long division, modular exponentiation, the extended Euclidean
algorithm) each declare a handful of `BigUint` (328-byte struct) locals per
iteration by the most natural way to write them, and real key sizes run
those loops enough times to exhaust the arena within seconds. The general
shape of the workaround, applied throughout `bignum.mlx`: hoist every
loop-body `var x: T = undefined` to above its loop and reuse it by
overwriting through the existing pointer-based mutator functions (safe
because a limb-by-limb/field-by-field write never depends on a stale
previous value once every field is rewritten); where a function on a hot
path (called repeatedly from a loop, not just looping itself) needs its own
scratch aggregate, thread it in from the caller as an extra out-parameter
instead of declaring it locally (see `mulToScratch`/`biMulScratch` next to
the plain `mulTo`, which just supplies a fresh scratch buffer for
occasional/cold-path use). A real fix would extend the existing
correctly-scoped `nextStackSlot` path to sized allocations too — the
mechanism already exists and already reuses slots correctly across dynamic
executions for scalars — but doing that safely needs `functionFrameSize`
(`codegen/state.mlx`, currently `(instruction_count * 24 + 271)` rounded up,
with no awareness of aggregate sizes at all) to also account for the total
concurrent aggregate-local footprint of a function, which this port did not
attempt. No fix or regression test exists for this in the compiler itself
yet.

## ELF64 object writer (`object/elf64.mlx`)

`object/elf64.mlx`'s `buildExecutable` writes a complete `ET_EXEC` ELF64
file by hand: ELF header, one or two `PT_LOAD` program headers (text,
optionally read-only data), and section headers, all through raw
`elf.writeU16`/`writeU32`/`writeU64` byte-offset writes into a manually
allocated buffer — no object-file library or external linker is invoked
anywhere in this path. `LOAD_VADDR = 4194304` (`0x400000`) and
`TEXT_FILE_OFFSET = 4096` are hard fixed load addresses/alignment, matching
a static, non-PIE executable layout. This is the pipeline's stage 11
(`ObjectWriter`) and stage 12 (`IntegratedLink`) collapsed into one
self-contained function, consistent with the spec's `IntegratedLink` stage
existing at all — there is no separate `ld` invocation.

## Diagnostics (`diagnostic.mlx`)

`diagnostic.mlx`'s `Diagnostic` record is explicitly noted in its own
source comment to be a flattened version of the logical model: "Required
diagnostic fields are flattened for mlx0's bootstrap aggregate
representation. The logical model still matches spec/02-compiler." Its
`Code`, `Phase`, and `Severity` enums are the self-hosted compiler's
concrete implementation of the normative diagnostic identity described in
[docs/internals/compiler-pipeline.md](compiler-pipeline.md#diagnostic-model).

## Where the self-hosted implementation is narrower than the spec

Reading `diagnostic.mlx`'s `Code` enum against the full catalog in
`spec/02-compiler/diagnostics/codes.xml` shows a concrete gap: the
self-hosted compiler's `Code` enum currently defines only a subset of the
stable `MLX-E####` codes —

```mlx
pub const Code = enum(u16) {
    none = 0,
    unexpected_byte = 1001,
    unexpected_end_of_file = 1002,
    unexpected_token = 2001,
    unknown_identifier = 3001,
    duplicate_symbol = 3002,
    private_declaration = 3003,
    import_not_found = 3004,
    cyclic_comptime_import = 3005,
    type_mismatch = 4001,
    comptime_runtime_dependency = 5001,
    comptime_branch_quota_exceeded = 5002,
    compile_error_builtin = 5003,
    comptime_cycle = 5004,
    invalid_type_reflection = 5005,
    use_after_move = 6001,
    copy_noncopy = 6002,
    invalid_automatic_deinitializer = 6006,
    unsupported_instruction_lowering = 9001
}
```

(`compiler/selfhost/diagnostic.mlx`)

That is 18 codes against the full catalog's ~54 (all of `MLX-E1003/1004`,
most of `MLX-E2xxx`/`MLX-E4xxx`, `MLX-E6003`-`E6005`, all of
`MLX-E7xxx`/`MLX-E8xxx`, `MLX-E9002`/`E9003`, and every `MLX-Wxxxx` warning
are absent from this enum). This does not mean the self-hosted compiler
accepts programs the bootstrap compiler rejects — most of those conditions
(e.g. a genuine type mismatch inside a function body) are still caught by
sema and reported, just currently funneled through the coarser codes this
enum does define (e.g. `type_mismatch = 4001` used broadly, per
`driver/pipeline.mlx`'s failure calls). It does mean that a caller relying
on the self-hosted compiler's diagnostic *codes* for fine-grained tooling
(as opposed to the human message) should not yet expect the same code
granularity `mlx0`/the spec catalog promises. The `3001` member now uses
the `unknown_identifier` spelling, so its source-level name and numeric
identity align with the spec catalog's `UnknownIdentifier` entry.

## Contrast with the bootstrap compiler (`compiler/bootstrap/`)

`compiler/bootstrap/` is `mlx0`, the Zig implementation that exists solely
to bootstrap the self-hosted compiler above; its own `README.md` says
exactly that ("`mlx0` exists only to bootstrap the canonical Mlx compiler.
Keep dependencies minimal and follow `spec/` exactly"). `mlx0` is the
Stage-0 reference implementation the self-hosted compiler must match
semantically — where the two disagree, `spec/` (and the `SPEC_CONFLICTS.md`
gaps both must handle identically) is the tiebreaker, not either
implementation.

Its top-level layout (`compiler/bootstrap/ARCHITECTURE.md`) is organized by
pipeline concern rather than by self-hosted-style facade-plus-subdirectory:

```text
source/          files, locations and structured diagnostics
syntax/          tokens, lexer, AST and parser
modules/         import classification, package lookup and module graph
semantic/        types, scopes, ownership, comptime, reflection and sema
ir/              target-independent LIR and lowering
backend/x86_64/  ABI, register allocation, encoding and code generation
object/          ELF objects, linking and executable layout
platform/linux/  raw syscalls and host abstractions used by mlx0
driver/          command dispatch and compilation pipeline
```

Its own dependency rule ("Syntax never imports semantic code; semantic code
may import syntax and modules; IR may import semantic code; the backend may
import IR; object emission may import the backend encoder") mirrors the
strict top-to-bottom pipeline ordering the self-hosted compiler also
follows (`driver/pipeline.mlx`'s load → resolve → analyze declarations →
analyze bodies → lower → optimize → codegen → link sequence above), even
though the two are unrelated codebases in different languages. `mlx0` also
ships an LSP (`compiler/bootstrap/lsp.zig`, `mlx-lsp`) with no self-hosted
counterpart in `compiler/selfhost/` at this time. This repository's own
architecture doc for `mlx0` is a good place to go for Zig-level detail; this
page does not duplicate it.

## Runtime test fixtures

The tests below are not unit tests of isolated functions — each one drives
real source text through the self-hosted compiler's lexer/parser/sema/IR/
backend modules end to end and checks the concrete result, proving each
pipeline stage actually works rather than merely type-checking. A
representative few, read in full above or below:

- **`tests/145_selfhost_lexer_runtime.mlx`** — round-trips keyword,
  numeric-literal, string/byte-string/char-literal, long compound-operator
  (`<<%|=`, `+%<<=`, `->>=`, ...), nested-block-comment, and
  newline-to-`statement_end` filtering behavior directly against
  `lexer.next`/`lexer.scanNext`/`lexer.scanAll`.
- **`tests/151_selfhost_declaration_sema_runtime.mlx`** — parses a struct,
  two constants (one explicitly typed `u8`, one inferred as
  `comptime_int_type`), a multi-return function (`-> (u8, u16)`), and a
  function referencing a prior constant, then asserts on the resulting
  `Symbol`/`Type` records — including that a struct's implicit `Self`
  symbol is registered alongside its declared members.
- **`tests/155_selfhost_comptime_runtime.mlx`** — evaluates a small program
  mixing arithmetic, an `if`-expression, `@sizeOf`, and `@isInteger`, and
  checks the folded `comptimeInteger`/`comptimeTag` values sema attaches to
  each constant's `Symbol`, then re-checks a local inside a function body
  after `analyzeFunctionBodies` runs.
- **`tests/179_selfhost_lir_optimizer_runtime.mlx`** — builds a small LIR
  program by hand (`lir.addInst`/`lir.constI`/`lir.binary`/`lir.condBranch`)
  and asserts `optimize.run`'s exact fold/simplify/eliminate counts and the
  resulting opcodes (a `bit_or` of two constants becomes `const_i 255`; a
  `condbr` on a true constant becomes an unconditional `br`; unused
  instructions become `dead`).
- **`tests/233_selfhost_comptime_diagnostic_runtime.mlx`** — the
  quota-vs-unsupported distinction discussed above, plus a case (`enum(u16)
  { one = 1 / 0 ... }`) proving an enum whose explicit discriminant can't be
  evaluated at compile time is now rejected by `sema.analyzeDeclarations`
  rather than silently dropped.

`tests/234_slice_var_reassign_bootstrap_bug.mlx` is deliberately different:
it records a known bootstrap code-generation bug rather than a currently
passing conformance case. A slice-typed `var` initialized from `""` and then
reassigned to a non-empty pointer-derived slice retains a stale zero length;
the fixture therefore exits `1` under the current `mlx0`/`mlx1`, while exit
`13` is the expected result once the lowering bug is fixed. The self-hosted
driver avoids that pattern when carrying diagnostic paths, so the known bug
does not block compiler self-hosting.

The rest of the family, by name and what they exercise (inferred from their
imports and test number, following the same low-numbered/single-feature
convention the guide's `tests/` citations use):

| Fixture | Exercises |
| --- | --- |
| `140_selfhost_source_runtime.mlx` | `source.mlx` source-file loading |
| `141_selfhost_diagnostic_runtime.mlx` | `diagnostic.mlx` code/phase/severity/span construction |
| `146_selfhost_parser_runtime.mlx` | `parser.mlx` end-to-end parsing into the `ast/store.mlx` node store |
| `148_selfhost_types_runtime.mlx` | `types.mlx` type interning/layout |
| `149_selfhost_type_resolution_runtime.mlx` | resolving syntax type expressions to interned `Type`s |
| `150_selfhost_symbols_runtime.mlx` | `symbols.mlx` scope/symbol table construction |
| `152_selfhost_function_flow_sema_runtime.mlx` | `sema/functions.mlx` + `sema/control_flow.mlx` body analysis |
| `153_selfhost_builtins_runtime.mlx` | `sema/builtins/` registry arity/result typing |
| `154_selfhost_move_sema_runtime.mlx` | move/ownership state transitions in `sema/symbols/symbol.mlx` |
| `156_selfhost_aggregate_layout_runtime.mlx` | struct/enum/union layout computation in `sema/types/` |
| `157_selfhost_modules_runtime.mlx` | `modules.mlx` multi-file module graph loading |
| `158_selfhost_module_errors_runtime.mlx` | module-graph error paths (import cycles, missing imports) |
| `182_selfhost_atomic_encoder_runtime.mlx` | `backend/x86_64/encoder.mlx` atomic instruction encoding |
| `194_selfhost_lir_copy_propagation_runtime.mlx` | `ir/optimize.mlx`'s copy-propagation pass specifically |
| `195_selfhost_encoder_rax_cache_runtime.mlx` | the backend's `rax`-caching optimization (see `cachedRaxLoads` metric in `driver/pipeline.mlx`) |
| `196_selfhost_scope_index_runtime.mlx` | `symbols.mlx` scope indexing |
| `197_selfhost_encoder_immediate_runtime.mlx` | encoder immediate-operand handling |
| `199_selfhost_branch_fusion_runtime.mlx` | `ir/optimize.mlx`'s comparison-branch fusion pass |
| `200_selfhost_compact_encoding_runtime.mlx` | compact x86_64 instruction encoding forms |
| `201_selfhost_function_dce_runtime.mlx` | `ir/function_dce.mlx` unreachable-function elimination |
| `202_selfhost_post_terminator_dce_runtime.mlx` | `ir/control_flow_dce.mlx` post-terminator dead-code elimination |

Each of these imports directly from `compiler/selfhost/...` (rather than
compiling a `.mlx` file as a black box), so they test the self-hosted
compiler's internal module API surface — the same API this page describes
— not just its command-line behavior.

`157_selfhost_modules_runtime.mlx` in particular drives `modules.mlx`'s
`loadRoot`/`putPackage` functions directly against small fixture files under
`tests/modules/`: `157_root.mlx` exercises every normative import form in one
file (`@import("./157_values.mlx")`, a `././`-prefixed relative path,
`@import("builtin")`, a registered package name `@import("example")`
resolved through `putPackage` to `157_package.mlx`, a `@import("std.testing")`
dotted stdlib-style path, and a cyclic import), and `157_missing_root.mlx` /
`157_private_root.mlx` drive the missing-import and private-declaration
error paths that `158_selfhost_module_errors_runtime.mlx` asserts against.
