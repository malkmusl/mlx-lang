# Mlx standard library

`std.mlx` is the Stage-1 public facade. It currently exposes the complete
bootstrap foundation, including raw streams, direct text output, typed
`std.fmt.print`/`std.io.printFmt`, files, allocation and collections. Bootstrap
format arguments use explicit constructors such as `std.fmt.string` and
`std.fmt.unsigned`; the normative `anytype` tuple adapter will replace that
surface once Mlx1 can instantiate reflection-heavy generics itself.

Native code uses `std.os.*`; portable POSIX software uses `std.posix`; Wayland
uses `std.wayland` plus generated protocol modules.

`std.path`, `std.mode`, `std.parse`, `std.shell` and `std.calendar` provide
coreutils-grade building blocks (path manipulation/canonicalization, chmod-style
permission parsing, sized-number/duration parsing, POSIX-ish word splitting and
calendar/timestamp parsing) that were generalized out of `examples/coreutils`.
