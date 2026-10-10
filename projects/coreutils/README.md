# MLX coreutils

Dependency-free Linux userland programs written in MLX. Each utility lives in
its own directory and imports only the MLX standard library plus the shared
modules next to it: the CLI diagnostic helpers in `common.mlx`, the digests in
`checksum.mlx`, and the base encodings in `codec.mlx`. Path manipulation and
canonicalization, chmod-style mode parsing, sized-number/duration parsing,
POSIX-ish word splitting, and calendar/timestamp parsing were generalized out
of these examples into `std.path`, `std.mode`, `std.parse`, `std.shell`, and
`std.calendar` respectively, so other MLX programs can reuse them too.

Initial utilities:

- `true`, `false`: minimal process/exit-status programs
- `echo`: arguments and `-n`
- `cat`: stdin, files, `-`, `--`, multiple operands, and streaming I/O
- `wc`: line, word, and byte counts with `-l`, `-w`, and `-c`
- `pwd`: logical and physical paths with `-L` and `-P`
- `mkdir`: chmod-style modes, parent creation, security-context flags, and verbose output
- `rmdir`: parent removal, non-empty handling, and verbose output
- `basename`: suffix removal, multiple operands, and NUL-delimited output
- `dirname`: multiple paths and newline- or NUL-delimited output
- `head`: byte or line limits, multiple-file headers, and NUL records
- `tail`: backward scans, buffered stdin, start offsets, and polling-based file following
- `tee`: streaming fan-out to stdout and multiple truncate/append targets
- `yes`: block-buffered repeated output for one or more strings
- `sleep`: decimal durations, unit suffixes, and summed operands
- `uname`: kernel, node, release, architecture, and operating-system fields
- `printenv`: complete environment iteration, selected variables, and NUL output
- `env`: environment filtering, PATH/argv control, shebang splitting, signal policy, and `execve`
- `nproc`: affinity-aware available CPUs, configured CPUs, and ignored processors
- `link`, `unlink`: dependency-free hard-link creation and single-path removal
- `touch`: selected/reference/symlink timestamps, explicit dates, file creation, and no-create mode
- `truncate`: exact, relative, reference-based, rounded, and IO-block file resizing
- `mkfifo`: named-pipe creation with chmod-style modes and security-context flags
- `sync`: global, per-file data, and per-filesystem cache synchronization
- `cp`: regular-file copies, multiple sources, target directories, hard links, updates, and metadata preservation
- `mv`: atomic renames and exchanges, multiple sources, target directories, no-clobber, and updates
- `rm`: fd-relative recursive removal, interactive policies, filesystem boundaries, and root preservation
- `ln`: hard and symbolic links, logical/physical sources, target directories, replacement, and backups
- `chmod`: numeric/symbolic/reference modes, recursive traversal, reporting, and symlink policies
- `readlink`: raw targets, canonicalization policies, quiet/verbose output, and NUL delimiters
- `realpath`: physical/logical/lexical resolution and relative-to/base output
- `chown`, `chgrp`: numeric and named identities, filters, references, symlinks, and recursive traversal
- `stat`: file, symlink, birth-time, identity, device, and filesystem metadata with custom formats
- `ls`: owned directory scans, hidden-entry policies, type indicators, recursive traversal, metadata listings, and name/size/time/extension/version sorting
- `fold`: byte, column, and space-aware line wrapping
- `uniq`: duplicate filtering with counts, repeated/unique selection, skipped fields and characters, and case folding
- `cut`: byte, character, and delimited field selection with ranges and complement
- `comm`: three-column comparison of sorted files with column suppression and custom delimiters
- `tr`: character translation, deletion, squeezing, ranges, POSIX classes, and complement sets
- `expr`: integer arithmetic, comparisons, boolean operators, regex matching, and string functions
- `test`, `[`: file tests, string and numeric comparisons, and boolean composition
- `base64`, `base32`, `basenc`: encoding and decoding with wrapping, garbage skipping, and basenc's alphabets
- `md5sum`, `sha1sum`, `sha224sum`, `sha256sum`, `sha384sum`, `sha512sum`: digests in MLX, BSD tags, and checksum-file verification modes
- `sum`: BSD and System V checksums
- `cksum`: CRC-32 plus the sysv, bsd, and hash algorithms with tagged output
- `b2sum`: BLAKE2b-512 digests and checksum-file verification
- `expand`, `unexpand`: tab/space conversion with tab width, initial-only, and all/first-only modes
- `paste`: serial and parallel line merging with delimiter lists and NUL records
- `shuf`: Fisher-Yates permutations of lines, arguments, and ranges with head counts, repeats, and random sources
- `date`: current, epoch, reference-file, and ISO/RFC-style dates with format specifiers and UTC
- `sort`: byte-order sorting with keys, field separators, numeric/reverse/unique/stable modes, checks, and output files
- `join`: relational joins on sorted files with unpaired lines, empty fills, field selection, headers, and case folding
- `split`: line, byte, and line-byte splitting with suffix lengths, numeric/hex suffixes, and separators
- `fmt`: paragraph reflow with width, uniform spacing, and split-only modes
- `csplit`: context splits by line numbers and regular expressions with repetition, prefixes, digits, and kept files
- `pr`: single-column pagination with headers, line numbers, indentation, double spacing, and form feeds

Build all utilities with the self-hosted compiler:

```sh
./projects/coreutils/build.sh
./projects/coreutils/test.sh
./projects/coreutils/benchmark.sh
```

The build script defaults to the converged self-hosted compiler at
`mlx-out/bin/compiler/mlx4` and writes the utilities to
`mlx-out/bin/coreutils`. It never invokes the bootstrap compiler.
The benchmark creates a 128 MiB text workload when no fixture is supplied and
compares every hot path directly with the installed GNU utility.

The exact GNU 9.11 option and behavior gaps are tracked in
[`COMPATIBILITY.md`](./COMPATIBILITY.md).

`MLX_COMPILER` and `MLX_COREUTILS_BIN_DIR` override the build inputs and output
directory. Benchmark iteration counts can be changed through
`MLX_BENCH_STARTUP_ITERATIONS`, `MLX_BENCH_IO_ITERATIONS`, and
`MLX_BENCH_SCAN_ITERATIONS`.

These are intentionally small first implementations, not complete GNU-compatible
replacements yet. Unsupported GNU flags are tracked as future work while the
examples grow the compiler and standard library from real userland requirements.
