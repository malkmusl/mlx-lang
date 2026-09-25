# Operators

**Source tests:** `68_div_rem_runtime.mlx`, `69_bit_shift_runtime.mlx`,
`70_compound_assignment_runtime.mlx`, `71_shift_combine_runtime.mlx`,
`72_logical_short_circuit_runtime.mlx`, `73_prefix_operators_runtime.mlx`,
`89_checked_overflow_comptime.mlx`, `91_wrapping_saturating_runtime.mlx`,
`92_saturating_shift_runtime.mlx`, `93_checked_overflow_runtime.mlx`,
`94_wrapping_shift_count_runtime.mlx`, `113_signed_integer_runtime.mlx`,
`114_unsigned_comparison_runtime.mlx`, `183_float_exponent_runtime.mlx`,
`184_signed_right_shift_runtime.mlx`, `203_immediate_alu_gep_runtime.mlx`,
`207_popcount_runtime.mlx`

## Arithmetic, division and remainder

```mlx
// Tests integer division and remainder by evaluating 48 / 5 + 48 % 5 = 12.
pub fn main() u8 {
    return 48 / 5 + 48 % 5
}
```

(`tests/68_div_rem_runtime.mlx` → 12)

## Bitwise and shift

```mlx
// Tests left shift and bitwise OR by evaluating (5 << 2) | 3 = 23.
pub fn main() u8 {
    return 5 << 2 | 3
}
```

(`tests/69_bit_shift_runtime.mlx` → 23)

Prefix `~` (bitwise not), unary `-`, and `!` (logical not) are also
available:

```mlx
fn main() u8 {
    var value: u8 = 5
    const pointer = &value
    const changed: u8 = ~pointer.*
    if !false {
        return 0 - changed
    }
    return 1
}
```

(`tests/73_prefix_operators_runtime.mlx`)

## Compound assignment

Every binary operator has a compound-assignment form (`<<=`, `|=`, `+=`, …):

```mlx
// Tests compound shift and OR assignments by transforming 3 into 13.
pub fn main() u8 {
    var value: u8 = 3
    value <<= 2
    value |= 1
    return value
}
```

(`tests/70_compound_assignment_runtime.mlx` → 13)

## Shift-combine operators

Mlx composes a shift with a following bitwise operator into a single
operator, e.g. `|<<` (shift left, then OR into the result) and `<<|`
(saturating shift left):

```mlx
// Tests the combined shift operator |<< by evaluating 3 |<< 2 and returning 15.
pub fn main() u8 {
    return 3 |<< 2
}
```

(`tests/71_shift_combine_runtime.mlx` → 15, i.e. `(3 << 2) | 3`)

## Shift counts and signed shifts

`<<%` is a *wrapping-shift-count* left shift: unlike plain `<<`, a shift
amount that would exceed the type's bit width doesn't trap — the amount
itself wraps modulo the bit width first:

```mlx
fn main() u8 {
    const value: u7 = 1
    return @intCast(u8, value <<% 8)   // shift amount 8 wraps to 8 % 7 = 1
}
```

(`tests/94_wrapping_shift_count_runtime.mlx`)

`>>` on a signed integer is an arithmetic (sign-extending) shift — the
vacated high bits are filled with copies of the sign bit rather than
zeros, so shifting a negative number right keeps it negative:

```mlx
fn shifted(value: i64, amount: u8) -> i64 {
    return value >> amount
}

pub fn main() -> u8 {
    if shifted(-8, 2) != -2 { return 1 }   // -8 >> 2 == -2, not a huge positive number
    return 13
}
```

(`tests/184_signed_right_shift_runtime.mlx`)

## Signed vs. unsigned semantics

Division, remainder, and comparison all respect signedness. Signed division
truncates toward zero, and the sign of a negative dividend's remainder
follows the dividend:

```mlx
fn main() u8 {
    var lhs: i8 = -7
    var rhs: i8 = 2
    const quotient = lhs / rhs     // -3 (truncated toward zero, not -4)
    const remainder = lhs % rhs    // -1
    if quotient == -3 && remainder == -1 && lhs < rhs {
        return 13
    }
    return 1
}
```

(`tests/113_signed_integer_runtime.mlx`)

An unsigned comparison never reinterprets its operands as signed — a `u64`
value whose top bit is set still compares correctly as a large positive
number, not as a negative one:

```mlx
fn main() u8 {
    var large: u64 = 9223372036854775808   // 2^63: negative if compared as i64
    var small: u64 = 1
    if large > small {
        return 13
    }
    return 1
}
```

