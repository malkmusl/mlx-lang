# Basics

**Source tests:** `01_basic.mlx`, `10_decl_modifiers.mlx`, `11_builtins_core.mlx`,
`19_cast_out_of_range.mlx`, `33_function_call_result_type.mlx`

## Comments

Line comments start with `//` and run to the end of the line. By convention,
every fixture in `tests/` opens with a one-line comment describing what it
tests and what result to expect:

```mlx
// Tests basic const and var declarations by defining a value and copying it into mutable storage.
const a = 1
var b = a
```

(`tests/01_basic.mlx`)

## `const` and `var`

- `const` declares an immutable binding.
- `var` declares a mutable binding.

Statements are newline-terminated — there is no `;` at the end of a
statement.

## Declaration modifiers

`pub` exposes a declaration outside its module (see [Modules](10-modules.md)).
It is the only modifier used at this level; `inline`/`noinline` are function
modifiers (see [Functions](04-functions.md)).

```mlx
// Tests the pub declaration modifier by exposing main and returning the exit code 7.
pub fn main() u8 {
    return 7
}
```

(`tests/10_decl_modifiers.mlx`)

## The `main` function

An executable's entry point is `fn main()`. Its return type is typically an
unsigned integer that becomes the process exit code (`u8` in most fixtures).
`main` may also return an error union (`!void`) — see
[Errors](07-errors.md).

## Primitive types and casts

Mlx has arbitrary-width signed/unsigned integers `iN`/`uN` for `1..4096`
bits (e.g. `u8`, `i32`, `u13`), plus floats, `bool`, and `usize`. An explicit
type annotation on a binding is checked against the initializer's type:

```mlx
fn main() u8 {
    const x: i32 = 5
    const bad: u8 = x   // MLX-E4001: i32 is not assignable to u8 without a cast
    return bad
}
```

(`tests/09_type_errors.mlx` — rejected with `MLX-E4001`)

Narrowing an integer requires an explicit cast such as `@intCast(T, value)`.
The cast is range-checked at compile time when possible:

```mlx
// Tests integer range checking by casting 256 to u8; compilation must emit MLX-E4002.
```

(`tests/19_cast_out_of_range.mlx` — `@intCast(u8, 256)` is rejected with `MLX-E4002`
because 256 does not fit in a `u8`)

A function call's result can be bound with an explicit type and then narrowed
explicitly:

```mlx
fn add(a: i32, b: i32) i32 {
    return a + b
}

fn main() u8 {
    const sum: i32 = add(40, 2)
    return @intCast(u8, sum)
}
```

(adapted from `tests/33_function_call_result_type.mlx`, which returns 42)

## Core reflection/cast builtins

A handful of builtins are used throughout the test suite for compile-time
introspection; see [Comptime and generics](08-comptime-and-generics.md) for
the full list. The essentials:

```mlx
// Tests core type, layout, predicate, and cast builtins by combining their compile-time results.
pub fn main() u8 {
    const value: u8 = 7
    const ValueType = @typeOf(value)
    const layout = @sizeOf(ValueType) + @alignOf(u32) + @bitSizeOf(i13)
    const predicates = @isInteger(ValueType) + @isFloat(f32) + @isPointer(*u8)
        + @isSlice([]u8) + @isArray([3]u8) + @isOptional(?u8) + @isErrorUnion(!u8)
        + @isCopyable(u8)
    const casted = @intCast(u8, 7)
    return layout + predicates + casted
}
```

(`tests/11_builtins_core.mlx`)
