# Unsafe and safety

**Source tests:** `14_unsafe_ptr_builtin.mlx`, `17_unsafe_ptr_roundtrip.mlx`,
`18_invalid_pointer_alignment.mlx`, `225_runtime_bounds_safety.mlx`,
`226_runtime_bounds_in_range.mlx`, `227_runtime_slice_index_safety.mlx`,
`228_runtime_slice_range_safety.mlx`, `231_optional_pointer_unwrap_safety.mlx`,
`232_optional_pointer_unwrap_valid.mlx`

## The `unsafe {}` boundary

Operations that can violate memory safety — most notably constructing a raw
pointer from an integer — are only permitted inside an explicit `unsafe {}`
block. Using them outside `unsafe` is a compile-time error:

```mlx
pub fn main() usize {
    const ptr = @ptrFromInt(*u8, 1)   // MLX-E7004: unsafe operation outside unsafe block
    return @intFromPtr(ptr)
}
```

(`tests/14_unsafe_ptr_builtin.mlx`)

Inside `unsafe {}`, the same round trip is allowed. `@ptrFromInt`'s target
type can also carry pointer modifiers such as `align(N)` and `volatile`:

```mlx
pub fn main() usize {
    unsafe {
        const ptr = @ptrFromInt(*align(8) volatile u8, 1)
        return @intFromPtr(ptr)
    }
}
```

(`tests/17_unsafe_ptr_roundtrip.mlx`)

An invalid alignment value (not a power of two) is rejected even inside
`unsafe {}` — the unsafe boundary opts into raw-pointer construction, not
into malformed types:

```mlx
pub fn main() usize {
    unsafe {
        const ptr = @ptrFromInt(*align(3) u8, 1)   // MLX-E7002: invalid alignment
        return @intFromPtr(ptr)
    }
}
```

(`tests/18_invalid_pointer_alignment.mlx`)

## Runtime safety checks

In a safe build, the compiler inserts runtime checks around operations that
would otherwise be undefined behavior on an out-of-range access, and traps
(aborts) instead of reading/writing outside the intended memory.

Array indexing is checked against the array's compile-time-known length:

```mlx
noinline fn readAt(index: usize) -> u8 {
    var values: [2]u8 = [2]u8{ 17, 29 }
    return values[index]
}

fn main() -> u8 {
    return readAt(2)   // traps: index 2 is out of range for a 2-element array
}
```

(`tests/225_runtime_bounds_safety.mlx`; an in-range index, e.g. `readAt(1)`,
runs normally — `tests/226_runtime_bounds_in_range.mlx`)

The `noinline` modifier here is deliberate: it forces `readAt` to remain a
real call with a runtime-only `index`, so the check can't be proven
statically and elided by the optimizer.

A slice carries its length across a call boundary (part of its ABI — see
[Functions](04-functions.md)), and indexing/sub-slicing it is checked the
same way:

```mlx
noinline fn readAt(values: []const u8, index: usize) -> u8 {
    return values[index]
}

fn main() -> u8 {
    const values = [_]u8{ 17, 29 }
    return readAt(values[0..], 2)   // traps: index 2 is out of range
}
```

(`tests/227_runtime_slice_index_safety.mlx`)

```mlx
noinline fn firstInSlice(values: []const u8, upper: usize) -> u8 {
    const result = values[0..upper]
    return result[0]
}

fn main() -> u8 {
    const values = [_]u8{ 17, 29 }
    return firstInSlice(values[0..], 3)   // traps: upper bound 3 exceeds the source
}
```

(`tests/228_runtime_slice_range_safety.mlx`)

Unwrapping (`.?`) a `null` optional pointer traps before the would-be
dereference happens, rather than reading through address 0:

```mlx
fn main() -> u8 {
    const pointer: ?*u8 = null
    return pointer.?.*   // traps: pointer.? on a null optional
}
```

(`tests/231_optional_pointer_unwrap_safety.mlx`)

A present optional pointer unwraps to the same pointee it held, unaffected
by the safety check:

```mlx
fn main() -> u8 {
    var value: u8 = 37
    const pointer: ?*u8 = &value
    return pointer.?.*   // 37
}
```

(`tests/232_optional_pointer_unwrap_valid.mlx`)
