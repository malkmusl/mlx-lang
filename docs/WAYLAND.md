# Native Wayland in Mlx

Mlx treats Wayland as part of its standard library ecosystem, not as a C dependency and not as a project-local schema import.

```text
canonical wayland.xml / extension XML
             |
             v
          std.xml
             |
             v
  std.wayland.protocol parser
             |
             v
       Protocol AST
        /        \
       v          v
 client decls   server decls
        \        /
         v      v
         std.wayland
             |
      +------+------+ 
      |      |      |
    Linux   BSD   brixOS
```

The XML is consumed while the Mlx standard library is bootstrapped/rebuilt. Applications do not add the XML to `build.mlx` and do not call a schema builtin.

Client code:

```mlx
const wl = @import("std.wayland")

pub fn main() !void {
    var display = try wl.client.Display.connect(allocator)
    defer display.disconnect()

    const registry = try display.getRegistry()
    _ = registry

    while display.running() {
        try display.dispatch()
    }
}
```

Server/compositor code imports the same module and uses `wl.server`.
