# Operators

**Source tests:** `68_div_rem_runtime.mlx`, `69_bit_shift_runtime.mlx`,
`70_compound_assignment_runtime.mlx`, `71_shift_combine_runtime.mlx`,
`72_logical_short_circuit_runtime.mlx`, `73_prefix_operators_runtime.mlx`,
`89_checked_overflow_comptime.mlx`, `91_wrapping_saturating_runtime.mlx`,
`92_saturating_shift_runtime.mlx`, `93_checked_overflow_runtime.mlx`

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
