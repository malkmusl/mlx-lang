# Comptime and generics

**Source tests:** `04_comptime.mlx`, `05_lir.mlx`, `12_unknown_builtin.mlx`,
`13_compile_error_builtin.mlx`, `24_reflection_schema_gap.mlx`,
`25_atomic_signature_gap.mlx`, `26_vector_signature_gap.mlx`,
`74_comptime_runtime_dependency.mlx`, `106_anytype_generic_runtime.mlx`,
`107_comptime_type_generic_runtime.mlx`, `108_comptime_value_generic_runtime.mlx`,
`109_comptime_argument_runtime_dependency.mlx`, `110_generic_dependent_type_range.mlx`,
`111_generic_type_return_runtime.mlx`, `112_comptime_if_pruning_runtime.mlx`,
`171_imported_generic_runtime.mlx`, `support/generic_identity.mlx`,
`386_generic_aggregate_runtime.mlx`, `397_generic_containers_runtime.mlx`,
`398_generic_imported_runtime.mlx`, `support/generic_stack.mlx`,
`401_generic_signature_types_runtime.mlx`, `402_generic_type_argument_runtime.mlx`,
`405_generic_type_argument_error.mlx`, `406_generic_local_alias_runtime.mlx`

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

## Generic aggregate types

A function returning `type` may return a `struct`, `union`, `enum` or
`@noncopy(struct)` written in its body. Each distinct list of comptime
arguments is one instance of it: `Box(u32)` named anywhere is the same
type, `Box(u8)` another. The function's parameters are in scope in the
aggregate's fields, constants and methods, so are the constants the body
declares before `return`:

```mlx
fn Box(comptime T: type) -> type {
    return struct {
        value: T
        count: usize

        const WIDTH: usize = @sizeOf(T)

        fn double(value: T) -> T { return value * 2 }

        pub fn init(value: T) -> Self { return Self.{ .value = value, .count = 1 } }
        pub fn get(self: Self) -> T { return self.value }
        pub fn bump(self: *Self) -> void { self.*.count += 1 }
        pub fn doubled(self: *Self) -> T { return double(self.*.value) }

        pub fn describe(self: *Self) -> u8 {
            if @isInteger(T) { return 1 } else { return @compileError("only integers are boxed here") }
        }

        pub fn as(self: *Self, comptime U: type) -> U { return @intCast(U, self.*.value) }
    }
}

const Pair = struct {
    small: Box(u8)
    wide: Box(u32)
}

fn sum(boxes: [2]Box(u8)) -> u8 { return boxes[0].get() + boxes[1].get() }

fn through(box: *Box(u32)) -> *Box(u32) {
    box.bump()
    return box
}

fn maybe(present: bool) -> ?Box(u8) {
    if present { return Box(u8).init(3) }
    return null
}

fn make(comptime T: type, value: T) -> Box(T) { return Box(T).init(value) }

fn main() -> u8 {
    const Wide = Box(u32)
    var a = Wide.init(20)
    a.bump()
    Box(u32).bump(&a)
    ...
}
```

(`tests/386_generic_aggregate_runtime.mlx`, abridged; the file holds the
checks)

The body of such a function is constants, `if`s whose condition is known
at compile time, and a `return` of a type; a runtime value as an argument
is rejected with MLX-E5001 (`tests/405_generic_type_argument_error.mlx`).
A `comptime` `if` inside a method (`if @isInteger(T) { ... } else {
@compileError(...) }`) is analyzed and generated for the branch taken
alone, as the `if` expression of the previous section is. Instances are
named for diagnostics by the call that made them, `Box(u32)`, and their
methods in the executable's symbol table as `Box(u32).get`.

