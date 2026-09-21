# ABI: calling conventions

**Normative source:** `spec/01-abi/mlxcc-x86_64.xml`, `spec/01-abi/foreign-abi.xml`

**Source tests:** `121_slice_parameter_abi_runtime.mlx`,
`122_slice_return_abi_runtime.mlx`, `123_slice_stack_argument_abi_runtime.mlx`,
`124_struct_register_return_runtime.mlx`, `125_array_memory_return_runtime.mlx`,
`126_struct_argument_return_runtime.mlx`, `127_syscall_getpid_runtime.mlx`,
`128_syscall_six_args_runtime.mlx`, `136_six_register_parameters_runtime.mlx`,
`115_multi_return_runtime.mlx`, `116_multi_return_two_registers_runtime.mlx`,
`117_multi_return_memory_runtime.mlx`, `46_error_union_abi_runtime.mlx`,
`58_error_union_slice_abi_runtime.mlx`, `59_error_union_float_abi_runtime.mlx`,
`61_error_union_memory_abi_runtime.mlx`, `62_error_union_multi_register_abi_runtime.mlx`,
`177_float_sse_runtime.mlx`, `178_float_cast_abi_runtime.mlx`,
`186_threadlocal_abi_gap.mlx`, `198_direct_and_indirect_call_runtime.mlx`,
`130_function_pointer_runtime.mlx`, `229_aggregate_field_assignment_runtime.mlx`

This chapter covers two distinct ABIs that Mlx defines:

- **mlxcc** — the internal calling convention every ordinary `fn` uses, defined
  in `spec/01-abi/mlxcc-x86_64.xml`. It is close to (but not identical to) the
  x86_64 System V ABI.
- The **foreign ABI**, defined in `spec/01-abi/foreign-abi.xml`, used by
  `extern("sysv")`, `extern("win64")`, and `extern("syscall")` declarations
  when Mlx code calls (or is called by) non-Mlx code, including the Linux
  kernel directly.

This is not a guide chapter; it documents implementation-level contracts.
Where the guide's [Functions](../guide/04-functions.md) chapter says a value
"crosses the call boundary," this chapter says exactly how.

## Registers and stack

mlxcc's integer/pointer argument registers, stack discipline, and
caller/callee-saved sets are fixed by `spec/01-abi/mlxcc-x86_64.xml`:

```xml
<Stack grows="down" alignmentBeforeCall="16" redZone="false"/>
<IntegerArgumentRegisters>rdi rsi rdx rcx r8 r9</IntegerArgumentRegisters>
<FloatArgumentRegisters>xmm0 xmm1 xmm2 xmm3 xmm4 xmm5 xmm6 xmm7</FloatArgumentRegisters>
<CallerSaved>rax rcx rdx rsi rdi r8 r9 r10 r11 xmm0-xmm15</CallerSaved>
<CalleeSaved>rbx rbp r12 r13 r14 r15 rsp</CalleeSaved>
```

That is the same register order and stack-growth direction as the x86_64
System V ABI, with no red zone. `136_six_register_parameters_runtime.mlx`
exercises all six integer argument registers directly:

```mlx
fn combine(a: u8, b: u8, c: u8, d: u8, e: u8, f: u8) -> u8 {
    return a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6
}

pub fn main() -> u8 {
    return combine(1, 2, 3, 4, 5, 6)
}
```

(`tests/136_six_register_parameters_runtime.mlx`)

A seventh integer argument spills to the stack. `123_slice_stack_argument_abi_runtime.mlx`
passes five `u8` values (`a`..`e`, filling five of the six integer argument
registers) and then a slice; since the slice needs two eightbytes and only
one integer register remains, `ArgumentAssignment`'s atomic-aggregate rule
sends the whole slice to the stack instead of splitting it — the fixture's
own comment states the slice's length ends up as "the seventh stack
argument":

```mlx
// The slice consumes two physical slots; its length is the seventh stack argument.
fn select(a: u8, b: u8, c: u8, d: u8, e: u8, values: []const u8) -> u8 {
    var result: u8 = a + b + c + d + e
    for item in values {
        result = result + item
    }
    return result
}

pub fn main() -> u8 {
    const values = [_]u8{1, 8}
    return select(1, 1, 1, 1, 1, values[0..])
}
```

(`tests/123_slice_stack_argument_abi_runtime.mlx`)

