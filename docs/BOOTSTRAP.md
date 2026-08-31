# Bootstrap architecture

```text
Linux + Zig
    |
    v
 zin0 (Zig bootstrap compiler)
    |
    +--> compiler bootstrap std (allocators, collections, files, diagnostics,
    |    process arguments and ELF64 emission)
    |
    +--> zin1 compiler core written in Zin
    |
    v
 Stage-1 Zin std foundation
    |
    +--> std.xml / std.json
    +--> std.posix / std.os.*
    +--> consume canonical wayland.xml once for stdlib construction
    +--> materialize std.wayland client + server API
    |
    v
 zin2 (self-compiled canonical compiler)
    |
    +--> full std rebuilt entirely with Zin
    +--> brixOS
```

brixOS being written in Zin is not a cycle. Zin is bootstrapped first on an existing host. The canonical compiler then builds the Zin standard library and brixOS. Once brixOS can run Zin, the system can rebuild itself without Zig.

Wayland is not a per-project build feature. Its canonical XML is an input used while constructing `std.wayland` during Stage 0/1. Ordinary applications only use:

```zin
const wl = @import("std.wayland")
```

The compiler has no Wayland/XML special case. `std.xml`, the Wayland protocol parser, generated declarations, client/server runtime and OS transports are Zin code.