(`tests/114_unsigned_comparison_runtime.mlx`)

## Float literals

Float literals accept a decimal exponent suffix (`e`/`E`, with an optional
sign), and convert correctly between `f32`/`f64`:

```mlx
pub fn main() -> u8 {
    if 1.25e2 != 125.0 { return 1 }
    if 125e-2 != 1.25 { return 2 }
    if 2E+3 != 2000.0 { return 3 }
    if 5e-1 != 0.5 { return 4 }
    if @bitCast(u64, @floatCast(f64, 1e0)) != 4607182418800017408 { return 5 }
    if @floatCast(f32, 1.25e0) != @floatCast(f32, 1.25) { return 6 }
    return 13
}
```

(`tests/183_float_exponent_runtime.mlx`; `@bitCast` reinterprets a value's
bits as another same-size type, `@floatCast` converts between float widths)

Unary minus negates floats as floats, including literals whose type comes
from their context (`@intFromFloat(i64, -2.25)` is `-2`)
(`tests/261_float_negation_context_runtime.mlx`).

## Bit-counting

`@popCount(value)` counts the set bits in an integer, signed or unsigned:

```mlx
pub fn main() -> usize {
    const empty: u64 = 0
    const sparse: u64 = 9223372036854775809
    const alternating: u64 = 12297829382473034410
    const signed: i8 = -1
    if @popCount(empty) != 0 { return 1 }
    if @popCount(sparse) != 2 { return 2 }
    if @popCount(alternating) != 32 { return 3 }
    if @popCount(signed) != 8 { return 4 }   // -1 is all-ones in two's complement
    return 13
}
```

(`tests/207_popcount_runtime.mlx`)

A chain of arithmetic and bitwise operators combined with array indexing —
the kind of expression that exercises the backend's immediate-operand and
address-calculation paths — evaluates with ordinary operator precedence:

```mlx
fn calculate(value: i64) -> i64 {
    const added = value + 7
    const multiplied = added * -3
    const subtracted = multiplied - 5
    const masked = subtracted & 255
    return (masked | 256) ^ 17
}

pub fn main() -> u8 {
    if calculate(11) != 468 { return 1 }
    const bytes = [_]u8{ 3, 5, 8, 13 }
    if bytes[3] != 13 { return 2 }
    return 13
}
```

(`tests/203_immediate_alu_gep_runtime.mlx`; see
[docs/internals/selfhost-compiler.md](../internals/selfhost-compiler.md) for
how the self-hosted x86_64 backend generates code for expressions like this)

## Logical operators and short-circuit evaluation

`&&` and `||` short-circuit and bind with the usual precedence (`&&` tighter
than `||`):

```mlx
// Tests logical precedence and short-circuit evaluation; true || (...) selects the branch returning 1.
pub fn main() u8 {
    if true || false && false {
        return 1
    } else {
        return 2
    }
}
```

(`tests/72_logical_short_circuit_runtime.mlx` → 1)

## Overflow behavior

Plain arithmetic operators (`+`, `-`, `*`) are **checked**: overflow is a
compile-time error for constant expressions and a runtime trap for values
only known at runtime.

```mlx
fn main() u8 {
    const value: u8 = 255 + 1   // rejected at compile time: constant overflow
    return value
}
```

(`tests/89_checked_overflow_comptime.mlx`)

```mlx
fn add(left: u8, right: u8) u8 {
    return left + right
}

fn main() u8 {
    return add(250, 10)   // traps at runtime: 250 + 10 overflows u8
}
```

(`tests/93_checked_overflow_runtime.mlx`)

For explicit wraparound or clamping semantics, use the `%`- and `|`-prefixed
compound operators: `+%=`/`-%=`/`*%=` wrap, and `+|=`/`-|=`/`*|=` saturate at
the type's bounds. `<<|` saturates a left shift instead of trapping/wrapping.

```mlx
fn main() u8 {
    var wrapping: u8 = 250
    wrapping +%= 10          // wraps to 4

    var saturating: u8 = 250
    saturating +|= 10        // saturates at 255

    var underflow: u8 = 3
    underflow -|= 9          // saturates at 0

    if wrapping != 4 {
        return 1
    }
    if underflow != 0 {
        return 2
    }
    return saturating
}
```

(`tests/91_wrapping_saturating_runtime.mlx` → 255)

```mlx
fn main() u8 {
    const value: u8 = 200
    return value <<| 2   // saturates instead of overflowing
}
```

(`tests/92_saturating_shift_runtime.mlx`)
