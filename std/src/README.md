# Mlx standard library

`std.mlx` is the Stage-1 public facade. It currently exposes the complete
bootstrap foundation, including raw streams, direct text output, typed
`std.fmt.print`/`std.io.printFmt`, files, allocation and collections. Bootstrap
format arguments use explicit constructors such as `std.fmt.string` and
`std.fmt.unsigned`; the normative `anytype` tuple adapter will replace that
surface once Mlx1 can instantiate reflection-heavy generics itself.

Native code uses `std.os.*`; portable POSIX software uses `std.posix`; Wayland
uses `std.wayland` plus generated protocol modules.

`xml.mlx` (`std.xml`) is the allocation-free XML pull tokenizer.
`wayland.mlx` (`std.wayland`) and `wayland/` contain the protocol parser,
the materializer, the wire format, the Linux transport, and the client and
server runtimes. `wayland/generated/` holds the modules materialized from
`std/protocols/wayland` and must not be edited by hand. Both are imported by
name and are not re-exported from `std.mlx`.

`json.mlx` (`std.json`) is the allocation-free JSON pull tokenizer.
`vulkan.mlx` (`std.vulkan`) is the Vulkan API, materialized from
`std/registry/vulkan/vk.xml` by `vulkan/registry.mlx` and
`vulkan/materialize.mlx` — do not edit it by hand. `vulkan/loader.mlx`
(`std.vulkan.loader`) finds and opens drivers without a C loader, and
`vulkan/icd.mlx` (`std.vulkan.icd`) is the driver side of that interface.
`spirv/core.mlx` (`std.spirv.core`, generated from
`std/registry/spirv/spirv.core.grammar.json`), `spirv/builder.mlx` and
`spirv/module.mlx` write, read and validate SPIR-V. See
`docs/reference/vulkan.md`.

`truetype.mlx` (`std.truetype`) reads TrueType fonts, rasterizes glyphs
with anti-aliasing, packs them into an atlas and lays out text; see
`docs/reference/truetype.md`.

`std.path`, `std.mode`, `std.parse`, `std.shell` and `std.calendar` provide
coreutils-grade building blocks (path manipulation/canonicalization, chmod-style
permission parsing, sized-number/duration parsing, POSIX-ish word splitting and
calendar/timestamp parsing) that were generalized out of `examples/coreutils`.
