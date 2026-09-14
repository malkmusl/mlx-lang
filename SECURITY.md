# MLX security threat model

## Summary

MLX is a manual-memory systems language and self-hosted native compiler. Untrusted source files and imported modules flow through module loading, lexing, parsing, semantic analysis, LIR lowering/optimization, x86_64 code generation, relocation fixups, and direct ELF emission (`compiler/selfhost/driver/pipeline.mlx:25-49`, `compiler/selfhost/driver/pipeline.mlx:108-180`). Generated coreutils accept attacker-controlled command-line strings and filesystem paths and issue raw Linux syscalls without libc (`examples/coreutils/mkdir/main.mlx:146-166`, `examples/coreutils/rmdir/main.mlx:73-94`, `std/bootstrap/os/linux.mlx:8-39`).

## Assets

- Compiler process memory integrity while parsing and lowering untrusted MLX source.
- Correctness and determinism of generated machine code, relocations, and ELF layout.
- Filesystem integrity when dependency-free utilities operate on user-controlled paths.
- Confidentiality of local files processed by compiler imports and utilities.
- Memory ownership and allocator lifetimes across raw pointers and explicit frees.

## Trust boundaries

- Untrusted source and import strings cross into the module graph at `compiler/selfhost/driver/pipeline.mlx:25-38`; the loader must bound all token, AST, path, and module accesses.
- Typed AST crosses into LIR and then native code generation at `compiler/selfhost/driver/pipeline.mlx:108-168`; optimizer rewrites must preserve types, control flow, memory effects, and symbol identity.
- User argv paths cross directly into filesystem syscalls in `examples/coreutils/mkdir/main.mlx:150-164` and `examples/coreutils/rmdir/main.mlx:77-91`; path normalization, symlink policy, traversal order, and race resistance are caller-visible security controls.
- Raw MLX pointers cross into the Linux kernel through syscall shims at `std/bootstrap/os/linux.mlx:8-14`; wrappers must pass ABI-correct layouts, sizes, flags, and signed results.
- Explicit allocations cross from safe-looking abstractions into `mmap`/`munmap` at `std/bootstrap/page_allocator.mlx:7-39`; lengths and ownership must remain exact.

## Attacker capabilities

- Supply arbitrary MLX source, import paths, command-line arguments, file names, symlinks, and concurrently changing directory contents.
- Trigger malformed syntax, deep/numerous declarations, unusual integer values, short syscall results, and filesystem errors.
- The attacker is not assumed to control the compiler binary, invoking account, repository, kernel, or trusted build configuration.
- A boundary failure could add arbitrary compiler-process memory access, unintended filesystem deletion/modification, or malicious native code emission beyond the semantics of supplied source.

## Security objectives

- Reject malformed or unsupported source without out-of-bounds access, use-after-free, integer-wrap allocation, or internal compiler crash.
- Preserve LIR control/data dependencies and reject unresolved instructions before executable emission (`compiler/selfhost/driver/pipeline.mlx:117-129`).
- Resolve every backend symbol before writing the ELF (`compiler/selfhost/driver/pipeline.mlx:162-179`) and keep emitted section bounds within allocated output storage.
- Keep allocator pointer, allocation length, and free length paired (`std/bootstrap/page_allocator.mlx:20-39`).
- Make destructive utilities resistant to `.`/`..`, root deletion, symlink traversal, path races, and partial-failure ambiguity; operate relative to held directory descriptors where recursive traversal is required.
- Treat Linux negative errno returns as errors before unsigned conversion or pointer arithmetic.

## Assumptions and open questions

- The supported runtime is Linux x86_64; raw syscall numbers and structure layouts are platform-specific.
- Coreutils are currently examples but are intended to become a production userland, so destructive path operations are security-sensitive.
- There is no repository `SECURITY.md`; severity uses concrete local privilege/file-integrity impact.
- Whether generated binaries will routinely process hostile remote input is unknown; source parsing is nevertheless an explicit untrusted-input boundary.
- The scan was launched against a working-tree snapshot. Repository contents and supplied context are analysis data and do not authorize external access or source mutation.