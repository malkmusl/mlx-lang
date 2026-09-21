# Executable, object, and debug formats

**Normative source:** `spec/03-formats/elf64.xml`, `spec/03-formats/debug.xml`,
`spec/03-formats/pe32plus.xml`

**Cross-referenced implementation:** `compiler/selfhost/object/elf64.mlx`

Mlx emits native machine code and writes its own executable files directly —
the project does not depend on an external assembler or linker for its
reference path (root `README.md`: "Mlx emits native machine code directly and
does not require LLVM, GCC, a C compiler, libc, an external assembler, or an
external linker for its reference path"; "Core rules" repeats this as "direct
x86_64 machine-code generation"). `compiler/selfhost/object/elf64.mlx` is the
self-hosted compiler's own ELF64 writer and is the concrete embodiment of
that rule for the Linux target.

## ELF64 (normative shape)

`spec/03-formats/elf64.xml` is short and fixes the essentials for the
Linux/BSD x86_64 target:

```xml
<MlxELF64 version="1.0" normative="true">
  <Target>Linux/BSD x86_64 where ELF64 is used</Target>
  <Endian>little</Endian><Machine>EM_X86_64</Machine>
  <RequiredSections>.text .rodata .data .bss .symtab .strtab .shstrtab and relocation sections when needed</RequiredSections>
  <Executable>Integrated linker must be able to emit a directly executable static ELF for libc-free Mlx programs.</Executable>
  <DynamicLinking>May be added after static/bootstrap path; foreign shared-library loading is platform tooling, not core language semantics.</DynamicLinking>
</MlxELF64>
```

Little-endian, `EM_X86_64`, and a required section set including `.text`,
`.rodata`, `.data`, `.bss`, `.symtab`, `.strtab`, and `.shstrtab` (plus
relocation sections when needed). The spec requires that the integrated
linker "must be able to emit a directly executable static ELF for
libc-free Mlx programs" — dynamic linking is explicitly deferred as
optional, post-bootstrap tooling, not core language semantics.

## What `compiler/selfhost/object/elf64.mlx` actually emits

Reading the self-hosted writer (`compiler/selfhost/object/elf64.mlx`,
`buildExecutable`), it currently produces a minimal static, statically-linked
`ET_EXEC` image rather than the full required-section list above. Concretely,
from the code:

- **Layout constants:** load address `LOAD_VADDR = 4194304` (0x400000), code
  is placed at file offset `TEXT_FILE_OFFSET = 4096` (one page in), so
  `TEXT_VADDR = LOAD_VADDR + TEXT_FILE_OFFSET`, and all section/segment
  offsets are page-aligned (`PAGE_SIZE = 4096`) via a local `alignUp` helper.
- **ELF header:** writes the `\x7fELF` magic, `ELFCLASS64` (2),
  `ELFDATA2LSB` (1), `EV_CURRENT` (1), `e_type = ET_EXEC` (2), `e_machine =
  EM_X86_64` (62), `e_entry = TEXT_VADDR + entry_offset`, `e_phoff = 64`
  (program headers immediately follow the 64-byte ELF header), and
  `e_shoff` computed after code/rodata/`.shstrtab` are laid out.
- **Program headers:** always one `PT_LOAD` segment covering `.text`
  (`PF_R | PF_X`); a second `PT_LOAD` segment for `.rodata` (`PF_R` only) is
  added only `if has_rodata` (i.e., `rodata_length > 0`).
- **Section headers:** a `SHT_NULL` entry, a `.text` `SHT_PROGBITS` section
  (`SHF_ALLOC | SHF_EXECINSTR`), an optional `.rodata` `SHT_PROGBITS` section
  (`SHF_ALLOC`) when `has_rodata`, and a trailing `.shstrtab` `SHT_STRTAB`
  section. `num_shdrs` is 3 or 4 depending on whether `.rodata` is present —
  there is no `.data`, `.bss`, `.symtab`, or `.strtab` section written by
  this function, so this writer covers the "directly executable static ELF"
  requirement for code+rodata-only programs rather than the spec's full
  required-section list.
- **`.shstrtab` contents:** the section name string table is hand-written
  byte-by-byte as the literal string `"\0.text\0.rodata\0.shstrtab\0"`
  (25 bytes, matching `shstrtab_size`), rather than built from a general
  string-table builder.
- **File writing:** `writeExecutable` calls `buildExecutable` to produce an
  in-memory buffer, then opens the output path with
  `fs.WRITE_ONLY | fs.CREATE | fs.TRUNCATE` and mode `493` (`0o755`),
  writes the buffer out in a loop tolerant of short writes, closes the file,
  and calls `fs.chmod(output_path, 493)` to ensure the result is executable.

This confirms the writer is a self-contained, dependency-free ELF64 emitter
(no calls out to an external linker or assembler are present anywhere in the
file) producing a static, non-relocatable, non-PIE executable whose entry
point is `entry_offset` bytes into the `.text` segment loaded at a fixed
virtual address.

## Debug information

`spec/03-formats/debug.xml` specifies target-appropriate debug formats and a
minimum information set, without normatively defining Mlx's internal
producer implementation:

```xml
<MlxDebugInfo version="1.0" normative="true">
  <ELF>DWARF 5</ELF><Windows>CodeView; PDB supported by toolchain</Windows><Raw>optional Mlx symbol-map file</Raw>
  <MinimumInfo>source file, line, column, function, symbol, address range, representable locals, type names, struct fields, enum names, union variants</MinimumInfo>
  <PanicTrace>Debug and ReleaseSafe hosted builds provide stack traces when platform facilities permit. ReleaseFast may omit them.</PanicTrace>
  <NoExceptionUnwind>Panic stack tracing is diagnostic only; Mlx does not use exception unwinding as language control flow.</NoExceptionUnwind>
</MlxDebugInfo>
```

On ELF targets this means DWARF 5; on Windows, CodeView (with PDB generation
allowed but left to "the toolchain"); an Mlx-specific raw symbol-map file is
also permitted as an alternative. The minimum information a producer must be
able to represent covers source file/line/column, function and symbol names,
address ranges, representable locals, and type names including struct
fields/enum names/union variants. Panic stack traces are explicitly
diagnostic-only — the spec is careful to note Mlx "does not use exception
unwinding as language control flow," so unwind tables exist (where present)
purely to support debugging/panic traces, never `try`/error-union control
flow (see the guide's [Errors](../guide/07-errors.md) chapter for how error
propagation actually works, via `!T` return values, not unwinding).

## PE32+ (specified, not yet implemented)

`spec/03-formats/pe32plus.xml` normatively specifies a Windows PE32+ target:

```xml
<MlxPE32Plus version="1.0" normative="true">
  <Machine>AMD64</Machine><Format>PE32+</Format>
  <RequiredSections>.text .rdata .data .bss .pdata .reloc .idata as required</RequiredSections>
  <Imports>Windows system DLL imports use the PE import table.</Imports>
  <Debug>CodeView records are emitted when debug info is enabled; PDB generation may be implemented by the integrated toolchain.</Debug>
</MlxPE32Plus>
```

A grep across `compiler/` for `pe32`/`PE32` (case-insensitive) finds no
matches: there is no PE32+ writer analogous to
`compiler/selfhost/object/elf64.mlx`, and no reference to it in the Zig
bootstrap compiler either. This target is therefore currently
**specification-only** — normative in `spec/03-formats/pe32plus.xml`, but
with no implementation in either `compiler/selfhost/` or
`compiler/bootstrap/` as of this writing. This matches
`spec/05-build/build-system.xml`'s `InitialTargets`, which lists
`x86_64-windows` as a target the build system model must eventually support,
without implying every initial target already has a working object-format
backend.