`ArgumentAssignment` in the spec makes this precise: stack arguments use
"naturally aligned slots of at least 8 bytes," final pre-call `rsp` alignment
is 16 bytes, and a multi-eightbyte aggregate (such as a slice) is assigned
registers atomically — if it cannot fully fit in the remaining registers, the
*whole* aggregate goes to the stack rather than being split across the
register/stack boundary.

## Float/SSE arguments and returns

`f32`/`f64` values use the SSE class and are passed/returned in the
`xmm0`..`xmm7` float argument registers, banked independently from the
integer registers:

```mlx
fn arithmetic(left: f64, right: f64) -> f64 {
    return (left + right) * right / left - right
}

fn mixed(integer_a: u8, float_a: f64, integer_b: u16, float_b: f32) -> u8 {
    if integer_a != 7 { return 1 }
    if float_a != 1.5 { return 2 }
    if integer_b != 513 { return 3 }
    if float_b != @floatCast(f32, 3.5) { return 4 }
    return 13
}
```

(`tests/177_float_sse_runtime.mlx`; `mixed` shows an integer and a float
argument interleaved in source order while still drawing from separate
integer/SSE register banks)

`178_float_cast_abi_runtime.mlx` pushes both banks to their limits in one
call — seven `u8` integer arguments (filling all six integer registers plus
one stack slot) and nine `f64` arguments (filling all eight `xmm` registers
plus one stack slot), then checks specific elements from each bank landed
correctly:

```mlx
fn overflowBanks(i0: u8, i1: u8, i2: u8, i3: u8, i4: u8, i5: u8, i6: u8, f0: f64, f1: f64, f2: f64, f3: f64, f4: f64, f5: f64, f6: f64, f7: f64, f8: f64) -> u8 {
    if i0 != 1 || i5 != 6 || i6 != 7 { return 1 }
    if f0 != 1.0 || f7 != 8.0 || f8 != 9.0 { return 2 }
    return 13
}
```

(`tests/178_float_cast_abi_runtime.mlx`)

Normal-return floating values use `xmm0` (and `xmm1` for a second
eightbyte), per `<NormalReturn><Floating first="xmm0" second="xmm1"/>`.

## Slices: pointer + length

A slice (`[]T`) is not a single scalar for ABI purposes — the spec's
`AggregateClassification` rules classify it as a two-eightbyte aggregate (a
pointer eightbyte and a length eightbyte), so it is passed and returned as a
`{pointer, length}` pair through whichever register/stack slots its position
resolves to:

```mlx
// A slice parameter is transported as pointer plus length and remains iterable.
fn second(values: []const u8) -> u8 {
    return values[1]
}

pub fn main() -> u8 {
    const values = [_]u8{4, 13}
    return second(values[0..])
}
```

(`tests/121_slice_parameter_abi_runtime.mlx`)

Returned, a plain slice fits the 16-byte register-return path and comes back
in `rax`/`rdx` (pointer in `rax`, length in `rdx`), per `NormalReturn`'s
two-eightbyte register rule:

```mlx
// A plain slice return transports both pointer and length through rax/rdx.
fn tail(values: []const u8) -> []const u8 {
    return values[1..]
}
```

(`tests/122_slice_return_abi_runtime.mlx`)

The [Unsafe and safety](../guide/09-unsafe-and-safety.md) chapter notes that
a slice "carries its length across a call boundary (part of its ABI)" — this
is exactly that mechanism: the length travels as a real ABI value, not
something recovered from the pointee, which is what lets a callee's
bounds check on a parameter slice be sound.

## Aggregate (struct/array) argument and return classification

