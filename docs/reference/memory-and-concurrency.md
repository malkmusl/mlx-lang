# Memory model and concurrency

**Normative source:** `spec/00-language/memory-model.xml`
(`MlxMemoryModel`) and `spec/00-language/atomics-tls.xml`
(`MlxConcurrencyPrimitives`). **Related:** `SPEC_CONFLICTS.md` (open
normative gaps), `docs/guide/02-operators.md` (checked/wrapping/saturating
arithmetic), `docs/guide/09-unsafe-and-safety.md` (the `unsafe` boundary and
runtime safety checks), `docs/guide/06-ownership.md` (`@nocopy`/`@noncopy`,
`@move`, automatic drop).

This page's job is to be the single place that states, without hedging,
what Mlx guarantees about memory safety and concurrency today and what it
explicitly does not yet guarantee. Both halves matter equally here: per this
project's rule that "the implementation must never silently invent
behavior," the gaps below are not omissions to paper over — they're places
where the spec deliberately stops short and the compiler is required to
reject rather than improvise.

## What `spec/00-language/memory-model.xml` guarantees

The whole model is stated compactly enough to quote in full:

```xml
<ManualMemoryManagement>true</ManualMemoryManagement>
<GC>false</GC><ReferenceCounting implicit="false"/><BorrowChecker>false</BorrowChecker>
<AllocatorRule>Runtime heap allocation must be explicit through std.mem.Allocator, an explicitly allocator-owning object, or an explicitly named target primitive.</AllocatorRule>
<UndefinedBehavior>use-after-free; double-free unless allocator traps; invalid or misaligned pointer dereference; read of undefined storage; invalid type bit patterns; data race; foreign ABI violation</UndefinedBehavior>
<UndefinedValue>No initialization store is required; reading before initialization is undefined behavior. Safe builds may poison/trap.</UndefinedValue>
<Endianness targetDefined="true" initialX86_64="little"/>
<IntegerSignedRepresentation>two's complement</IntegerSignedRepresentation>
<Overflow safe="trap" comptime="compile error" releaseFast="undefined if safety disabled" wrapping="+% -% *%" saturating="+| -| *|"/>
<DivisionByZero safe="trap" comptime="compile error"/>
```

(`spec/00-language/memory-model.xml`)

### No GC, no implicit refcounting, no borrow checker

Three attributes rule out the three usual ways a systems-adjacent language
gives you memory safety without manual `free`: `<GC>false</GC>`,
`<ReferenceCounting implicit="false"/>`, `<BorrowChecker>false</BorrowChecker>`.
Mlx's actual safety mechanism is the one `docs/guide/06-ownership.md`
documents in depth — `@nocopy`/`@noncopy` wrapping plus compiler-tracked
"used exactly once" enforcement (move/copy/drop), with a residual *runtime*
flag inserted only where control flow isn't statically decidable. That's a
narrower guarantee than a borrow checker: it prevents a non-copyable value
from being used twice, not arbitrary aliasing or lifetime violations on raw
pointers. Anything reached through a raw pointer, rather than through a
tracked `@noncopy` value, is outside that mechanism entirely and governed by
the `UndefinedBehavior` list below instead.

### The allocator rule

`<AllocatorRule>` is one sentence but it's the thing that makes "manual
memory management" mean something concrete rather than "anything goes":
every runtime heap allocation must go through `std.mem.Allocator`, an object
that explicitly owns an allocator, or an explicitly named target primitive.
There is no implicit `malloc`-equivalent path. The Wayland transport layer's
buffering rule (`spec/06-wayland/transport.xml`) is a concrete instance of
this same rule applied to one subsystem — see
[`docs/reference/wayland.md`](wayland.md).

### Undefined behavior, itemized

The `<UndefinedBehavior>` list is the exhaustive-by-declaration set of ways
a program can leave defined behavior; nothing on this list gets a runtime
guarantee from the language itself:

- **use-after-free**
- **double-free** — "unless allocator traps": a specific allocator
  implementation is free to detect and trap a double-free (many debug
  allocators do), but that's an allocator-level guarantee, not a
  language-level one. The language does not promise every allocator catches
  this.
- **invalid or misaligned pointer dereference** — the runtime-checked half
  of this (in a safe build) is what `docs/guide/09-unsafe-and-safety.md`
  documents via `tests/18_invalid_pointer_alignment.mlx` and the
  `225`–`232` range (`see docs/guide/09-unsafe-and-safety.md`).
- **read of undefined storage**
- **invalid type bit patterns** — constructing a value whose bit pattern
  isn't a valid instance of its type (an out-of-range enum discriminant
  read as that enum type, for example).
- **data race** — defined precisely in `atomics-tls.xml`, see below.
- **foreign ABI violation** — violating the calling convention/layout
  contract of an `extern` boundary.

### Reading before initialization: the `undefined` boundary

`<UndefinedValue>` draws a specific line: "No initialization store is
required; reading before initialization is undefined behavior. Safe builds
may poison/trap." Two things follow from this wording that are easy to
misread:

