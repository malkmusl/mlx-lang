# Diagnostics: self-hosted and module-error fixtures

**Companion to:** [docs/guide/11-diagnostics.md](../guide/11-diagnostics.md)

**Normative source:** `spec/02-compiler/diagnostics.xml` (diagnostic model)
and `spec/02-compiler/diagnostics/codes.xml` (the stable code catalog) — same
sources as the guide chapter this page extends.

`docs/guide/11-diagnostics.md` scoped itself to `tests/` fixtures in the
`01`–`232` range, excluding anything named `*_selfhost_*` or `*_bootstrap_*`
(see that guide's [README](../guide/README.md) for the full scope
statement). This page picks up where it left off: it documents the
diagnostic-shaped fixtures that scoping rule left out, plus a couple of
fixtures in-range that the guide's code table lists with no example (`—`)
or cites without a worked example. It does not repeat any table row the
guide already has a fixture for — `tests/100_union_explicit_tag_mismatch.mlx`
(`MLX-E4006`-adjacent, covered under Types and aggregates),
`tests/102_error_set_mismatch.mlx` and `tests/103_error_set_duplicate.mlx`
(covered under Errors) are skipped here for that reason.

## `MLX-E4001` (`TypeMismatch`): tuple return arity

The guide's table cites `tests/09_type_errors.mlx` for `MLX-E4001`, but a
distinct and non-obvious use of the same code is a multi-return arity
mismatch. `tests/119_multi_return_arity_error.mlx` declares a function whose
return type is a two-element tuple and returns three values instead:

```mlx
fn broken() -> (u8, u8) {
    return -> (1, 2, 3)
}

fn main() -> u8 {
    return 0
}
```

(`tests/119_multi_return_arity_error.mlx`)

This is grounded directly in the bootstrap compiler's semantic analyzer, not
inferred: `compiler/bootstrap/semantic/control_flow/return.zig`'s
`analyzeTupleReturn` special-cases a `return -> (...)` tuple literal against
a tuple-typed function return, and reports the arity mismatch with the exact
code `4001`:

```zig
const count = sema.ast_tree.extra_data[expression.data.lhs];
if (count != expected_fields.len) {
    try sema.reportError(4001, .sema, sema.ast_tree.tokens[expression.main_token].start, "Returned tuple has the wrong number of values");
}
```

(`compiler/bootstrap/semantic/control_flow/return.zig`)

So a tuple-return arity mismatch is folded into `TypeMismatch` rather than
getting its own code — there is no separate "arity" diagnostic in
`spec/02-compiler/diagnostics/codes.xml`, and this repository's own rule
against inventing behavior means this page states that as what the source
shows, not as an assumption. The same function also reports `4001` for a
per-value type mismatch inside a returned tuple, and `4002`
(`IntegerOutOfRange`) if a comptime-integer tuple element doesn't fit its
declared slot's type — both in the same file, immediately below the arity
check.

## `MLX-E3003`/`MLX-E3004` internals: the self-hosted module loader's error accounting

The guide's table already cites fixtures for `MLX-E3003` (`PrivateDeclaration`,
`tests/76_import_private_error.mlx`) and `MLX-E3004` (`ImportNotFound`,
`tests/77_import_not_found_error.mlx`). `tests/158_selfhost_module_errors_runtime.mlx`
exercises the same two error classes from inside the self-hosted compiler's
own module loader (`compiler/selfhost/modules.mlx`), which is useful
precisely because it shows the *internal bookkeeping* those single-feature
fixtures don't: module counts, per-module state, and total error counts.

Its private-access case drives the loader against
`tests/modules/157_private_root.mlx`:

```mlx
const values = @import("./157_values.mlx")
const stolen = values.secret
```

(`tests/modules/157_private_root.mlx`), where `157_values.mlx` declares
`secret` without `pub`:

```mlx
pub const Byte = u8
pub const answer: Byte = 42
const secret: Byte = 7
```

(`tests/modules/157_values.mlx`)

