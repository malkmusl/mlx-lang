# Control flow

**Source tests:** `06_control_flow.mlx`, `42_if_else_runtime.mlx`,
`43_while_break_runtime.mlx`, `44_for_break_runtime.mlx`,
`45_for_index_runtime.mlx`, `49_while_state_break_runtime.mlx`,
`50_for_continue_runtime.mlx`, `51_if_expression_runtime.mlx`,
`52_labeled_loops_runtime.mlx`, `53_break_value_runtime.mlx`,
`47_break_outside_loop.mlx`, `54_unknown_loop_label.mlx`, `55_array_for_runtime.mlx`,
`56_slice_for_runtime.mlx`, `57_array_pointer_capture_runtime.mlx`,
`63_labeled_continue_runtime.mlx`, `64_break_value_requires_infinite_loop.mlx`,
`83_match_range_runtime.mlx`, `84_match_non_exhaustive.mlx`,
`96_enum_match_runtime.mlx`, `98_nonexhaustive_enum_requires_else.mlx`

## `if`/`else`

`if` takes an unparenthesized-or-parenthesized `bool` condition (both forms
appear in the suite) and can be used as a statement:

```mlx
fn main() u8 {
    const a = 1
    const b = 2
    if (true) {
        const c = a + b
    } else {
        const c = a * b
    }
    // ...
    return a + b + b
}
```

(`tests/06_control_flow.mlx` → 5)

`if` is also an expression:

```mlx
// Tests if as an expression by selecting 4 from the true branch and returning it.
pub fn main() u8 {
    const result = if true {
        4
    } else {
        1
    }
    return result
}
```

(`tests/51_if_expression_runtime.mlx` → 4)

A non-`bool` condition (e.g. a bare integer) is rejected at compile time
(`tests/48_non_bool_condition.mlx`).

## `while`

```mlx
fn main() u8 {
    var value: u8 = 1
    while true {
        value = 6
        break
    }
    return value
}
```

(adapted from `tests/43_while_break_runtime.mlx` → 6)

`while` carries loop-local mutable state naturally, since `break` just exits
the loop:

```mlx
// Tests mutable while-loop state and break by stopping when value reaches 3.
```

(`tests/49_while_state_break_runtime.mlx`)

`break` (and `continue`) are only meaningful inside a loop — using either
outside any enclosing loop is a compile-time error, the same kind of
loop-context validation that rejects an unknown label:

```mlx
pub fn main() void {
    break   // rejected: no enclosing loop
}
```

(`tests/47_break_outside_loop.mlx`)

## `for`

`for` iterates a range, an array, or a slice. `for value in <range|container>`
binds the element; a second binding after a comma is the index:

```mlx
// Tests range iteration and break by capturing the first value equal to 4.
pub fn main() u8 {
    var found: u8 = 0
    for value in 0..8 {
        if value == 4 {
            found = value
            break
        }
    }
    return found
}
```

(`tests/44_for_break_runtime.mlx` → 4)

```mlx
// Tests range iteration with an index variable; index 2 selects value 5 and returns it.
pub fn main() u8 {
    for value, index in 3..7 {
        if index == 2 {
            return value
        }
    }
    return 9
}
```

(`tests/45_for_index_runtime.mlx` → 5)

`continue` skips to the next iteration:

```mlx
// Tests continue in a range loop by skipping values below 3 and returning the first accepted value.
```

(`tests/50_for_continue_runtime.mlx`)

Iterating an array or a string slice works the same way:

```mlx
pub fn main() u8 {
    const items = [_]u8{2, 3, 4}
    var sum: u8 = 0
    for item in items {
        sum = sum + item
    }
    return sum
}
```

(`tests/55_array_for_runtime.mlx` → 9)

```mlx
pub fn main() u8 {
    var result: u8 = 0
    for item, index in "abc" {
        if index == 2 {
            result = item
        }
    }
    return result
}
```

(`tests/56_slice_for_runtime.mlx` → `'c'` = 99)

Prefixing the loop binding with `*` captures a pointer to each element
instead of a copy — useful for mutation, and for writing through the
element:

```mlx
pub fn main() u8 {
    var items = [_]u8{4, 8}
    var result: u8 = 0
    for *item in items {
        result = item.*
    }
    return result
}
```

(`tests/66_pointer_deref_runtime.mlx` → 8, see also `tests/57_array_pointer_capture_runtime.mlx`)

## Labels

Any loop can carry a `label:` prefix. `break :label` and `continue :label`
target an enclosing labeled loop instead of the innermost one, and
`break :label value` can also yield a value from that loop when it is used
as an expression:

```mlx
pub fn main() u8 {
    var result: u8 = 0
    outer: while true {
        while true {
            result = 9
            break :outer
        }
    }
    return result
}
```

(`tests/52_labeled_loops_runtime.mlx` → 9)

```mlx
pub fn main() u8 {
    const result = outer: while true {
        while true {
            break :outer 11
        }
    }
    return result
}
```

(`tests/53_break_value_runtime.mlx` → 11)

```mlx
pub fn main() u8 {
    var count: u8 = 0
    outer: while count < 3 {
        count = count + 1
        while true {
            continue :outer
        }
    }
    return count
}
```

(`tests/63_labeled_continue_runtime.mlx` → 3)

Breaking to an undeclared label (`tests/54_unknown_loop_label.mlx`) is
rejected at compile time, and a value-carrying `break` outside an infinite
(`while true`) loop is also rejected
(`tests/64_break_value_requires_infinite_loop.mlx`).

## `match`

`match` compares a value against a set of arms, which may be single values,
inclusive ranges, enum-tag shorthand (`.tag`), or `else`:

```mlx
fn main() u8 {
    const value: u8 = 7
    return match value {
        0 => 1
        3..8 => 42
        else => 9
    }
}
```

(`tests/83_match_range_runtime.mlx` → 42)

```mlx
const Mode = enum(u8) {
    idle,
    active
}

fn choose(mode: Mode) u8 {
    return match mode {
        .idle => 3,
        .active => 9
    }
}
```

(`tests/96_enum_match_runtime.mlx`)

A `match` over a value with more possible cases than are covered must be
exhaustive — either every case is listed, or an `else` arm is present:

```mlx
fn main() u8 {
    const value: u8 = 7
    return match value {
        7 => 1
        // missing else / other arms: rejected as non-exhaustive
    }
}
```

(`tests/84_match_non_exhaustive.mlx`)

A `nonexhaustive` enum (see [Types and aggregates](05-types-and-aggregates.md))
always requires an `else` arm even if every currently-declared tag is
listed, since more tags can be added later without breaking callers:

```mlx
const Mode = enum(u8, nonexhaustive) {
    idle
}

fn main() u8 {
    return match Mode.idle {
        .idle => 1
        // still rejected without `else`
    }
}
```

(`tests/98_nonexhaustive_enum_requires_else.mlx`)