1. Declaring a variable `= undefined` (or leaving an aggregate field
   unwritten before use) is **not itself** the undefined behavior — the
   language does not require an initializing store at declaration time.
   The undefined behavior is specifically *reading* the storage before a
   store has happened.
2. Taking the address of an `undefined`-initialized value, or writing to it
   before reading, is fine — there is no requirement that a value be
   "trap-poisoned" merely for existing in an uninitialized state.

`tests/206_undefined_aggregate_address_runtime.mlx` is a runtime (not
diagnostic) fixture that exercises exactly this boundary — it declares a
struct and a large array as `undefined`, immediately takes their address,
writes fields/elements, and only then reads them back:

```mlx
const Box = struct {
    value: usize
}

pub fn main() -> u8 {
    var box: Box = undefined
    box.value = 7
    if @intFromPtr(&box) == 0 || box.value != 7 { return 1 }

    var buffer: [65536]u8 = undefined
    buffer[0] = 11
    buffer[65535] = 13
    if @intFromPtr(&buffer) == 0 || buffer[0] != 11 || buffer[65535] != 13 { return 2 }
    return 13
}
```

(`tests/206_undefined_aggregate_address_runtime.mlx`)

This test exits `13` (success) — it is not a diagnostic-triggering fixture.
What it demonstrates is the *allowed* side of the `UndefinedValue` boundary:
`&box` and `&buffer` are well-defined addresses immediately after an
`undefined` declaration (the address of storage is always defined; it's the
stored *value* that isn't), and writing before reading never touches
undefined behavior at all, including for a 64 KiB stack array. The
compile-time guard against the *forbidden* side of this boundary — reading
a value that provably hasn't been written yet — is `MLX-E6004`
(`UseOfUninitializedValue`), listed in
`docs/guide/11-diagnostics.md`'s Copyability/`@move`/initialization-state
table; that code doesn't yet have an example fixture in the guide's scope
(see [`docs/reference/diagnostics-selfhost.md`](diagnostics-selfhost.md) for
the fixtures this page's sibling documents).

### Integers: representation, overflow, division

- **Endianness** is target-defined, with `x86_64`'s initial value fixed as
  little-endian — the attribute name (`initialX86_64`) signals this is the
  first target's value, not a promise that every future target is little
  endian.
- **Signed integers** are two's complement, unconditionally.
- **Overflow** has four distinct behaviors depending on context, all fixed
  by one line: checked arithmetic (`+`, `-`, `*`) traps at runtime in a safe
  build and is a compile error at comptime; under `releaseFast` it's
  undefined behavior if safety is disabled; wrapping arithmetic uses the
  `+%`/`-%`/`*%` operators; saturating arithmetic uses `+|`/`-|`/`*|`. The
  worked examples for all four are in `docs/guide/02-operators.md`, grounded
  in `tests/89_checked_overflow_comptime.mlx` and
  `tests/93_checked_overflow_runtime.mlx` (see
  [docs/guide/02-operators.md](../guide/02-operators.md)).
- **Division by zero** follows the same safe-trap/comptime-compile-error
  split as checked overflow.

## Atomics, and thread-local storage as a documented gap

`spec/00-language/atomics-tls.xml` names six atomic builtins and six memory
orders, and since the `<CallShapes>` element it also fixes their call
shapes, result types, the representation of a read-modify-write operation
and the valid order combinations. The canonical compiler implements them;
`threadlocal` is still the gap described further down.

### The atomic builtins

```mlx
var word: u64 = 5
const seen = @atomicLoad(u64, &word, seq_cst)              // 5
@atomicStore(u64, &word, 9, release)
const old = @atomicRmw(u64, &word, add, 3, seq_cst)        // 9; word is 12
const result = @cmpxchgStrong(u64, &word, 12, 1, acq_rel, acquire)
// result.0: true (it swapped), result.1: 12 (the value seen); word is 1
@fence(seq_cst)
```

- `@atomicLoad(T, pointer, order) -> T`
- `@atomicStore(T, pointer, value, order) -> void`
- `@atomicRmw(T, pointer, op, value, order) -> T`: the value before the
  operation; `op` is one of `xchg add sub and nand or xor max min`, `max`
  and `min` by T's signedness
- `@cmpxchgStrong(T, pointer, expected, new, success, failure) -> (bool, T)`
  and `@cmpxchgWeak(...)`: whether it swapped (the memory held `expected`,
  and holds `new` now) and the value seen; the weak form may fail without
  cause, so it belongs in a loop
- `@fence(order) -> void`

`T` is an integer of 1, 2, 4 or 8 bytes, a `bool`, an enum or a pointer, and
`pointer` points to a `T`. The orders and the operation are bare names, as
the spec spells them (`unordered monotonic acquire release acq_rel
seq_cst`), not values. The compiler rejects the combinations C11 rejects:
a load that would release, a store that would acquire, an unordered
read-modify-write, a fence below acquire, and a compare-exchange whose
failure order releases or exceeds its success order
(`compiler/selfhost/sema/builtins/atomics.mlx` holds the rules and the
messages).

On x86_64 an aligned load or store is already atomic and ordered: a load
is a `mov`, a `seq_cst` store an `xchg`, `xchg` and `add`/`sub` are `xchg`
and `lock xadd`, the other operations a `lock cmpxchg` loop, the
compare-exchange `lock cmpxchg`, and `@fence(seq_cst)` an `mfence` (the
other fences order the compiler alone, which the LIR's barriers do). On
aarch64 the acquiring loads and releasing stores are `LDAR` and `STLR`, the
read-modify-writes and the compare-exchange `LDAXR`/`STLXR` loops, and a
fence `DMB ISH` (`ISHLD` for acquire). `tests/295_atomics_runtime.mlx`
exercises every operation at every width and signedness and has four
threads count on one word through `@atomicRmw` and through a lock made of
`@cmpxchgWeak` and `@atomicStore`.

### What `atomics-tls.xml` fixes for `threadlocal`

```xml
<TLS keyword="threadlocal" scope="module/global variable only" initializer="comptime evaluable" heapAllocation="false"/>
<Volatile>Volatile controls observable memory access and is not inter-thread synchronization.</Volatile>
<DataRace>Non-atomic concurrent conflicting access with at least one write is undefined behavior.</DataRace>
```

(`spec/00-language/atomics-tls.xml`)

The spec fixes the `threadlocal` keyword and that it is only legal on a
module/global variable, with a comptime-evaluable initializer and no heap
allocation involved; that `volatile` is about observable access, not
synchronization; and the data-race definition backing the `data race`
entry in the memory model's `UndefinedBehavior` list above.

### What it does not fix, per `SPEC_CONFLICTS.md`

"Thread-local storage ABI":

> `spec/00-language/atomics-tls.xml` fixes the source spelling, declaration
> scope, initializer requirement, and absence of heap allocation for
> `threadlocal`, but does not define the executable TLS model, TLS
> relocation model, per-thread initialization protocol, or how a
> freestanding executable obtains its thread pointer. The compiler
> preserves and validates the declaration marker, and the canonical
> compiler reports MLX-E9001 before object emission rather than silently
> treating TLS as ordinary global storage. It cannot emit a private TLS ABI
> until that contract is normative.

(`SPEC_CONFLICTS.md`)

The shape of that gap: the spec fixes *surface* (keyword, allowed scope,
initializer) but not the *codegen contract* (TLS model, relocations, thread
pointer), and the compiler's response is to recognize the construct,
validate what can be validated, then refuse to emit an invented lowering,
reporting `MLX-E9001` (`UnsupportedInstructionLowering`) rather than
treating `threadlocal` as an ordinary global.

### The fixtures

`tests/186_threadlocal_abi_gap.mlx` documents the TLS gap: a `threadlocal`
declaration using otherwise-ordinary syntax, which must be rejected because
there is no ABI to lower it to:

```mlx
// The TLS ABI is not normatively specified yet. Compilation must reject this
// declaration instead of silently treating it as ordinary global storage.
threadlocal var counter: usize = 0

pub fn main() u8 {
    return @intCast(u8, counter)
}
```

(`tests/186_threadlocal_abi_gap.mlx`) — checked against `MLX-E9001` via
`tests/run_error.sh`.

`tests/25_atomic_signature_gap.mlx` (`@fence()` with no arguments) is Stage
0's fixture from before the call shapes were fixed: mlx0 still rejects every
atomic with `MLX-E9001`; the canonical compiler rejects this one for its
arity and accepts `@fence(seq_cst)`. `tests/182_selfhost_atomic_encoder_runtime.mlx`
checks one encoder primitive (`lock xadd`) at the byte level.

## Summary: guaranteed vs. open today

| Area | Status |
| --- | --- |
| Manual allocation via `std.mem.Allocator` | Specified and enforced |
| No GC / no implicit refcounting / no borrow checker | Specified (Mlx uses `@nocopy`/`@noncopy` + move-tracking instead — see `docs/guide/06-ownership.md`) |
| UB list (use-after-free, double-free, misaligned deref, undefined read, invalid bit pattern, data race, foreign ABI violation) | Specified; some are runtime-checked in safe builds (see `docs/guide/09-unsafe-and-safety.md`), the rest are unchecked by the language |
| Checked/wrapping/saturating overflow, division by zero | Specified and enforced (`docs/guide/02-operators.md`) |
| Two's-complement signed integers | Specified |
| `threadlocal` keyword, scope, initializer rule | Specified at the source level; **no executable TLS ABI** — the compiler rejects any use with `MLX-E9001` |
| `@atomicLoad`/`@atomicStore`/`@atomicRmw`/`@cmpxchgWeak`/`@cmpxchgStrong`/`@fence`, the six memory orders | Specified (names, call shapes, result types, operations, order rules) and implemented on x86_64 and aarch64 (`tests/295_atomics_runtime.mlx`); Stage 0 (mlx0) still rejects them |
| Data race definition | Specified (non-atomic concurrent conflicting access with a write is UB); the atomics are the way to share mutable state across threads |

Of the three rows, `threadlocal` is the one still closed off: per
`SPEC_CONFLICTS.md`'s framing, the compiler "cannot emit a private TLS ABI
until that contract is normative." Shared mutable state across threads goes
through the atomics (and the locks built on them) meanwhile.
