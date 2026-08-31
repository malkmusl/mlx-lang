# Bootstrap std

This directory contains the explicit-allocation foundation used to start the
self-hosted compiler. Every module must compile with `zin0`; it deliberately
uses only language features already covered by an end-to-end runtime test.

Implemented Stage-1 bootstrap layer:

- `mem.zin`: `copy`, `copyBackwards`, `set`, `zero`, and `eql` over raw byte pointers.
- `ascii.zin`: ASCII classification and case conversion needed by the lexer.
- `ranges.zin`: ordered range bounds and the first real consumer of native multiple returns.
- `os/linux.zin`: direct Linux x86_64 syscall gateway without libc.
- `allocator.zin`, `page_allocator.zin`, `fixed_buffer_allocator.zin`, and `arena_allocator.zin`: explicit allocation, scratch allocation, reset, and ownership.
- `string.zin`: borrowed byte strings, equality, slicing, ordering, prefixes, suffixes, and hashing.
- `array_list.zin`: growing byte list used for source and output buffers.
- `vector.zin`: type-erased growing storage for Zin1 token and AST records.
- `hash_map.zin`: growing `String -> usize` map for compiler symbol tables.
- `fmt.zin`: allocation-free unsigned decimal and hexadecimal formatting primitives.
- `fs.zin` and `io.zin`: raw file operations, complete reads/writes, and standard streams.
- `process.zin`: Linux process arguments exposed as borrowed bootstrap strings.
- `elf.zin`: complete in-memory and on-disk ELF64 executable emission.
- `root.zin`: stable facade exporting the complete bootstrap layer.

The byte list and hash map are deliberately concrete bootstrap specializations.
The self-hosted compiler can genericize them once Zin1's comptime pipeline is
available; no compiler magic or hidden allocation is involved.