`AggregateClassification` in `mlxcc-x86_64.xml` fixes the size threshold:
non-packed aggregates of 1..16 bytes are split into one or two eightbytes and
classified INTEGER/SSE/MEMORY recursively; anything larger than 16 bytes (or
whose eightbytes can't be satisfied by available registers) is passed/returned
via memory instead.

A 16-byte struct of two `u64` fields fits the register path and returns in
`rax`/`rdx`:

```mlx
// A 16-byte struct crosses the call boundary in rax/rdx.
const Pair = struct {
    first: u64
    second: u64
}

fn make_pair() -> Pair {
    return Pair.{ .first = 7, .second = 12 }
}
```

(`tests/124_struct_register_return_runtime.mlx`)

A struct argument is likewise copied into ABI-appropriate storage (registers
here, since `Pair` is two `u8` fields) before the call, and struct fields are
value-copied rather than referenced:

```mlx
// Struct arguments are copied into caller-owned ABI storage before the call.
const Pair = struct {
    first: u8
    second: u8
}

fn swap(value: Pair) -> Pair {
    return Pair.{ .first = value.second, .second = value.first }
}
```

(`tests/126_struct_argument_return_runtime.mlx`)

Aggregate field *assignment* (as opposed to argument passing) copies bytes
the same way — assigning one struct-typed field to another does not alias:

```mlx
// Aggregate field assignment copies the value bytes rather than its address.
holder.pair = replacement
```

(`tests/229_aggregate_field_assignment_runtime.mlx`)

A 20-byte array exceeds the 16-byte register-return threshold, so it returns
through caller-owned hidden memory instead:

```mlx
// An aggregate larger than 16 bytes uses caller-owned hidden return storage.
fn make_values() -> [20]u8 {
    return [_]u8{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20}
}
```

(`tests/125_array_memory_return_runtime.mlx`)

Per `NormalReturn`'s rule for larger/MEMORY returns, this hidden-storage
pointer is passed as the *first* integer argument (ahead of the callee's own
declared parameters, per `ArgumentAssignment`'s "a hidden large-return
pointer, when present, consumes rdi before user integer arguments"), and
is returned unchanged in `rax`.

## Multiple returns

A source-level multiple return (`-> (T, U, ...)`) is classified exactly as
its concrete tuple layout under the same aggregate rules — small tuples use
the normal register-return path, larger ones use hidden memory storage:

```mlx
fn connect() -> (u8, u8) {
    return -> (7, 6)
}

fn main() -> u8 {
    const result = connect()
    return result.0 + result.1
}
```

(`tests/115_multi_return_runtime.mlx`; two `u64` fields still fit in
`rax`/`rdx` — `tests/116_multi_return_two_registers_runtime.mlx`)

Three `u64` values (24 bytes) exceed the 16-byte threshold and fall back to
memory return:

```mlx
fn triple() -> (u64, u64, u64) {
    return -> (4, 5, 6)
}
```

(`tests/117_multi_return_memory_runtime.mlx`)

## Error unions (`!T`) across the call boundary

`ErrorUnionReturn` defines a dedicated transport shape for `!T`: the error
tag always occupies `rax` (zero on success, non-zero on failure), and the
payload — since `rax` is taken — uses a *separate* payload register bank
rather than the normal-return registers:

```xml
<ErrorUnionReturn errorRegister="rax" successValue="0" failure="non-zero">
  <IntegerPayloadRegisters>rdx rcx r8 r9</IntegerPayloadRegisters>
  <FloatPayloadRegisters>xmm0 xmm1 xmm2 xmm3</FloatPayloadRegisters>
</ErrorUnionReturn>
```

A small scalar payload (`!u8`) rides in the payload bank and survives being
forwarded through an intermediate function unchanged:

```mlx
// Tests forwarding and unwrapping a !u8 result across calls; the ABI path must deliver payload 7.
fn source() !u8 {
    return 7
}

fn forward() !u8 {
    return source()
}
```

(`tests/46_error_union_abi_runtime.mlx`)

The same forwarding pattern is exercised for a slice payload (pointer+length
riding two payload registers):

```mlx
fn source() ![]const u8 {
    return "abc"
}
```

(`tests/58_error_union_slice_abi_runtime.mlx`)

...and for a float payload, which uses the float payload bank (`xmm0`) while
`rax` still carries the error tag:

```mlx
fn source() !f64 {
    return 1.5
}
```

(`tests/59_error_union_float_abi_runtime.mlx`)

A payload that fits within four INTEGER and four SSE payload registers uses
that bank in layout order — a two-element `[2]u64` array payload (16 bytes,
two INTEGER payload registers) still returns by register:

```mlx
fn source() ![2]u64 {
    return [_]u64{6, 7}
}
```

(`tests/62_error_union_multi_register_abi_runtime.mlx`)

A larger payload (a five-element `[5]u64`, 40 bytes) exceeds the payload
register bank and instead uses caller-provided payload storage, passed as the
first integer argument, exactly as for an oversized plain return — `rax`
still carries only the error tag:

```mlx
fn source() ![5]u64 {
    return [_]u64{1, 2, 3, 4, 5}
}
```

(`tests/61_error_union_memory_abi_runtime.mlx`)

## Function pointers and indirect calls

A function value assigned to a variable of function-pointer type (`fn(...) ->
T`) can be called indirectly, and both the direct and indirect calls to the
same underlying function go through the same mlxcc argument/return
convention:

```mlx
fn increment(value: u8) -> u8 {
    return value + 1
}

fn invoke(callback: fn(u8) -> u8, value: u8) -> u8 {
    return callback(value)
}

pub fn main() -> u8 {
    const direct = increment(5)
    const callback: fn(value: u8) -> u8 = increment
    return direct + invoke(callback, 6)
}
```

(`tests/198_direct_and_indirect_call_runtime.mlx`; a simpler version of the
same pattern is `tests/130_function_pointer_runtime.mlx`)

## The foreign ABI: `extern` and raw syscalls

`spec/01-abi/foreign-abi.xml` defines three supported foreign conventions —
`extern("sysv")` (the standard x86_64 System V ABI, for calling/being called
by C-family code), `extern("win64")` (the Windows x64 ABI, with its 32-byte
shadow space), and `extern("syscall")` (a target-defined raw syscall ABI).
`mlxcc` itself is a fourth, internal convention distinct from all three.

For Linux x86_64, `extern("syscall")` is explicit about register placement:

```xml
<Rule>extern("syscall") is target-defined raw syscall ABI; on Linux x86_64 syscall number is rax and arg4 uses r10 instead of rcx.</Rule>
```

A zero-argument-beyond-the-number syscall (`getpid`, syscall number 39) shows
the minimal shape — the syscall number is the function's own first
parameter:

```mlx
// extern("syscall") lowers directly to the Linux x86_64 raw syscall ABI.
pub extern("syscall") fn raw0(number: usize) -> isize {}

pub fn main() -> u8 {
    const pid = raw0(39)
    if pid > 0 {
        return 0
    } else {
        return 1
    }
}
```

(`tests/127_syscall_getpid_runtime.mlx`)

A six-kernel-argument syscall (`mmap`, number 9) exercises every argument
register the raw syscall ABI defines, including the `r10`-for-arg4 override
called out above — a normal mlxcc/sysv call would have used `rcx` for the
fourth integer argument, but the kernel calling convention reserves `rcx` for
the `syscall` instruction's own use and substitutes `r10`:

```mlx
// mmap exercises all six kernel argument registers, including arg4 in r10.
pub extern("syscall") fn raw6(number: usize, a1: usize, a2: usize, a3: usize, a4: usize, a5: isize, a6: usize) -> isize {}
pub extern("syscall") fn raw2(number: usize, a1: isize, a2: usize) -> isize {}

pub fn main() -> u8 {
    const address = raw6(9, 0, 4096, 3, 34, -1, 0)
    if address < 0 {
        return 1
    }
    const result = raw2(11, address, 4096)
    if result != 0 {
        return 2
    }
    return 0
}
```

(`tests/128_syscall_six_args_runtime.mlx`; `raw2` here is `munmap`, syscall
number 11)

The foreign-ABI spec also fixes the C-compatible scalar type aliases
(`c_char`, `c_int`, `c_long`, `c_size`, ...) and the opaque-pointer type
(`*anyopaque`) used at an `extern("sysv")`/`extern("win64")` boundary, and is
explicit that conformance never requires a C preprocessor, C parser, Clang,
libclang, or header translator (`NoRequiredCParser`).

## Open ABI gap: thread-local storage

Unlike every rule above, thread-local storage has no defined ABI yet.
`SPEC_CONFLICTS.md`'s "Thread-local storage ABI" entry explains why:
`spec/00-language/atomics-tls.xml` fixes the `threadlocal` declaration's
source spelling, scope, and initializer requirement, but does not define
"the executable TLS model, TLS relocation model, per-thread initialization
protocol, or how a freestanding executable obtains its thread pointer." Per
the project's normative rule (`SPEC_INDEX.md`: "the implementation must
never silently invent behavior"), the compiler rejects a `threadlocal`
declaration outright rather than lowering it as ordinary global storage:

```mlx
// The TLS ABI is not normatively specified yet. Compilation must reject this
// declaration instead of silently treating it as ordinary global storage.
threadlocal var counter: usize = 0

pub fn main() u8 {
    return @intCast(u8, counter)
}
```

(`tests/186_threadlocal_abi_gap.mlx` — a compile-error fixture, not a runtime
one: it documents the gap by showing what must *not* compile until a TLS ABI
becomes normative)
