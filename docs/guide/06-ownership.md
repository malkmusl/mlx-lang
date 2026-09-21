# Ownership: `@nocopy`, `@move`, and automatic drop

**Source tests:** `02_nocopy_error.mlx`, `03_move.mlx`, `21_redundant_move_warning.mlx`,
`27_nocopy_representation_escape.mlx`, `213_noncopy_alias.mlx`,
`214_autodrop_runtime.mlx`, `215_autodrop_move_runtime.mlx`,
`216_autodrop_lifo_runtime.mlx`, `217_autodrop_signature_error.mlx`,
`218_autodrop_conditional_move_runtime.mlx`, `219_autodrop_reinitialize_runtime.mlx`,
`220_autodrop_parameter_runtime.mlx`, `221_autodrop_return_transfer_runtime.mlx`,
`222_autodrop_loop_exit_runtime.mlx`, `223_noncopy_local_copy_error.mlx`,
`224_noncopy_representation_copy_error.mlx`

Mlx has no garbage collector, reference counting, or implicit destructors.
Instead, it tracks ownership of values whose type is wrapped with
`@nocopy`/`@noncopy`, and enforces at compile time (with a residual runtime
flag where control flow is not statically decidable) that such a value is
used — copied, moved, or dropped — in exactly one way.

## `@nocopy(T)` / `@noncopy(T)`

`@nocopy(TypeHead)` makes a type non-copyable. `@noncopy` is the canonical
spelling; `@nocopy` remains an accepted alias:

```mlx
// @noncopy is the canonical ownership spelling; @nocopy remains compatible.
const Owned = @noncopy(struct) {
    value: u8
}
```

(`tests/213_noncopy_alias.mlx`)

Both wrap a "type head" — a primitive (`@nocopy(u8)`), or an aggregate
(`@nocopy(struct) { ... }`, `@nocopy(enum(u8)) { ... }`).

Copying a non-copyable value — including binding it to a second variable, or
passing it by value without `@move` — is rejected:

```mlx
const OwnedByte = @nocopy(u8)
var file: OwnedByte = @intCast(OwnedByte, 1)
var file2 = file // MLX-E6002: Cannot copy a non-copyable type
```

(`tests/02_nocopy_error.mlx`)

The same rule holds for a local aggregate value and for assigning it away
under its wrapped representation type — ownership can't be silently
discarded by "casting it away":

```mlx
const OwnedCounter = @noncopy(struct) {
    counter: *u8
    fn deinit(self: *Self) -> void { self.*.counter.* += 1 }
}

fn main() -> u8 {
    var drops: u8 = 0
    const source = OwnedCounter.{ .counter = &drops }
    const copy = source   // rejected: `source` can only be moved, not copied
    return drops
}
```

(`tests/223_noncopy_local_copy_error.mlx`)

```mlx
const OwnedByte = @noncopy(u8)

fn main() -> u8 {
    const owned: OwnedByte = 1
    const escaped: u8 = owned   // rejected: strips ownership metadata
    return escaped
}
```

(`tests/224_noncopy_representation_copy_error.mlx`; see also
`tests/27_nocopy_representation_escape.mlx`)

## `@move(value)`

`@move(value)` explicitly transfers a tracked value: the destination becomes
the owner, and the source binding is invalidated. Using the source after a
move is a compile-time error:

```mlx
const OwnedByte = @nocopy(u8)
var file: OwnedByte = @intCast(OwnedByte, 1)
var file2 = @move(file)
var file3 = file // MLX-E6001: Use of moved value
```

(`tests/03_move.mlx`)

`@move` also works on ordinary copyable values, where it is legal but
redundant (the compiler warns rather than errors):

```mlx
// Tests an explicit move of a copyable value by moving value into moved and returning 1.
```

(`tests/21_redundant_move_warning.mlx`)

## Automatic drop (`deinit`)

A `@noncopy` aggregate may declare a `fn deinit(self: *Self) -> void` method.
The compiler inserts a call to it automatically at the end of the owning
binding's scope, unless ownership was moved away first — there is no
implicit reference counting, so this is a purely lexical, statically-tracked
obligation.

```mlx
// A non-copyable local with a valid deinitializer is dropped at block exit.
const OwnedCounter = @noncopy(struct) {
    counter: *u8

    fn deinit(self: *Self) -> void {
        self.*.counter.* += 1
    }
}

fn main() -> u8 {
    var drops: u8 = 0
    {
        const owned = OwnedCounter.{ .counter = &drops }
    }   // `owned` goes out of scope here: deinit runs, drops becomes 1
    return drops
}
```