The driving assertions in `158_selfhost_module_errors_runtime.mlx` show
*where* in the pipeline this gets caught — not at parse/prepare, but at
declaration analysis:

```mlx
const private_loader = modules.initLoader(allocator, @ptrCast([*]const u8, "std/src"), 7, null)
const private_root = modules.loadRoot(private_loader, @ptrCast([*]const u8, "tests/modules/157_private_root.mlx"), 34)
if private_root == modules.NONE || modules.totalErrors(private_loader) != 0 { return 1 }
var private_types = types.init(allocator)
var private_symbols = symbols.initStore(allocator, 16)
if modules.prepare(private_loader, &private_types, &private_symbols) != 0 { return 2 }
if modules.analyzeDeclarations(private_loader, &private_types, &private_symbols) == 0 { return 3 }
```

(`tests/158_selfhost_module_errors_runtime.mlx`)

Read as assertions: `loadRoot` and `prepare` must both succeed with zero
errors (the private access isn't a load-time or name-resolution-time
failure — `stolen` does resolve to a real declaration), and it's
specifically `analyzeDeclarations` that must report a nonzero error count,
consistent with `codes.xml` fixing `MLX-E3003`'s phase as `resolve`.

Its missing-import case is symmetric, driving the loader against
`tests/modules/157_missing_root.mlx`:

```mlx
const missing = @import("./does-not-exist.mlx")
```

(`tests/modules/157_missing_root.mlx`), and this time asserting on the
loader's module table directly:

```mlx
const missing_loader = modules.initLoader(allocator, @ptrCast([*]const u8, "std/src"), 7, null)
const missing_root = modules.loadRoot(missing_loader, @ptrCast([*]const u8, "tests/modules/157_missing_root.mlx"), 34)
if missing_root == modules.NONE || modules.totalErrors(missing_loader) == 0 { return 4 }
if modules.moduleCount(missing_loader) != 2 { return 5 }
if modules.moduleAt(missing_loader, 1).*.state != modules.State.failed { return 6 }
```

(`tests/158_selfhost_module_errors_runtime.mlx`)

This documents something the simple `tests/77_import_not_found_error.mlx`
fixture can't, by itself: a failed `@import` still allocates a module-table
entry (`moduleCount` is `2` — the root plus a stub for the unresolved
import) and that entry is left in an explicit `failed` state rather than
being silently dropped from the table, which is how the loader is able to
report a nonzero `totalErrors` while still letting the rest of the root
module's declarations be analyzed.

## `MLX-E5001`/`MLX-E5002`: three worked comptime-diagnostic cases

The guide cites `tests/233_selfhost_comptime_diagnostic_runtime.mlx` for
`MLX-E5002` (`ComptimeBranchQuotaExceeded`) without a worked example. The
fixture actually drives the self-hosted compiler's comptime evaluator
(`compiler/selfhost/comptime.mlx`) through three distinct outcomes in one
file, and its own comments are explicit about the code-selection logic
being tested — worth quoting rather than paraphrasing:

**1. Branch-quota exhaustion must report `MLX-E5002`, not the generic
`MLX-E5001`.** The comment states the intent directly:

```mlx
// A comptime-evaluable expression that exhausts a deliberately tiny
// branch quota must be distinguished from a generically unsupported
// expression so callers can report MLX-E5002 instead of MLX-E5001.
const quota_source = "const value = 1 + 2 + 3\n"
...
const exhausted = comptime_module.evaluate(quota_source, quota_tokens, &quota_nodes, &quota_types, &quota_symbols, &quota_scope, quota_initializer, 1)
if comptime_module.isValid(exhausted) { return 2 }
if exhausted.reason != comptime_module.Reason.quota_exceeded { return 3 }
```

(`tests/233_selfhost_comptime_diagnostic_runtime.mlx`) — note the last
argument to `evaluate` here is `1`, a deliberately starved branch-step
budget for evaluating `1 + 2 + 3`, and the assertion is on
`comptime_module.Reason.quota_exceeded` specifically, not just "evaluation
failed."

