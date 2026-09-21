# Comptime and generics

**Source tests:** `04_comptime.mlx`, `05_lir.mlx`, `12_unknown_builtin.mlx`,
`13_compile_error_builtin.mlx`, `24_reflection_schema_gap.mlx`,
`25_atomic_signature_gap.mlx`, `26_vector_signature_gap.mlx`,
`74_comptime_runtime_dependency.mlx`, `106_anytype_generic_runtime.mlx`,
`107_comptime_type_generic_runtime.mlx`, `108_comptime_value_generic_runtime.mlx`,
`109_comptime_argument_runtime_dependency.mlx`, `110_generic_dependent_type_range.mlx`,
`111_generic_type_return_runtime.mlx`, `112_comptime_if_pruning_runtime.mlx`

Mlx has no separate template/macro system. Generics are ordinary functions
whose parameters (or whose parameter *types*) are known at compile time,
combined with `type` being a first-class comptime value.

Every `const` initializer built purely from other compile-time-known values
is folded during compilation (constant arithmetic reaches the compiler's LIR
stage as an already-computed constant rather than emitted code):

```mlx
const a = 1
const b = 2
const c = a + b
const d = c * 5
```

(`tests/05_lir.mlx`)

## `comptime` parameters

A parameter marked `comptime` must be resolvable at compile time for every
call site. The classic use is a type parameter:

```mlx
fn choose(comptime T: type, value: T) T {
    return value
}

fn main() u8 {
    const small = choose(u8, 10)
    const wide = choose(u16, 3)
    return small + @intCast(u8, wide)
}
```

(`tests/107_comptime_type_generic_runtime.mlx`)

The comptime parameter doesn't have to be a type — any comptime-known value
works, e.g. a fixed amount baked into a computation:

```mlx
fn add(comptime amount: u8, value: u8) u8 {
    return value + amount
}

fn main() u8 {
    return add(5, 8)   // 13
}
```

(`tests/108_comptime_value_generic_runtime.mlx`)

A `comptime` parameter is checked against its declared type even when the
value only becomes concrete through generic instantiation — passing a value
outside that type's range is a compile-time error, not a runtime one:

```mlx
fn choose(comptime T: type, value: T) T {
    return value
}

fn main() u8 {
    return choose(u8, 300)   // rejected: 300 doesn't fit u8
}
```

(`tests/110_generic_dependent_type_range.mlx`)

## Functions returning `type`

Because `type` is a normal comptime value, a function can compute and return
a type. Combined with `const`, this gives type aliases that depend on
compile-time logic:

```mlx
fn Same(comptime T: type) type {
    return T
}

const Byte = Same(u8)

fn main() u8 {
    const value: Byte = 13
    return value
}
```

(`tests/111_generic_type_return_runtime.mlx`)

## `anytype`

`anytype` accepts a value of any type without requiring the caller to name
it explicitly; the function body is instantiated per concrete argument type,
similar to a `comptime T: type` parameter used only to type one argument:

```mlx
fn identity(value: anytype) anytype {
    return value
}

fn main() u8 {
    return identity(10) + identity(3)
}
```

(`tests/106_anytype_generic_runtime.mlx`)

## Comptime/runtime dependency checking

A `comptime` parameter must actually receive a comptime-known argument —
passing a `var` whose value is only known at runtime is rejected, even
though the parameter's *type* would otherwise fit:

```mlx
fn select(comptime value: u8) u8 {
    return value
}

fn main() u8 {
    var runtime: u8 = 7
    return select(runtime)   // rejected: `runtime` isn't comptime-known
}
```

(`tests/109_comptime_argument_runtime_dependency.mlx`; see also the `comptime`
expression form in `tests/74_comptime_runtime_dependency.mlx`, which forces
an expression to be evaluated at compile time and is rejected the same way
when its operand depends on a runtime value)

## Comptime-if pruning

An `if` whose condition is comptime-known prunes the untaken branch entirely
at compile time — it is not merely a runtime branch the optimizer might
remove, it is never type-checked/generated for the untaken side. This is
observable because a `@compileError` placed in the untaken branch does not
fire:

```mlx
fn main() u8 {
    return if true {
        13
    } else {
        @compileError("unselected branch must be pruned")
    }
}
```

(`tests/112_comptime_if_pruning_runtime.mlx` → 13)

## Reflection builtins

The core reflection surface used throughout the suite:

| Builtin | Purpose |
| --- | --- |
| `@typeOf(value)` | the static type of an expression |
| `@sizeOf(T)` | byte size of a type |
| `@alignOf(T)` | required alignment of a type |
| `@bitSizeOf(T)` | bit width of a type |
| `@fieldCount(T)` | number of fields of a struct/enum/union |
| `@offsetOf(T, "name")` | byte offset of a field |
| `@fieldType(T, "name")` | type of a named field |
| `@hasField(T, "name")` | whether a field exists |
| `@isInteger`/`@isFloat`/`@isPointer`/`@isSlice`/`@isArray`/`@isOptional`/`@isErrorUnion`/`@isStruct`/`@isEnum`/`@isUnion`/`@isCopyable(T)` | type-category predicates |
| `@intCast(T, value)` | range-checked integer conversion |
| `@intFromEnum(value)` / `@tagOf(value)` | integer/tag value of an enum or tagged-union instance |

```mlx
const a = 1
const b = @typeOf(a)
const c = @sizeOf(b)
```

(`tests/04_comptime.mlx`)

Calling an unknown/undefined builtin name is rejected at compile time
(`tests/12_unknown_builtin.mlx`).

## `@compileError`

`@compileError("message")` unconditionally fails compilation with the given
message — the standard tool for a generic function to reject an
unsupported instantiation:

```mlx
// Tests the explicit compile-time failure builtin; @compileError must stop compilation with MLX-E5003.
```

(`tests/13_compile_error_builtin.mlx` — `MLX-E5003`)

## Stage-0 gaps in the comptime surface

A few builtins are *named* by the specification but their result value
schema is not yet normative. Stage 0 recognizes the name and rejects the
call with a structured diagnostic rather than inventing a private
representation (see `SPEC_CONFLICTS.md` for the full list):

- `@languageVersion` (result format undefined) — `tests/24_reflection_schema_gap.mlx`
- `@fence` and the other atomic builtins (argument/result shape undefined) — `tests/25_atomic_signature_gap.mlx`
- `@splat` and the other vector operation builtins (argument/result shape undefined) — `tests/26_vector_signature_gap.mlx`

Similarly, float-to-integer builtin lowering is a known Stage-0 limitation
that is documented, not silently miscompiled:

```mlx
// Documents the Stage-0 limitation for float-to-integer lowering; compilation must emit MLX-E9001.
```

(`tests/22_float_builtin_lowering.mlx`)