(`tests/214_autodrop_runtime.mlx` → 1)

`deinit` must take `self: *Self` and return `void`; any other signature is
rejected:

```mlx
const Broken = @noncopy(struct) {
    value: u8
    fn deinit(self: Self) -> u8 {   // MLX-E6006: wrong signature
        return self.value
    }
}
```

(`tests/217_autodrop_signature_error.mlx`)

### Moving cancels the obligation

Moving a value out of a binding transfers its pending `deinit` obligation to
the new owner — the old binding no longer runs `deinit`:

```mlx
fn main() -> u8 {
    var drops: u8 = 0
    {
        const source = OwnedCounter.{ .counter = &drops }
        const destination = @move(source)
    }   // only `destination` is dropped, once
    return drops   // 1, not 2
}
```

(`tests/215_autodrop_move_runtime.mlx`)

### LIFO ordering

Multiple pending drops in the same scope run in reverse declaration order
(last declared, first dropped) — the same lexical LIFO order as `defer`:

```mlx
fn main() -> u8 {
    var result: u8 = 0
    {
        const first = OwnedDigit.{ .result = &result, .digit = 1 }
        const second = OwnedDigit.{ .result = &result, .digit = 2 }
    }   // second.deinit() runs, then first.deinit()
    return result   // 21: second's digit then first's digit
}
```

(`tests/216_autodrop_lifo_runtime.mlx`)

### Conditional moves stay exactly-once

When whether a value is moved depends on runtime control flow, the compiler
maintains a hidden runtime "is this still owned here" flag so the drop still
runs exactly once — never zero times (a leak) and never twice (a
double-free):

```mlx
fn conditionalMove(counter: *u8, take: bool) -> void {
    const source = OwnedCounter.{ .counter = counter }
    if take {
        const destination = @move(source)
    }
    // if `take` was false, `source` is still owned here and drops normally;
    // if `take` was true, ownership (and the drop obligation) moved to
    // `destination`, which drops at the end of the `if` block instead
}
```

(`tests/218_autodrop_conditional_move_runtime.mlx`)

### Reinitialization restores the obligation

Assigning a brand-new value into a `var` binding that was previously moved
from restores its drop obligation:

```mlx
fn main() -> u8 {
    var drops: u8 = 0
    {
        var source = OwnedCounter.{ .counter = &drops }
        const destination = @move(source)   // source's obligation moves away
        source = OwnedCounter.{ .counter = &drops }   // source owns again
    }   // both destination and the reinitialized source drop: drops == 2
    return drops
}
```

(`tests/219_autodrop_reinitialize_runtime.mlx`)

### Parameters and returns

A moved-in by-value parameter owns its drop obligation for the duration of
the callee:

```mlx
fn consume(value: OwnedCounter) -> u8 {
    return 7
}   // `value` drops here, at the end of consume's body

fn main() -> u8 {
    var drops: u8 = 0
    const source = OwnedCounter.{ .counter = &drops }
    const ignored = consume(@move(source))
    return drops   // 1
}
```

(`tests/220_autodrop_parameter_runtime.mlx`)

Returning a value through `@move` transfers the obligation from the callee's
local storage to the caller's:

```mlx
fn create(counter: *u8) -> OwnedCounter {
    const local = OwnedCounter.{ .counter = counter }
    return @move(local)   // no drop here; ownership moves to the caller
}

fn main() -> u8 {
    var drops: u8 = 0
    {
        const received = create(&drops)
    }   // `received` drops here
    return drops   // 1
}
```

(`tests/221_autodrop_return_transfer_runtime.mlx`)

### Loop exits run pending cleanups

Both `continue` and `break` run the automatic cleanups owed by the scope
they exit, exactly like leaving that scope normally:

```mlx
fn main() -> u8 {
    var drops: u8 = 0
    var index: u8 = 0
    while index < 3 {
        const iteration = OwnedCounter.{ .counter = &drops }
        index += 1
        if index == 1 { continue }   // `iteration` still drops
        if index == 2 { break }      // `iteration` still drops
    }
    return drops
}
```

(`tests/222_autodrop_loop_exit_runtime.mlx`)
