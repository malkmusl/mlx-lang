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
const std = @import("std")
const wl = @import("std.wayland")

pub fn main() -> !void {
    var display: wl.client.Display = undefined
    try wl.client.Display.connect(&display, std.page_allocator.init())
    defer display.disconnect()

    const registry = wl.displayProxy(display).getRegistry()
    try display.roundtrip()

    while display.running() {
        try display.dispatch()
    }
}
```

Server/compositor code imports the same module and uses `wl.server`.

The Linux implementation is complete: the vendored canonical XML lives in
`std/protocols/wayland`, the materialized modules live in
`std/src/wayland/generated`, and the client and server run over native Unix
sockets with `SCM_RIGHTS` and memfd shared memory. They are wire-compatible
with libwayland in both directions. See
[`reference/wayland.md`](reference/wayland.md) for the pipeline, the runtime
and the tests. The BSD and brixOS transports come later in the
implementation order.

GPU rendering works the same way: the stable linux-dmabuf protocol is
materialized alongside the core and xdg-shell XML, so a Vulkan client
(`examples/vulkan-wayland-client`) hands its frames over as dma-bufs, and
the nested compositor (`examples/wayland-compositor --renderer vulkan`)
composes client buffers on the GPU. Vulkan itself is native too; see
[`VULKAN.md`](VULKAN.md).

For larger programs, see
[`examples/wayland-compositor`](../examples/wayland-compositor/README.md), a
nested compositor with keyboard and pointer input that launches clients,
and [`examples/wayland-terminal`](../examples/wayland-terminal/README.md), a
terminal emulator client.
