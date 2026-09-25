# Types and aggregates

**Source tests:** `15_aggregate_builtins.mlx`, `16_enum_union_builtins.mlx`,
`20_vector_type_builtin.mlx`, `23_tagged_union_layout.mlx`,
`60_array_layout_runtime.mlx`, `65_index_slice_runtime.mlx`,
`67_optional_unwrap_runtime.mlx`, `85_struct_literal_runtime.mlx`,
`86_struct_literal_missing_field.mlx`, `87_field_builtin_runtime.mlx`,
`88_lvalue_assignment_runtime.mlx`, `95_enum_value_runtime.mlx`,
`97_tagged_union_runtime.mlx`, `98_nonexhaustive_enum_requires_else.mlx`,
`99_enum_duplicate_value.mlx`, `100_union_explicit_tag_mismatch.mlx`,
`104_optional_pointer_null_runtime.mlx`, `174_aggregate_functions_runtime.mlx`,
`176_pointer_struct_fields_runtime.mlx`, `212_aggregate_index_runtime.mlx`

## Structs

A struct is a list of named, typed fields. A struct literal is
`TypeName.{ .field = value, ... }`; every field must be initialized:

```mlx
const Point = struct {
    x: u8
    y: u8
}

fn main() u8 {
    const point: Point = Point.{
        .x = 19,
        .y = 23,
    }
    return point.x + point.y
}
```

(`tests/85_struct_literal_runtime.mlx` → 42)

Omitting a field in the literal is a compile-time error
(`tests/86_struct_literal_missing_field.mlx`).

A field can also be read through the `@field(value, "name")` builtin instead
of `value.name` — useful when the field name itself is comptime-computed
(see [Comptime and generics](08-comptime-and-generics.md) for a generic
example that does exactly that):

```mlx
const Point = struct {
    x: u8
    y: u8
}

fn main() u8 {
    const point: Point = Point.{ .x = 19, .y = 23 }
    return @field(point, "y")
}
```

(`tests/87_field_builtin_runtime.mlx`)

Any lvalue — a struct field, an array element, or a pointer dereference —
supports compound assignment directly, not just plain variables:

```mlx
const Pair = struct {
    left: u8
    right: u8
}

fn main() u8 {
    var pair: Pair = Pair.{ .left = 3, .right = 4 }
    pair.left += 2
    var values: [2]u8 = [2]u8{ 5, 6 }
    values[1] *= 2
    var extra: u8 = 7
    const pointer = &extra
    pointer.* += 1
    return pair.left + values[1] + pointer.*
}
```

(`tests/88_lvalue_assignment_runtime.mlx`)

Struct layout/reflection builtins:

```mlx
const Header = struct {
    byte: u8
    word: u32
}

pub fn main() u8 {
    const layout = @fieldCount(Header) + @offsetOf(Header, "word")
        + @sizeOf(Header) + @sizeOf(@fieldType(Header, "word"))
    const traits = @hasField(Header, "byte") + @isStruct(Header) + @isCopyable(Header)
    return layout + traits
}
```

(`tests/15_aggregate_builtins.mlx`)

## Methods

A struct can declare functions in its body. `Self` refers to the enclosing
type. A method that takes `self` (by value or by pointer, `self: Self` /
`self: *Self`) is called on an instance with `.`; a function that doesn't
take `self` is called on the type itself, like an associated/"static"
function:

```mlx
pub const Parser = struct {
    value: u8

    fn rejectAggregateDeclaration(parser: *anyopaque) -> usize {
        return 17
    }

    pub fn current(self: Self) -> u8 {
        return self.value
    }
}

fn main() -> u8 {
    unsafe {
        if Parser.rejectAggregateDeclaration(@ptrFromInt(*anyopaque, 1)) != 17 { return 1 }
    }
    const parser = Parser.{ .value = 13 }
    return parser.current()
}
```

(`tests/174_aggregate_functions_runtime.mlx`; see
[Modules](10-modules.md) for the same pattern used with a struct imported
from another file)