**2. A generically unsupported comptime expression keeps the default
"unsupported" reason.** Evaluating a string literal where a numeric constant
is expected, with a generous budget (`1000000`), still fails, but with a
different `Reason`:

```mlx
// A generically unsupported comptime expression keeps the default
// "unsupported" reason, not "quota_exceeded".
const bad_source = "const value = \"not a number\"\n"
...
const unsupported = comptime_module.evaluate(bad_source, bad_tokens, &bad_nodes, &bad_types, &bad_symbols, &bad_scope, bad_initializer, 1000000)
if comptime_module.isValid(unsupported) { return 5 }
if unsupported.reason != comptime_module.Reason.unsupported { return 6 }
```

(`tests/233_selfhost_comptime_diagnostic_runtime.mlx`)

Together, cases 1 and 2 are what ground this page's summary of the code
split: the evaluator's `Reason` enum distinguishes `quota_exceeded` from
`unsupported` internally, which is the mechanism that lets the compiler
choose `MLX-E5002` over the generic `MLX-E5001` (`ComptimeRuntimeDependency`)
specifically for the quota case, per the file's own comment. This page
does not assert that `Reason.unsupported` maps 1:1 onto `MLX-E5001`
specifically — `codes.xml` names `MLX-E5001` `ComptimeRuntimeDependency`,
which reads as a narrower condition (a comptime expression depending on a
runtime value) than "generically unsupported," and the test doesn't state
that mapping explicitly, so it isn't repeated as fact here.

**3. A comptime-unevaluable enum discriminant must now be rejected, not
silently dropped.** The third case is a full sema pass, not just an
evaluator call, and its comment documents a behavior change:

```mlx
// An enum whose explicit discriminant cannot be evaluated at compile
// time must now be rejected instead of silently discarding the type.
const enum_source = "const Bad = enum(u16) {\none = 1 / 0\ntwo\n}\n"
...
if sema.analyzeDeclarations(enum_source, enum_tokens, &enum_nodes, enum_root, &enum_types, &enum_symbols, &enum_scope) == 0 { return 9 }
```

(`tests/233_selfhost_comptime_diagnostic_runtime.mlx`)

The discriminant expression is `1 / 0` — comptime division by zero, which
`spec/00-language/memory-model.xml` fixes as a compile error
(`<DivisionByZero safe="trap" comptime="compile error"/>`, see
[docs/reference/memory-and-concurrency.md](memory-and-concurrency.md)).
`analyzeDeclarations` must return a nonzero error count here too. Neither
the test nor its comment names a specific `MLX-E####` code for this case, so
this page names the rule it enforces (comptime division-by-zero in an enum
discriminant is a compile error, and the enum declaration must be rejected
rather than have its bad discriminant silently ignored) without assigning
it a code number that isn't in evidence.

## `MLX-E2004` worked example: the trace/statement-terminator fixture

The guide's table already cites `tests/230_trace_syntax_error.mlx` for
`MLX-E2004` (`InvalidStatementTermination`) but has no room in a table row
for a worked example. The fixture is a two-line function with a trailing
semicolon on its `return` statement:

```mlx
// The trace reports the invalid statement terminator below.
fn main() -> u8 {
    return 0;
}
```

(`tests/230_trace_syntax_error.mlx`)

Mlx statements are newline/brace-terminated, not semicolon-terminated (no
fixture or spec chapter in this guide's scope introduces `;` as a statement
terminator), so the trailing `;` after `return 0` is the invalid statement
termination the parser rejects with `MLX-E2004`. The fixture's own comment —
"the trace reports the invalid statement terminator below" — signals this
is also a diagnostic-*rendering* fixture (checking that the compiler's
trace/pointer output lands on the `;`), not merely a
does-it-fail-to-compile check; that rendering aspect is orthogonal to the
code itself, which is why `run_error.sh`'s grep-for-the-code check is the
right level of verification for it (as for every other fixture on this
page), rather than asserting on the exact trace text.
