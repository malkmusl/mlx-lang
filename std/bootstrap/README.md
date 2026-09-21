# Bootstrap std

This directory contains the explicit-allocation foundation used to start the
self-hosted compiler. Every module must compile with `mlx0`; it deliberately
uses only language features already covered by an end-to-end runtime test.

Implemented Stage-1 compiler-core std:

- `mem.mlx`: `copy`, `copyBackwards`, `set`, `zero`, and `eql` over raw byte pointers.
- `ascii.mlx`: ASCII classification and case conversion needed by the lexer.
- `ranges.mlx`: ordered range bounds and the first real consumer of native multiple returns.
- `os/linux.mlx`: direct Linux x86_64 syscall gateway without libc.
- `allocator.mlx`, `page_allocator.mlx`, `fixed_buffer_allocator.mlx`, and `arena_allocator.mlx`: explicit allocation, scratch allocation, reset, and ownership.
  `Allocator` follows Zig's `std.mem.Allocator` shape: a `ptr: *anyopaque` context plus a
  `vtable: *const VTable` (`alloc`/`resize`/`remap`/`free`) that each implementation builds once as
  a static table, rather than storing per-instance function pointers. It keeps the pointer/length
  convenience methods (`alloc`, `resize`, `free`) every other bootstrap module already called, and
  adds Zig-style typed helpers (`create`, `destroy`, `allocSlice`, `freeSlice`, `resizeSlice`,
  `dupe`) built on `comptime T: type` generics, returning `[*]T`/`*T` rather than `[]T` since Mlx
  slices carry no readable `.length`/`.pointer` accessor the way Zig's do.
- `string.mlx`: borrowed byte strings, equality, slicing, ordering, prefixes, suffixes, and hashing.
- `array_list.mlx`: growing byte list used for source and output buffers.
- `vector.mlx`: type-erased growing storage for Mlx1 token and AST records.
- `hash_map.mlx`: growing `String -> usize` map for compiler symbol tables.
- `fmt/` and `fmt.mlx`: allocation-free integer conversion plus a typed,
  fallible format writer for strings, booleans, characters, pointers and
  signed/unsigned integers. The bootstrap argument adapter keeps the runtime
  independent of unfinished generic reflection.
- `fs.mlx` and `io.mlx`: raw file operations, slice reads/writes, complete writes,
  standard streams, direct string/line output, allocation-free integer output,
  and formatted stdout/stderr output.
- `process.mlx`: Linux process arguments exposed as borrowed bootstrap strings.
- `elf.mlx`: complete in-memory and on-disk ELF64 executable emission.
- `root.mlx`: stable facade exporting the complete bootstrap layer.

The byte list and hash map are deliberately concrete bootstrap specializations.
The self-hosted compiler can genericize them once Mlx1's comptime pipeline is
available; no compiler magic or hidden allocation is involved.
