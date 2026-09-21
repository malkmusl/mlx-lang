# Errors

**Source tests:** `36_try_requires_error_function.mlx`,
`37_error_void_fallthrough.mlx`, `38_try_in_error_function.mlx`,
`39_unhandled_error_union.mlx`, `40_error_payload_return_mismatch.mlx`,
`41_error_nonvoid_missing_return.mlx`, `46_error_union_abi_runtime.mlx`,
`79_defer_lifo_runtime.mlx`, `80_defer_break_runtime.mlx`,
`81_errdefer_success_runtime.mlx`, `82_errdefer_requires_error_function.mlx`,
`101_error_set_runtime.mlx`, `102_error_set_mismatch.mlx`,
`103_error_set_duplicate.mlx`

Mlx has no exceptions. Fallibility is part of a function's type via error
unions, and propagation is always explicit (`try`).

## Error unions

`!T` is an error union: either an error, or a payload of type `T`. `!void`
is an error union that carries no payload on success.

```mlx
fn fallible() !u8 {
    return 7
}

pub fn main() u8 {
    return try fallible()
}
```

`try` unwraps a `!T` and, if it holds an error, propagates that error out of
the *current* function — which means `try` can only be used inside a
function whose own return type is also an error union:

```mlx
fn fallible() !u8 {
    return 7
}

pub fn main() u8 {
    return try fallible()   // MLX-E4008: main's return type (u8) isn't an error union
}
```

(`tests/36_try_requires_error_function.mlx`)

```mlx
fn source() !void {
}

fn consume() !void {
    try source()   // fine: consume() is itself a !void function
}
```

(`tests/38_try_in_error_function.mlx`)

A `!void` function may simply fall off the end on success:

```mlx
fn completes() !void {
}
```

(`tests/37_error_void_fallthrough.mlx`)

Calling a function that returns an error union without unwrapping it (via
`try` or an explicit `match`/handling construct) is a compile-time error —
an error union can never be silently discarded:

```mlx
fn source() !void {
}

pub fn main() u8 {
    source()   // MLX-E4008: unhandled error union
    return 0
}
```

(`tests/39_unhandled_error_union.mlx`)

Just like an ordinary function, an error union's success payload type is
checked (`tests/40_error_payload_return_mismatch.mlx`) and every reachable
path through a non-`void` error-union function must return
(`tests/41_error_nonvoid_missing_return.mlx`).

`!T` propagates correctly through call chains, including across the ABI
boundary of a real function call:

```mlx
fn source() !u8 {
    return 7
}

fn forward() !u8 {
    return source()
}

pub fn main() !void {
    const value = try forward()
    while value != 7 {
    }
    return
}
```

(`tests/46_error_union_abi_runtime.mlx`)

## Named error sets

An `error { ... }` declaration defines a named set of error values. A
function's error union can be scoped to a specific error set
(`SetName!T`) instead of the anonymous "any error" set:

```mlx
const Failure = error {
    NotFound,
    AccessDenied
}

fn fail() Failure!u8 {
    return Failure.NotFound
}

fn propagate() !u8 {
    return try fail()
}

fn main() !void {
    const ignored = try propagate()
}
```

(`tests/101_error_set_runtime.mlx`)

A function declared to return one named error set cannot return a value
from a different error set:

```mlx
const ReadError = error { NotFound }
const WriteError = error { ReadOnly }

fn broken() ReadError!void {
    return WriteError.ReadOnly   // rejected: WriteError.ReadOnly isn't in ReadError
}
```

(`tests/102_error_set_mismatch.mlx`)

Declaring the same error name twice in one set is rejected:

```mlx
const Broken = error {
    Invalid,
    Invalid
}
```

(`tests/103_error_set_duplicate.mlx`)

## `defer` and `errdefer`

`defer` schedules a statement to run when the enclosing block exits, in
LIFO order — the same lexical-scope-exit machinery that drives automatic
drop (see [Ownership](06-ownership.md)):

```mlx
fn main() u8 {
    var value: u8 = 1
    {
        defer value += 3
        defer value *= 2
        value += 4
    }
    return value   // (1 + 4) * 2 + 3 = 13
}
```

(`tests/79_defer_lifo_runtime.mlx` → 13)

A `defer` inside a loop body still runs when the loop is exited via `break`:

```mlx
fn main() u8 {
    var value: u8 = 1
    while true {
        defer value += 4
        break
    }
    return value   // 5
}
```

(`tests/80_defer_break_runtime.mlx`)

`errdefer` schedules a statement that only runs when the function is
unwinding because of a propagated error — not on the success path. Because
of that, `errdefer` is only meaningful (and only allowed) inside a function
that returns an error union:

```mlx
fn source() !u8 {
    var value: u8 = 3
    errdefer value += 5   // only runs if this function returns an error
    return value
}
```

(`tests/81_errdefer_success_runtime.mlx`)

```mlx
fn main() u8 {
    errdefer {}   // rejected: errdefer requires an error-returning function
    return 0
}
```

(`tests/82_errdefer_requires_error_function.mlx`)
