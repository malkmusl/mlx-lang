# Modules

**Source tests:** `75_import_const_runtime.mlx`, `76_import_private_error.mlx`,
`77_import_not_found_error.mlx`, `78_import_function_runtime.mlx`,
`135_imported_string_literal_runtime.mlx`, `175_import_aggregate_functions_runtime.mlx`

## `@import`

`@import("path")` loads another source file as a module value, resolving
relative paths against the importing file. Its declarations are accessed as
members of that value:

```mlx
const values = @import("./modules/75_values.mlx")

fn main() u8 {
    return values.answer
}
```

(`tests/75_import_const_runtime.mlx`)

Functions defined in the imported module are called the same way:

```mlx
const values = @import("./modules/75_values.mlx")

fn main() u8 {
    return values.add(19, 23)
}
```

(`tests/78_import_function_runtime.mlx`)

A struct's methods (see [Types and aggregates](05-types-and-aggregates.md))
work identically when the struct type itself is imported from another
module — calling an associated function through the module value and
calling a `self`-taking method on an instance both work exactly as if the
struct had been declared locally:

```mlx
// tests/modules/175_parser.mlx
pub const Parser = struct {
    value: u8

    pub fn rejected() -> u8 {
        return 17
    }

    pub fn current(self: Self) -> u8 {
        return self.value
    }
}
```

```mlx
const parser_module = @import("./modules/175_parser.mlx")

fn main() -> u8 {
    if parser_module.Parser.rejected() != 17 { return 1 }
    const parser = parser_module.Parser.{ .value = 13 }
    return parser.current()
}
```

(`tests/175_import_aggregate_functions_runtime.mlx`)

A string literal returned from an imported function behaves exactly like a
local one — including carrying an implicit trailing NUL byte one past its
slice length, even though the slice's own length only covers the visible
characters:

```mlx
// tests/modules/135_string.mlx
pub fn escaped() -> []const u8 {
    return "ok\n"
}
```

```mlx
const strings = @import("./modules/135_string.mlx")

pub fn main() -> u8 {
    const value = strings.escaped()
    var length: usize = 0
    for byte in value {
        length += 1
    }
    if length != 3 || value[0] != 111 || value[1] != 107 || value[2] != 10 {
        return 1
    }
    unsafe {
        if @ptrCast([*]const u8, value)[3] != 0 {   // one byte past the 3-byte slice: NUL
            return 2
        }
    }
    return 13
}
```

(`tests/135_imported_string_literal_runtime.mlx`)

## Visibility

Only `pub` declarations are visible outside their module. Accessing a
non-`pub` (private) declaration through an imported module value is a
compile-time error:

```mlx
const values = @import("./modules/75_values.mlx")

fn main() u8 {
    return values.secret   // rejected: `secret` isn't declared `pub` in 75_values.mlx
}
```

(`tests/76_import_private_error.mlx`)

## Missing modules

Importing a path that doesn't resolve to a real file is a compile-time
error, not a runtime one — module resolution happens before any code runs:

```mlx
const missing = @import("./modules/does-not-exist.mlx")

fn main() u8 {
    return 0
}
```

(`tests/77_import_not_found_error.mlx`)
