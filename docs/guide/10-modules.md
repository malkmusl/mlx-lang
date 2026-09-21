# Modules

**Source tests:** `75_import_const_runtime.mlx`, `76_import_private_error.mlx`,
`77_import_not_found_error.mlx`, `78_import_function_runtime.mlx`

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
