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

Build all utilities with the self-hosted compiler:

```sh
./examples/coreutils/build.sh ./zig-out/bin/mlx2
./examples/coreutils/test.sh
./examples/coreutils/benchmark.sh
```

The build script defaults to `mlx2` and never invokes the bootstrap compiler.
The benchmark creates a 128 MiB text workload when no fixture is supplied and
compares every hot path directly with the installed GNU utility.

`MLX_COMPILER` and `MLX_COREUTILS_BIN_DIR` override the build inputs and output
directory. Benchmark iteration counts can be changed through
`MLX_BENCH_STARTUP_ITERATIONS`, `MLX_BENCH_IO_ITERATIONS`, and
`MLX_BENCH_SCAN_ITERATIONS`.

These are intentionally small first implementations, not complete GNU-compatible
replacements yet. Unsupported GNU flags are tracked as future work while the
examples grow the compiler and standard library from real userland requirements.
