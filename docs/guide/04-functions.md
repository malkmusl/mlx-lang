# Functions

**Source tests:** `07_functions.mlx`, `08_type_annotations.mlx`,
`28_missing_function_return_type.mlx`, `29_function_return_type_mismatch.mlx`,
`30_function_call_arity.mlx`, `31_function_call_argument_type.mlx`,
`32_function_missing_return.mlx`, `34_void_return.mlx`,
`35_forward_function_call.mlx`, `115_multi_return_runtime.mlx`,
`116_multi_return_two_registers_runtime.mlx`, `130_function_pointer_runtime.mlx`,
`188_inline_function_runtime.mlx`, `225_runtime_bounds_safety.mlx`

## Declaration syntax

A function declaration is `fn NAME(params) -> ReturnType { ... }`. The
canonical grammar (`spec/00-language/grammar.ebnf`) requires the `->` arrow;
some of the lowest-numbered fixtures in the suite predate that and omit it
(`fn main() u8 { ... }`), which the bootstrap compiler still accepts, but new
code should always write the arrow form.

```mlx
fn add(a: comptime_int, b: comptime_int) -> comptime_int {
    return a + b
}

fn main() -> comptime_int {
    const x = add(10, 5)
    return x
}
```

(`tests/07_functions.mlx` → 15)

Explicit types on parameters and locals are checked the same way as any
other binding:

```mlx
// Tests explicit i32 annotations and typed function calls; main returns 5 + 10 = 15.
```

(`tests/08_type_annotations.mlx`)

`pub` exposes a function to importers (see [Modules](10-modules.md)):

```mlx
pub fn main() u8 {
    return 7
}
```

## Return type and reachability checking

A function's return type must be spelled explicitly if it returns a value
(`tests/28_missing_function_return_type.mlx`), the returned expression's type
must match it (`tests/29_function_return_type_mismatch.mlx`), and every
reachable path through a non-`void` function must return
(`tests/32_function_missing_return.mlx`). A `void`-returning function may
simply fall off the end:

```mlx
// Tests calling a void function and returning normally from main; the expected exit code is 0.
```

(`tests/34_void_return.mlx`)

## Calls

Call arity and argument types are checked against the declaration:
`tests/30_function_call_arity.mlx` rejects a two-parameter function called
with one argument, and `tests/31_function_call_argument_type.mlx` rejects
passing a string where an `i32` is expected.

Functions may be called before their declaration appears in the file —
declaration order within a module does not matter:

```mlx
// Tests forward function resolution by calling later() before its declaration; main returns 9.
```

(`tests/35_forward_function_call.mlx`)

## Multiple returns

A function can return a tuple directly, without wrapping it in a struct.
`return -> (a, b)` returns the tuple; the caller accesses members positionally
with `.0`, `.1`, …

```mlx
fn connect() -> (u8, u8) {
    return -> (7, 6)
}

fn main() -> u8 {
    const result = connect()
    return result.0 + result.1
}
```

(`tests/115_multi_return_runtime.mlx` → 13; see also
`tests/116_multi_return_two_registers_runtime.mlx`)

## Function pointers

A function pointer's type is `fn(ParamTypes...) -> ReturnType`; a
plain function name decays to a value of that type:

```mlx
fn increment(value: u8) -> u8 {
    return value + 1
}

fn apply(callback: fn(u8) -> u8, value: u8) -> u8 {
    return callback(value)
}

pub fn main() -> u8 {
    const callback: fn(value: u8) -> u8 = increment
    return apply(callback, 12)
}
```

(`tests/130_function_pointer_runtime.mlx` → 13)

## `inline`/`noinline`

The `inline` and `noinline` modifiers are a request/veto, not a semantic
guarantee: `inline` asks the compiler to inline the call as a best effort,
`noinline` forbids it. Neither changes program behavior if the compiler
cannot honor `inline` (see `SPEC_CONFLICTS.md`, "Function inline modifier
strength").

```mlx
noinline fn identity(value: u8) -> u8 {
    return value
}

inline fn wrapper(value: u8) -> u8 {
    return identity(value)
}
```

(`tests/188_inline_function_runtime.mlx`; `noinline` is also used in the
runtime bounds-safety fixtures, e.g. `tests/225_runtime_bounds_safety.mlx`,
to force a real call boundary instead of letting the optimizer prove the
index in range)