An instance may be the element of another (`List(List(u8))`), the type of
a field in a struct the body of another generic declares (`const Entry =
struct { key: K, value: V }` and then `entries: List(Entry)`), and a
method may return `?V` or `?*V` and take `[]const T`
(`tests/397_generic_containers_runtime.mlx`, a list and a map over
`std.allocator`). A generic type declared in one module is instantiated by
the modules importing it, and a `comptime` integer parameter sizes an
array field (`generic.Stack(u8, 4)`, `tests/398_generic_imported_runtime.mlx`
with `tests/support/generic_stack.mlx`). Inside a function, `const Small =
Box(u8)` binds an instance to a local name
(`tests/406_generic_local_alias_runtime.mlx`).

A type written over a comptime type parameter resolves in a generic
*function* too, in its parameters, its result and its locals: `p: [*]T`,
`values: []const T`, `-> ?T`, `-> *T`, `var items: [*]T`
(`tests/401_generic_signature_types_runtime.mlx`). The type argument of a
call need not be a name: `leaf(@childType(T), a[0], b[0])`, a constant
bound to `@childType(T)`, `@typeOf(a[0])`, and locals declared `const x:
@childType(T) = a[at]` all name the type they resolve to, evaluated in the
caller's own instance (`tests/402_generic_type_argument_runtime.mlx`).

Not supported: a generic type naming itself through a call inside its own
body (`next: ?*List(T)`; write `?*Self`), and `var` declarations among an
aggregate's members.

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

Generic/`anytype` functions work the same way when imported from another
module — the comptime instantiation happens at the call site regardless of
which module the generic function was declared in:

```mlx
// tests/support/generic_identity.mlx
pub fn identity(value: anytype) -> anytype {
    return value
}

pub fn choose(comptime T: type, value: T) -> T {
    return value
}

pub fn fieldAt(value: anytype, comptime index: usize) -> anytype {
    return @field(value, index)
}

pub fn fieldName(value: anytype, comptime index: usize) -> []const u8 {
    return @fieldName(@typeOf(value), index)
}

pub fn isAggregate(value: anytype) -> bool {
    return @isStruct(@typeOf(value)) || @isTuple(@typeOf(value))
}
```

```mlx
const generic = @import("./support/generic_identity.mlx")

const Pair = struct {
    first: u8
    second: u8
}

pub fn main() -> u8 {
    const pair = Pair.{ .first = 2, .second = 8 }
    const name = generic.fieldName(pair, 1)
    if name[0] != 115 { return 1 }   // "second"[0] == 's' == 115
    if !generic.isAggregate(pair) { return 2 }
    return generic.identity(5) + generic.choose(u8, generic.fieldAt(pair, 1))
}
```

(`tests/171_imported_generic_runtime.mlx`, importing
`tests/support/generic_identity.mlx`; note `@field` also accepts a
comptime field *index*, not just a name string, and `@fieldName(T, index)`
goes the other way — index to name)

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
| `@field(value, "name" \| index)` | read a field by comptime name or index |
| `@fieldName(T, index)` | the name of the field at a comptime index |
| `@isInteger`/`@isFloat`/`@isPointer`/`@isSlice`/`@isArray`/`@isOptional`/`@isErrorUnion`/`@isStruct`/`@isEnum`/`@isUnion`/`@isTuple`/`@isCopyable(T)` | type-category predicates |
| `@intCast(T, value)` | range-checked integer conversion |
| `@intFromEnum(value)` / `@tagOf(value)` | integer/tag value of an enum or tagged-union instance |

```mlx
const a = 1
const b = @typeOf(a)
const c = @sizeOf(b)
```

(`tests/04_comptime.mlx`)

`@offsetOf` (like `@sizeOf`) is a compile-time constant for a type from
another module as well as a local one, wherever it is used — a call
argument, a local, a condition:

```mlx
const layout = @import("./support/layout_types.mlx")

if second("a slice before it", @offsetOf(layout.Tagged, "extent"), 0) != 16 { return 4 }
const imported = @offsetOf(layout.Tagged, "flags")
```

(`tests/250_offsetof_runtime.mlx`, with the aggregates in
`tests/support/layout_types.mlx`)

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
