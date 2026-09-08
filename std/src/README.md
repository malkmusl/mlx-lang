# Mlx standard library

`std.mlx` is the Stage-1 public facade. It currently exposes the complete
bootstrap foundation, including `std.io.print`, `std.io.println`, raw streams,
formatting primitives, files, allocation and collections. The remaining full
standard library is implemented here as Mlx becomes capable of compiling it.

Native code uses `std.os.*`; portable POSIX software uses `std.posix`; Wayland
uses `std.wayland` plus generated protocol modules.
