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

## Atomics and thread-local storage: a documented Stage-0 gap

Unlike the memory model above — which is fully specified and the compiler
enforces it — `spec/00-language/atomics-tls.xml` names a surface without
fully specifying it, and `SPEC_CONFLICTS.md` documents the resulting gap
explicitly. This is the most important thing to be honest about on this
page: **the atomic builtins and `threadlocal` exist in the grammar and are
recognized by name, but neither has a usable call shape or ABI yet, and the
compiler is required to reject any program that tries to use them rather
than inventing one.**

### What `atomics-tls.xml` fixes

```xml
<TLS keyword="threadlocal" scope="module/global variable only" initializer="comptime evaluable" heapAllocation="false"/>
<Atomics builtins="@atomicLoad @atomicStore @atomicRmw @cmpxchgWeak @cmpxchgStrong @fence"/>
<Orders>unordered monotonic acquire release acq_rel seq_cst</Orders>
<Validation>Invalid order/operation combinations are compile errors where statically known.</Validation>
<Volatile>Volatile controls observable memory access and is not inter-thread synchronization.</Volatile>
<DataRace>Non-atomic concurrent conflicting access with at least one write is undefined behavior.</DataRace>
```

(`spec/00-language/atomics-tls.xml`)

So the spec fixes: the `threadlocal` keyword and that it's only legal on a
module/global variable, with a comptime-evaluable initializer and no heap
allocation involved; the six atomic builtin names; the six memory orders
(`unordered` through `seq_cst`, the C11/LLVM-style order names); that
invalid order/operation combinations must be rejected at compile time when
statically knowable; that `volatile` is about observable access, not
synchronization; and the data-race definition — "non-atomic concurrent
conflicting access with at least one write is undefined behavior" is the
formal statement backing the `data race` entry in the memory model's
`UndefinedBehavior` list above.

### What it does not fix, per `SPEC_CONFLICTS.md`

`SPEC_CONFLICTS.md`'s "Atomic builtin call shapes" entry:

> `spec/00-language/atomics-tls.xml` names `@atomicLoad`, `@atomicStore`,
> `@atomicRmw`, `@cmpxchgWeak`, `@cmpxchgStrong`, and `@fence`, and defines
> the available memory orders. It does not define argument order, result
> types, the representation of an RMW operation, or the valid order
> combinations per builtin. Stage 0 and the canonical compiler recognize
> every name and report MLX-E9001; neither lowers an invented calling
> convention.

And "Thread-local storage ABI":

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

In both cases the shape of the gap is the same: the spec fixes *surface*
(names, keywords, allowed orders, declaration constraints) but not *codegen
contract* (call shape, RMW representation, TLS relocation model, thread
pointer acquisition), and Stage 0's response to that gap is uniform —
recognize the construct, validate what can be statically validated, then
refuse to emit an invented lowering, reporting `MLX-E9001`
(`UnsupportedInstructionLowering`) rather than silently treating either
construct as something it isn't (e.g. treating `threadlocal` as an ordinary
global, or picking an arbitrary argument order for `@atomicRmw`).

### The three fixtures

`tests/25_atomic_signature_gap.mlx` documents the atomic-builtin half of
the gap by calling the simplest of the six builtins, `@fence`, with no
arguments:

```mlx
// Documents the unsupported atomic builtin signature by calling @fence; compilation must be rejected.
pub fn main() u8 {
    @fence()
    return 0
}
```

(`tests/25_atomic_signature_gap.mlx`) — checked against `MLX-E9001` via
`tests/run_error.sh`.

`tests/186_threadlocal_abi_gap.mlx` documents the TLS half the same way —
a `threadlocal` declaration using otherwise-ordinary syntax, which must
still be rejected because there's no ABI to lower it to:

```mlx
// The TLS ABI is not normatively specified yet. Compilation must reject this
// declaration instead of silently treating it as ordinary global storage.
threadlocal var counter: usize = 0

pub fn main() u8 {
    return @intCast(u8, counter)
}
```

(`tests/186_threadlocal_abi_gap.mlx`) — also `MLX-E9001`.

