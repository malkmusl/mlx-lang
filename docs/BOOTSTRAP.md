# Bootstrap architecture

```text
Linux + Zig
    |
    v
 zin0 (Zig bootstrap compiler)
    |
    v
 Stage-1 compiler std (allocators, collections, files, diagnostics,
    |                   process arguments and ELF64 emission)
    |
    v
 zin1 compiler core written in Zin
    |\
    | +--> Stage-1 extensions (non-blocking)
    |      +--> std.xml / std.json
    |      +--> broader std.posix / std.os.*
    |      +--> std.wayland and other protocol modules
    |
    v
 zin2 (self-compiled canonical compiler)
    |
    +--> full std rebuilt entirely with Zin
    +--> brixOS
```

brixOS being written in Zin is not a cycle. Zin is bootstrapped first on an existing host. The canonical compiler then builds the Zin standard library and brixOS. Once brixOS can run Zin, the system can rebuild itself without Zig.

Wayland is a Stage-1 protocol extension, not part of the compiler std and not a per-project build feature. Its canonical XML is consumed after the compiler core can build ordinary Zin modules. Ordinary applications only use:

```zin
const wl = @import("std.wayland")
```

The compiler has no Wayland/XML special case. `std.xml`, the Wayland protocol parser, generated declarations, client/server runtime and OS transports are Zin code.