A pointer-receiver method (`self: *Self`) can mutate the instance it's
called on, and an associated function can return `*Self` — for example, to
hand back a heap-allocated instance (see
[Standard library](../reference/stdlib.md) for the allocator interface used
here):

```mlx
const page_allocator = @import("../std/bootstrap/page_allocator.mlx")

const Box = struct {
    value: usize

    fn set(box: *Self, value: usize) -> void {
        box.*.value = value
    }

    fn create() -> *Self {
        const allocator = page_allocator.init()
        const memory = allocator.allocFn(allocator.context, @sizeOf(Self), @alignOf(Self)).?
        unsafe {
            const box = @ptrCast(*Self, memory)
            box.*.value = 13
            return box
        }
    }
}

pub fn main() -> u8 {
    var local = Box.{ .value = 1 }
    Box.set(&local, 7)
    if local.value != 7 { return 1 }
    const allocated = Box.create()
    if allocated.*.value != 13 { return 2 }
    return 13
}
```

(`tests/176_pointer_struct_fields_runtime.mlx`)

## Enums

An enum declares a mandatory backing integer type. Tags default to
sequential values starting at 0, but any tag can set an explicit value:

```mlx
const Mode = enum(u8) {
    idle,
    active = 7,
    done
}

fn main() u8 {
    return @intFromEnum(Mode.done)   // 8: continues after the explicit 7
}
```

(`tests/95_enum_value_runtime.mlx`)

Two tags cannot share the same explicit value
(`tests/99_enum_duplicate_value.mlx`).

By default an enum is exhaustive: a `match` over it is complete once every
tag is listed (see [Control flow](03-control-flow.md)). Marking it
`nonexhaustive` always requires an `else` arm in a `match`, since new tags
may be added later without an ABI/source break for existing matches:

```mlx
const Mode = enum(u8, nonexhaustive) {
    idle
}
```

(`tests/98_nonexhaustive_enum_requires_else.mlx`)

An enum is stored and loaded like its backing integer, so the members of an
enum with a signed backing type may be negative and keep their value through
struct fields, copies, parameters and `@enumFromInt` — including values a
non-exhaustive enum does not name (Vulkan's `VkResult` error codes are
negative `i32` values):

```mlx
const Result = enum(i32, nonexhaustive) { success = 0, timeout = 2, errorDeviceLost = -4, errorUnknown = -13, }

holder.result = Result.errorDeviceLost
if holder.result != Result.errorDeviceLost { return 1 }
```

(`tests/252_negative_enum_runtime.mlx`)

## Unions

An untagged `union` has no hidden discriminator — the caller is responsible
for knowing which field is active:

```mlx
const Value = union {
    byte: u8
    word: u32
}
```

(`tests/16_enum_union_builtins.mlx`)

A tagged union, `union(EnumOrEnumLiteral)`, carries an enum discriminator and
requires each payload-carrying variant to match a declared enum tag:

```mlx
const Event = union(enum(u8)) {
    none,
    number: u8
}

fn main() u8 {
    const event = Event.{ .number = 42 }
    const tag = @tagOf(event)
    return match event {
        .none => 1,
        .number => event.number + tag
    }
}
```

(`tests/97_tagged_union_runtime.mlx`)

A tagged union can also be declared against a separately-named enum type;
every payload field must correspond to a tag that the enum actually declares
— a payload naming a tag the enum doesn't have is rejected:

```mlx
const Tag = enum(u8) {
    first,
    second
}

const Broken = union(Tag) {
    first: u8
    // `second` has no matching payload declared, or an extra field names a
    // tag Tag doesn't have — both are rejected
}
```

(`tests/100_union_explicit_tag_mismatch.mlx`)

Reflection over a tagged union's layout works the same as for structs and
enums:

```mlx
const Event = union(enum(u8)) {
    none
    data: u32
}

pub fn main() u8 {
    return @sizeOf(Event) + @alignOf(Event) + @offsetOf(Event, "data") + @fieldCount(Event)
}
```

(`tests/23_tagged_union_layout.mlx`)

## Arrays and slices

A fixed-size array literal is `[_]T{...}` (size inferred) or `[N]T{...}`
(size explicit); its type is `[N]T`. A slice, `[]T`, is a pointer+length
view and supports Python-style range indexing:

