# Bootstrap architecture

```text
Linux + Zig
    |
    v
 mlx0 (Zig bootstrap compiler)
    |
    v
 Stage-1 compiler std (allocators, collections, files, diagnostics,
    |                   process arguments and ELF64 emission)
    |
    v
 mlx1 compiler core written in Mlx
    |\
    | +--> Stage-1 extensions (non-blocking)
    |      +--> std.xml / std.json
    |      +--> broader std.posix / std.os.*
    |      +--> std.wayland and other protocol modules
    |
    v
 mlx2 (self-compiled canonical compiler)
    |
    +--> full std rebuilt entirely with Mlx
    +--> brixOS
```

brixOS being written in Mlx is not a cycle. Mlx is bootstrapped first on an existing host. The canonical compiler then builds the Mlx standard library and brixOS. Once brixOS can run Mlx, the system can rebuild itself without Zig.

Wayland is a Stage-1 protocol extension, not part of the compiler std and not a per-project build feature. Its canonical XML is consumed after the compiler core can build ordinary Mlx modules. Ordinary applications only use:

```mlx
const wl = @import("std.wayland")
```

The compiler has no Wayland/XML special case. `std.xml`, the Wayland protocol parser, generated declarations, client/server runtime and OS transports are Mlx code.
