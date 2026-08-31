# Bootstrap std

This directory contains the allocation-free foundation used to start the
self-hosted compiler. Every module must compile with `zin0`; it deliberately
uses only language features already covered by an end-to-end runtime test.

Implemented first layer:

- `mem.zin`: `copy`, `copyBackwards`, `set`, `zero`, and `eql` over raw byte pointers.
- `ascii.zin`: ASCII classification and case conversion needed by the lexer.
- `ranges.zin`: ordered range bounds and the first real consumer of native multiple returns.
- `root.zin`: stable bootstrap module facade.

The next layers are the allocator interface, slices/strings, minimal formatting,
Linux syscalls, ELF/file I/O, and the collections required by the compiler.
