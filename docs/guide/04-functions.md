# Functions

**Source tests:** `07_functions.mlx`, `08_type_annotations.mlx`,
`28_missing_function_return_type.mlx`, `29_function_return_type_mismatch.mlx`,
`30_function_call_arity.mlx`, `31_function_call_argument_type.mlx`,
`32_function_missing_return.mlx`, `34_void_return.mlx`,
`35_forward_function_call.mlx`, `115_multi_return_runtime.mlx`,
`116_multi_return_two_registers_runtime.mlx`, `130_function_pointer_runtime.mlx`,
`144_bootstrap_void_main_runtime.mlx`, `147_recursive_function_runtime.mlx`,
`188_inline_function_runtime.mlx`, `189_optimization_level_inline_runtime.mlx`,
`190_recursive_inline_guard_runtime.mlx`, `191_inline_multi_return_runtime.mlx`,
`192_inline_argument_once_runtime.mlx`, `193_inline_unsafe_accessor_runtime.mlx`,
`225_runtime_bounds_safety.mlx`

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

A function's return value can also simply be discarded by binding it and
never reading the binding again — nothing requires every call's result to be
used, and a `void`-returning `main` doesn't need to touch whatever a called
function handed back:

```mlx
fn touchReturnRegister() -> u8 {
    return 47
}

pub fn main() -> void {
    const value = touchReturnRegister()
}
```

(`tests/144_bootstrap_void_main_runtime.mlx`)

## Recursion

Ordinary (self- and mutual-) recursion works without any special
declaration. Because declaration order doesn't matter (see above), two
functions can call each other directly:

```mlx
fn countdown(value: u8) -> u8 {
    if value == 0 { return 13 }
    return countdown(value - 1)
}

fn is_even(value: u8) -> bool {
    if value == 0 { return true }
    return is_odd(value - 1)
}

fn is_odd(value: u8) -> bool {
    if value == 0 { return false }
    return is_even(value - 1)
}

pub fn main() -> u8 {
    if !is_even(4) { return 1 }
    return countdown(3)
}
```

(`tests/147_recursive_function_runtime.mlx`)

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

(adapted from `tests/188_inline_function_runtime.mlx`; `noinline` is also
used in the runtime bounds-safety fixtures, e.g.
`tests/225_runtime_bounds_safety.mlx`, to force a real call boundary instead
of letting the optimizer prove the index in range)

`inline` also works on a method declared inside a struct, and across a
module boundary — the full fixture imports both an `inline` free function
and a struct with an `inline` method from another file:

```mlx
// tests/modules/188_inline.mlx
pub inline fn combine(left: u8, right: u8) -> u8 {
    return left + right
}

pub const Number = struct {
    value: u8

    pub inline fn current(self: Self) -> u8 {
        return self.value
    }
}
```

```mlx
const helper = @import("./modules/188_inline.mlx")

noinline fn identity(value: u8) -> u8 {
    return value
}

inline fn wrapper(value: u8) -> u8 {
    return identity(value)
}

inline fn narrow(value: usize) -> u8 {
    return @intCast(u8, value)
}

pub fn main() -> u8 {
    const number = helper.Number.{ .value = 8 }
    return wrapper(narrow(@intCast(usize, helper.combine(5, number.current()))))
}
```

(`tests/188_inline_function_runtime.mlx`, importing `tests/modules/188_inline.mlx`)

`noinline` composes with ordinary calls at any optimization level — nesting
several `noinline`/plain calls still produces the right result whether or
not the compiler would otherwise have inlined them:

```mlx
fn combine(left: u8, right: u8) -> u8 {
    return left + right
}

fn combineSix(a: u8, b: u8, c: u8, d: u8, e: u8, f: u8) -> u8 {
    return a + b + c + d + e + f
}

noinline fn identity(value: u8) -> u8 {
    return value
}

pub fn main() u8 {
    return identity(combine(combineSix(1, 2, 3, 4, 1, 1), 1))
}
```

(`tests/189_optimization_level_inline_runtime.mlx`)

A recursive `inline` function is still safe: per `compiler/selfhost/README.md`,
"recursive expansion is bounded and falls back to an ordinary call," so
`inline`-requesting a recursive function doesn't attempt infinite compile-time
expansion — the compiler expands it a bounded number of times and then emits
a normal call for the rest:

```mlx
inline fn countdown(value: u8) -> u8 {
    return if value == 0 { 13 } else { countdown(value - 1) }
}

pub fn main() u8 {
    return countdown(3)
}
```

(`tests/190_recursive_inline_guard_runtime.mlx`)

Inlining composes correctly with the other function features in this
chapter: an inline function can return a multiple-return tuple,

```mlx
inline fn coordinates() -> (u8, u8) {
    return -> (5, 8)
}

pub fn main() -> u8 {
    const point = coordinates()
    return point.0 + point.1
}
```

(`tests/191_inline_multi_return_runtime.mlx`)

an inline function's argument is still evaluated exactly once even if
evaluating it has a side effect — inlining never duplicates an argument
expression's side effects, even though it substitutes the argument value
into the callee's body:

```mlx
fn advance(value: *u8) -> u8 {
    value.* += 1
    return value.*
}

inline fn duplicate(value: u8) -> u8 {
    return value + value
}

pub fn main() -> u8 {
    var current: u8 = 5
    if duplicate(advance(&current)) != 12 { return 1 }   // (5+1) + (5+1), not (5+1)+(6+1)
    if current != 6 { return 2 }   // advance() only ran once
    return 13
}
```

(`tests/192_inline_argument_once_runtime.mlx`)

and an inline function may itself use `unsafe {}` internally — the unsafe
boundary is a property of the function body, not something inlining strips
or requires the caller to repeat:

```mlx
const Cell = struct { value: u8 }

inline fn read(pointer: *Cell) -> u8 {
    unsafe { return pointer.*.value }
}

pub fn main(arguments: [][*]const u8) -> u8 {
    var cell = Cell.{ .value = 17 }
    return read(&cell)
}
```

(`tests/193_inline_unsafe_accessor_runtime.mlx`)
