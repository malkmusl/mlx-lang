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
  (`PF_R | PF_X`; the string literals live in the code); a second `PT_LOAD`
  segment for `.data` (`PF_R | PF_W`), on the page after the code, is added
  only when the program has a data area (module-level `var` globals, see
  [Globals](#globals) below): `elf64.dataOffsetAfterCode` names its place
  and the pipeline resolves the code's RIP-relative references against it
  before writing.
- **Section headers:** a `SHT_NULL` entry, a `.text` `SHT_PROGBITS` section
  (`SHF_ALLOC | SHF_EXECINSTR`), an optional `.data` `SHT_PROGBITS` section
  (`SHF_ALLOC | SHF_WRITE`) when there is a data area, and a trailing
  `.shstrtab` `SHT_STRTAB` section. `num_shdrs` is 3 or 4 depending on
  whether `.data` is present — there is no `.rodata` or `.bss` section
  written by this function (`.symtab` and `.strtab` are appended afterwards,
  see below), so this writer covers the "directly executable static ELF"
  requirement rather than the spec's full required-section list.
- **`.shstrtab` contents:** the section name string table is hand-written
  byte-by-byte as the literal string `"\0.text\0.data\0.shstrtab\0"`
  (23 bytes, matching `shstrtab_size`), rather than built from a general
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

## Dynamically linked executables

A program that imports C functions (see
[ABI](abi.md#c-functions-externc-and-export-fn)) is written by
`compiler/selfhost/object/elf64_dynamic.mlx` instead: still `ET_EXEC` at the
same fixed address, but loaded by the system dynamic linker
(`PT_INTERP /lib64/ld-linux-x86-64.so.2`). It has

- an `R` segment with the headers, `.interp`, `.hash` (one bucket),
  `.dynsym`, `.dynstr` and `.rela.dyn`;
- the `R+X` `.text` segment, at the static layout's address;
- an `R+W` segment with `.dynamic`, `.got` (one slot per import, filled by an
  `R_X86_64_GLOB_DAT` relocation) and `.data` (the aggregate-arena words
  `_start` records for exported functions, then the globals, see
  [Globals](#globals));
- `PT_DYNAMIC`, `PT_PHDR` and a non-executable `PT_GNU_STACK`.

Imports are unversioned undefined symbols, bound immediately (`DF_BIND_NOW`,
`DF_1_NOW`) to the default version in any `DT_NEEDED` library: always
`libc.so.6`, plus each library named with `--library NAME` on the command
line. Exports are global `STT_FUNC` symbols in `.text`. A program without
imports is still the static executable described above, exports or not:
its exports are C-ABI functions only (signal handlers, which the kernel
calls; `std.crash` installs one in every desktop program), so nothing
there needs glibc. `tests/run_foreign.sh` checks that, and runs
`tests/247_foreign_c_runtime.mlx`.

## Symbol table and line table

After an executable is written, `compiler/selfhost/object/symbol_table.mlx`
adds three sections to it:

- `.symtab` and `.strtab`: every function named where it is declared,
  `path:line:name` (the module's path as the compiler loaded it, the line
  of the function's name), next to the runtime's own `_start` and
  `__mlx_*` routines. `nm`, `gdb` and `perf` show these names.
- `.mlx_lines`: where the machine code of each source line starts. It is
  `"MLXLINES"`, the number of entries and of files (u32 each), then per
  entry the code offset from `.text`, the file and the line (u32 each,
  lines from 1), then the files' paths, each ended by a NUL.

The crash handler of the desktop programs
(`std/src/crash.mlx`) reads both from `/proc/self/exe`
and writes each frame as `path:line:name+0xOFFSET at path:line`;
`tools/check_crash_report.sh` crashes `tests/support/crash_report.mlx`
three ways and checks the lines it names. The sampling profiler
`mlx-profile` (`tools/profile/`) names the functions it samples the same
way; `tools/check_profile.sh` profiles `tests/support/profile_busy.mlx`.

## Shared objects

`mlx4 --shared FILE -o libNAME.so` writes the same image as a shared object
for `dlopen` (`ET_DYN`, linked at address 0; the same writer,
`elf64_dynamic.writeSharedObject`). The x86_64 code needs no relocations of
its own: every reference to code, string literals, the GOT and the data area
is RIP-relative, so it runs wherever the dynamic linker maps it. There is no
`_start` (the entry is 0) and no `main` is needed; the exported functions
are the object's symbols, found with `dlsym`. Imports work as in an
executable (GOT slots with `R_X86_64_GLOB_DAT`, `libc.so.6` needed).

A shared object has no aggregate arena of its own: its exported functions
keep the caller's (see [ABI](abi.md#c-functions-externc-and-export-fn)), so
it is meant to be loaded by an Mlx program and called on its threads, its
functions and the functions it hands out as pointers alike.

`mlx4 --plugin` writes the same kind of object for a host that is not an
Mlx program (an OBS plugin): each exported function, on entry, reserves an
aggregate arena of its own (256 MiB of address space, `MAP_NORESERVE`:
only the pages it touches count) and unmaps it on return, keeping its
results. Calls on several of the host's threads at once each have their
own, and a host calling for hours does not grow the process. What has to
outlive a call goes in memory from an allocator; functions handed to the
host as callbacks are `export fn` too. `tests/run_foreign.sh` loads
`tests/support/plugin_library.mlx` from python3 (ctypes) and calls it on
four threads at once; `projects/desktop/capture/obs.mlx` is such a plugin.

`--stack-arena[=BYTES]` (with `--plugin`; 65536 when no size is given,
rounded up to a multiple of 16) makes each export take its arena from its
own stack instead: the prologue moves `rsp` down by that many bytes and
points `r14`/`r15` at the reservation, the epilogue's `leave` gives it
back, so a call costs no system call (what a C library's `strlen` or
`malloc` written in Mlx needs). The host's threads must have that much
stack to spare below the export's frame (the usual 8 MiB thread stacks
have); an export whose aggregates outgrow the reservation traps, as any
exhausted arena does. `--no-libc` leaves `libc.so.6` out of `DT_NEEDED`,
which then names only the `--library` libraries (none, for an image that
imports nothing; imports, if there are any, bind against whatever the
loading process has). `tests/support/plugin_freestanding.mlx`, built with
both and checked by `tests/run_foreign.sh` (no `NEEDED` entry; under
`strace`, no mapping per call), is the shape of mlxlibc: `projects/libc`
(its [README](../../projects/libc/README.md)), built by
`tools/build_mlxlibc.sh` as `libmlxc.so.1` with no `DT_NEEDED` at all and
checked against glibc by `tools/check_mlxlibc.sh`.
`tests/271_shared_object_runtime.mlx` (run by `tests/run_foreign.sh`) loads
`tests/support/shared_object_library.mlx` twice from two copies, calls it
through `dlsym` and through function pointers it returns, and checks that
the structs it builds grow the caller's arena by exactly their size.
`projects/desktop/compositor` loads its shell and renderer this way, again
whenever they are rebuilt.

A module-level `export const NAME: T = value` of an integer type is a data
symbol of the image (`STT_OBJECT`, in `.data`, its size that of `T`): the
dynamic linker binds C references to such a variable (`extern enum
spa_log_level pw_log_level` in PipeWire's headers) to a word holding the
value, which C code reads and may write. To Mlx the constant stays a
compile-time value; a library that has to read the word as C sees it finds
it through the dynamic linker (`tests/support/plugin_library.mlx`,
`mlx_plugin_level_now`; `projects/desktop/libpipewire` exports
`pw_log_level` and `PW_LOG_TOPIC_DEFAULT` this way). A dynamically linked
executable gets the symbol too; a static one has no dynamic symbols.

## Globals

A module-level `var NAME: T = value` is a slot of the image's data area:
one per global, after the area's fixed words (`DATA_SIZE`, the arena
pointers), each at its type's alignment with its type's size (at least 8
bytes), in the order the lowering first meets them. The code reaches a slot
RIP-relative (`lea`, a `data_rel32` fixup resolved against the area's place,
so the same code runs in a static executable, a dynamic one, a `--shared`
object and a `--plugin`); loads and stores go through that address like any
other memory, and `&NAME` is it. An integer or bool initializer (comptime
evaluable, as `spec/00-language/modules.xml` asks) writes the slot's
initial bytes, and so does an array or struct literal of such values
(element by element at the type's layout, `writeInitialBytes` in
`ir/lower.mlx`; `var table: [3]u16 = [3]u16{ 7, 8, 9 }`); `undefined` and
`null` leave it zeroed. Other initializers (strings, floats) are not
lowered yet and are reported with the module's position. A static
executable with globals gets the `.data` segment described above; a dynamic
image keeps them in its `R+W` segment.

An `export var` is such a slot and a data symbol too (`STT_OBJECT`, its size
that of `T`, in `.dynsym`): C code reads and writes the same word the Mlx
code does (`tests/support/plugin_library.mlx`, `mlx_plugin_calls`, checked
from python3 by `tests/run_foreign.sh`). In a `--shared` or `--plugin`
object the Mlx code itself reaches an `export var` through a GOT slot
(`data_got_rel32`: `mov reg, [rip + slot]`, the slot after the imports'
with an `R_X86_64_GLOB_DAT` against the variable's own symbol), because a
C program linked against the object may hold its own copy of the variable
(`R_X86_64_COPY`, which gcc emits for `optind`, `optarg` or `stdout`): the
dynamic linker resolves the symbol to that copy, and through the slot the
library's `getopt` writes where the program reads (`tools/check_mlxlibc.sh`
checks it with a C program). `tests/297_module_globals_runtime.mlx`
covers the static image: scalars with initial values, aggregates, a slice
over an array global, a pointer global, and another module's `pub var`
globals read and written as `module.name` (`tests/support/module_globals.mlx`).
On aarch64 a global is a local data symbol reached with `adrp`/`add`
(`backend/aarch64/encoder.mlx`, `defineGlobal`).

`--soname=NAME` gives either kind its `DT_SONAME`, and
`--symbol-version=NAME` a version definition (`.gnu.version_d`: the base
version, named after the soname, and NAME) with every export at NAME and
every import unversioned (`.gnu.version`, `DT_VERSYM`, `DT_VERDEF`,
`DT_VERDEFNUM`). A library standing in for another needs both: programs
linked against a versioned library ask for its symbols at their version,
and glibc's loader stops a program whose versioned reference lands on an
unversioned definition. `projects/desktop/libpulse` builds
`libpulse.so.0` with `--soname=libpulse.so.0 --symbol-version=PULSE_0`,
as PulseAudio's. `tests/run_foreign.sh` finds a plugin's export at its
version with `dlvsym` (and not at another).

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
