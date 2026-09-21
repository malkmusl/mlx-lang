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
eliminated instructions, `aggregate_copy`, and separate `udiv`/`urem` next
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
    unknown_type = 3001,
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
granularity `mlx0`/the spec catalog promises; the id also uses the
non-normative name `unknown_type` at `3001` where the spec's catalog calls
that slot `UnknownIdentifier`. Also worth noting: `id 3001` in this enum is
called `unknown_type`, not `UnknownIdentifier` as `codes.xml` names it —
a naming drift worth being aware of if cross-referencing by name rather
than numeric code.

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
