# Bootstrap std

This directory contains the explicit-allocation foundation used to start the
self-hosted compiler. Every module must compile with `zin0`; it deliberately
uses only language features already covered by an end-to-end runtime test.

Implemented Stage-1 bootstrap layer:

- `mem.zin`: `copy`, `copyBackwards`, `set`, `zero`, and `eql` over raw byte pointers.
- `ascii.zin`: ASCII classification and case conversion needed by the lexer.
- `ranges.zin`: ordered range bounds and the first real consumer of native multiple returns.
- `os/linux.zin`: direct Linux x86_64 syscall gateway without libc.
- `allocator.zin` and `page_allocator.zin`: explicit allocator interface and mmap-backed implementation.
- `string.zin`: borrowed byte strings, equality, prefixes, and hashing.
- `array_list.zin`: allocation-backed byte list used for source and output buffers.
- `hash_map.zin`: allocation-backed `String -> usize` map for compiler symbol tables.
- `fmt.zin`: allocation-free unsigned decimal and hexadecimal formatting primitives.
- `fs.zin`: raw file open/read/write/seek/close/delete operations and `writeAll`.
- `elf.zin`: ELF64 executable/program-header serialization primitives.
- `root.zin`: stable facade exporting the complete bootstrap layer.

The byte list and hash map are deliberately concrete bootstrap specializations.
The self-hosted compiler can genericize them once Zin1's comptime pipeline is
available; no compiler magic or hidden allocation is involved.