```mlx
pub fn main() u8 {
    const items = [_]u8{5, 7, 9}
    const tail = items[1..]   // slice: [7, 9]
    return tail[1]            // 9
}
```

(`tests/65_index_slice_runtime.mlx`)

```mlx
noinline fn readAt(index: usize) -> u8 {
    var values: [2]u8 = [2]u8{ 17, 29 }
    return values[index]
}
```

(`tests/225_runtime_bounds_safety.mlx`/`tests/226_runtime_bounds_in_range.mlx`
— see [Unsafe and safety](09-unsafe-and-safety.md) for what happens on an
out-of-range index)

Array element layout follows normal row-major/sequential order
(`tests/60_array_layout_runtime.mlx`), which holds just as well for a
heap-allocated run of structs addressed through a raw pointer: indexing a
`[*]Entry` and then a field on the result addresses exactly the expected
byte offset (`entries[0]` and `entries[1]` sit `@sizeOf(Entry)` bytes apart):

```mlx
const page_allocator = @import("../std/bootstrap/page_allocator.mlx")

const Entry = struct {
    value: usize
    marker: u8
}

pub fn main() -> u8 {
    const allocator = page_allocator.init()
    unsafe {
        const memory = allocator.allocFn(allocator.context, 2 * @sizeOf(Entry), @alignOf(Entry)).?
        const entries = @ptrCast([*]Entry, memory)
        entries[0].value = 11
        entries[0].marker = 3
        entries[1].value = 29
        entries[1].marker = 13

        if entries[0].value != 11 || entries[0].marker != 3 { return 1 }
        if entries[1].value != 29 { return 2 }
        return entries[1].marker
    }
}
```

(`tests/212_aggregate_index_runtime.mlx`; see
[Unsafe and safety](09-unsafe-and-safety.md) for the `unsafe {}` boundary
this raw-pointer indexing requires)

## Optionals

`?T` is an optional `T`. `?*T` (an optional pointer) has a null-niche layout
— no extra tag byte is needed, since a null pointer bit pattern is
unambiguous. `.?` unwraps an optional; unwrapping a present value yields its
payload, and unwrapping `null` traps at runtime (see
[Unsafe and safety](09-unsafe-and-safety.md)).

```mlx
pub fn main() u8 {
    const value: ?u8 = 13
    return value.?
}
```

(`tests/67_optional_unwrap_runtime.mlx`)

```mlx
fn main() u8 {
    const value: ?*u8 = null
    if value == null {
        return 7
    } else {
        return 1
    }
}
```

(`tests/104_optional_pointer_null_runtime.mlx`)

> Only the `?*T` layout is normative so far; the general non-pointer `?T`
> ABI/reflection layout is an open gap (see `SPEC_CONFLICTS.md`,
> "Non-pointer optional layout").

## Pointers

`*T` is a pointer to `T`; `&value` takes a pointer, `pointer.*` dereferences
it. Pointer type modifiers include alignment (`*align(N) T`) and `volatile`.
See [Unsafe and safety](09-unsafe-and-safety.md) for the rules around
constructing raw pointers.

A single-item pointer converts implicitly to `*anyopaque`, C's `void *`
(and to `*const anyopaque` or `?*const anyopaque`), keeping its address; a
`*const T` only converts to a const opaque pointer. This is what C
extension chains such as Vulkan's `pNext` need
(`info.pNext = &external`). Going back takes an explicit `@ptrCast`.
(`tests/259_anyopaque_coercion_runtime.mlx`)

## Vectors

`@Vector(len, ElemType)` (or the equivalent vector type syntax) constructs a
fixed-width SIMD vector type; its size/layout are reflectable the same way as
other types:

```mlx
// Tests vector type construction and layout reflection; main returns the vector size plus its bit size.
```

(`tests/20_vector_type_builtin.mlx`)

> Vector *operation* builtins (`@splat`, `@shuffle`, `@reduce`, `@select`)
> name is normative but their call shapes are not yet — see
> `SPEC_CONFLICTS.md`, "Vector builtin call shapes".
