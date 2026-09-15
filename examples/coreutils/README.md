# MLX coreutils

Dependency-free Linux userland programs written in MLX. Each utility lives in
its own directory and imports only the MLX standard library plus the shared
helpers in `common.mlx`.

Initial utilities:

- `true`, `false`: minimal process/exit-status programs
- `echo`: arguments and `-n`
- `cat`: stdin, files, `-`, `--`, multiple operands, and streaming I/O
- `wc`: line, word, and byte counts with `-l`, `-w`, and `-c`
- `pwd`: logical and physical paths with `-L` and `-P`
- `mkdir`: octal modes, parent creation, and verbose output
- `rmdir`: parent removal, non-empty handling, and verbose output
- `basename`: suffix removal, multiple operands, and NUL-delimited output
- `dirname`: multiple paths and newline- or NUL-delimited output
- `head`: byte or line limits, multiple-file headers, and NUL records
- `tail`: backward regular-file scans plus buffered stdin for bytes or records
- `tee`: streaming fan-out to stdout and multiple truncate/append targets
- `yes`: block-buffered repeated output for one or more strings
- `sleep`: decimal durations, unit suffixes, and summed operands
- `uname`: kernel, node, release, architecture, and operating-system fields
- `printenv`: complete environment iteration, selected variables, and NUL output
- `env`: environment clearing, unsetting, assignments, and `execve` with `PATH` lookup
- `nproc`: affinity-aware available CPUs, configured CPUs, and ignored processors
- `link`, `unlink`: dependency-free hard-link creation and single-path removal
- `touch`: current-time timestamp updates, file creation, and no-create mode
- `truncate`: exact file resizing with binary size suffixes and no-create mode
- `mkfifo`: named-pipe creation with octal permission modes
- `sync`: global, per-file data, and per-filesystem cache synchronization

Build all utilities with the self-hosted compiler:

```sh
./examples/coreutils/build.sh
./examples/coreutils/test.sh
./examples/coreutils/benchmark.sh
```

The build script defaults to the converged self-hosted compiler at
`mlx-out/bin/compiler/mlx4` and writes the utilities to
`mlx-out/bin/coreutils`. It never invokes the bootstrap compiler.
The benchmark creates a 128 MiB text workload when no fixture is supplied and
compares every hot path directly with the installed GNU utility.

`MLX_COMPILER` and `MLX_COREUTILS_BIN_DIR` override the build inputs and output
directory. Benchmark iteration counts can be changed through
`MLX_BENCH_STARTUP_ITERATIONS`, `MLX_BENCH_IO_ITERATIONS`, and
`MLX_BENCH_SCAN_ITERATIONS`.

These are intentionally small first implementations, not complete GNU-compatible
replacements yet. Unsupported GNU flags are tracked as future work while the
examples grow the compiler and standard library from real userland requirements.