`tests/182_selfhost_atomic_encoder_runtime.mlx` is different in kind from
the two above, and worth being precise about rather than assuming it closes
the gap: it does not exercise the language-level `@atomicRmw` builtin at
all. It calls directly into the self-hosted compiler's own x86_64 machine
code encoder:

```mlx
const page_allocator = @import("../std/bootstrap/page_allocator.mlx")
const encoder = @import("../compiler/selfhost/backend/x86_64/encoder.mlx")
const target = @import("../compiler/selfhost/backend/x86_64/target.mlx")

pub fn main() -> u8 {
    const allocator = page_allocator.init()
    var value = encoder.init(allocator)
    encoder.emitAtomicXaddPointer(&value, target.Register.r14, target.Register.rax)
    if encoder.pos(&value) != 5 { return 1 }
    if value.buf.*.items[0] != 240 || value.buf.*.items[1] != 73 { return 2 }
    if value.buf.*.items[2] != 15 || value.buf.*.items[3] != 193 || value.buf.*.items[4] != 6 { return 3 }
    encoder.deinit(&value)
    return 13
}
```

(`tests/182_selfhost_atomic_encoder_runtime.mlx`)

It asserts that `encoder.emitAtomicXaddPointer` produces a specific 5-byte
instruction encoding (`F0 49 0F C1 06`, i.e. a `lock`-prefixed `xadd`
against a REX.WB-encoded register pair) at the machine-code level. This is
compiler-internals testing of one encoder primitive the self-hosted
backend's code generator has available to it — for the compiler's own
internal use in whatever lowering it eventually performs for atomic
operations — not a demonstration of a working `@atomicRmw` builtin at the
language level. The distinction matters for this page's "never invent
behavior" framing: the backend having an atomic-instruction *encoder*
primitive does not imply the language-level atomic builtins have a defined
call shape yet, and `tests/25_atomic_signature_gap.mlx` (which still rejects
`@fence()` with `MLX-E9001`) confirms they don't. What this test shows is
that when the call-shape gap in `SPEC_CONFLICTS.md` does get closed, the
backend already has at least one of the low-level encoding primitives
(`lock xadd`) a real lowering would need.

## Summary: guaranteed vs. open today

| Area | Status |
| --- | --- |
| Manual allocation via `std.mem.Allocator` | Specified and enforced |
| No GC / no implicit refcounting / no borrow checker | Specified (Mlx uses `@nocopy`/`@noncopy` + move-tracking instead — see `docs/guide/06-ownership.md`) |
| UB list (use-after-free, double-free, misaligned deref, undefined read, invalid bit pattern, data race, foreign ABI violation) | Specified; some are runtime-checked in safe builds (see `docs/guide/09-unsafe-and-safety.md`), the rest are unchecked by the language |
| Checked/wrapping/saturating overflow, division by zero | Specified and enforced (`docs/guide/02-operators.md`) |
| Two's-complement signed integers | Specified |
| `threadlocal` keyword, scope, initializer rule | Specified at the source level; **no executable TLS ABI** — Stage 0 rejects any use with `MLX-E9001` |
| `@atomicLoad`/`@atomicStore`/`@atomicRmw`/`@cmpxchgWeak`/`@cmpxchgStrong`/`@fence` names and the six memory orders | Specified at the source level; **no argument order, result type, or RMW representation** — Stage 0 rejects any use with `MLX-E9001` |
| Data race definition | Specified (non-atomic concurrent conflicting access with a write is UB) — but with no working atomics, the only way to satisfy this today is to not share mutable state across threads at all |

The practical consequence of the last three rows: Mlx today has no
supported way to write correct multi-threaded code that shares mutable
state, because the only two primitives that could make concurrent shared
mutation defined — atomics and `threadlocal` — are both intentionally
unimplemented rather than incompletely-but-usably implemented. This is a
deliberate stance, not an oversight: per `SPEC_CONFLICTS.md`'s framing, the
compiler "cannot emit a private TLS ABI until that contract is normative,"
and the same holds for the atomic call shapes — closing this gap requires a
normative spec change, not just an implementation effort.
