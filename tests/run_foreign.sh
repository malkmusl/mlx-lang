#!/usr/bin/env bash
# Runs the foreign-function (extern "c" / export fn) tests with the
# self-hosted compiler. They produce dynamically linked executables and need
# the system's glibc dynamic linker.
#
#   tests/run_foreign.sh [compiler]     (default: mlx-out/bin/compiler/mlx4,
#                                        then zig-out/bin/mlx1)
#
# Every test exits 13 on success. Run from any directory.
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-}}
if [[ -z "$compiler" ]]; then
    for candidate in mlx-out/bin/compiler/mlx4 zig-out/bin/mlx1; do
        if [[ -x "$candidate" ]]; then compiler=$candidate; break; fi
    done
fi
if [[ -z "$compiler" || ! -x "$compiler" ]]; then
    echo "run_foreign.sh: no Mlx compiler found (build mlx1 with 'zig build mlx1')" >&2
    exit 1
fi

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
failures=0
for test in tests/247_*.mlx; do
    if ! "$compiler" --quiet "$test" -o "$work/test" 2> "$work/errors"; then
        echo "FAIL (compile) $test"
        cat "$work/errors"
        failures=$((failures + 1))
        continue
    fi
    set +e
    timeout 20 "$work/test" > "$work/output" 2>&1
    status=$?
    set -e
    if [[ $status -ne 13 ]]; then
        echo "FAIL (exit $status) $test"
        cat "$work/output"
        failures=$((failures + 1))
    else
        echo "ok   $test"
    fi
done

# A shared object (--shared): ET_DYN, loaded twice (two copies) by the test.
if "$compiler" --quiet --shared tests/support/shared_object_library.mlx -o "$work/libshared.so" 2> "$work/errors" \
    && "$compiler" --quiet tests/271_shared_object_runtime.mlx -o "$work/test" 2>> "$work/errors"; then
    cp "$work/libshared.so" "$work/libshared-copy.so"
    set +e
    timeout 20 "$work/test" "$work/libshared.so" "$work/libshared-copy.so" > "$work/output" 2>&1
    status=$?
    set -e
    if [[ "$(od -An -tx1 -j16 -N2 "$work/libshared.so" | tr -d ' ')" != "0300" ]]; then
        echo "FAIL --shared did not write an ET_DYN object"
        failures=$((failures + 1))
    elif [[ $status -ne 13 ]]; then
        echo "FAIL (exit $status) tests/271_shared_object_runtime.mlx"
        cat "$work/output"
        failures=$((failures + 1))
    else
        echo "ok   tests/271_shared_object_runtime.mlx"
    fi
else
    echo "FAIL (compile) tests/271_shared_object_runtime.mlx"
    cat "$work/errors"
    failures=$((failures + 1))
fi

# A plugin (--plugin): a shared object a C host loads (python3's ctypes),
# called on four threads at once, many times: each call has an arena of its
# own, given back on return (the process does not grow).
if ! command -v python3 > /dev/null; then
    echo "skip --plugin (no python3)"
elif "$compiler" --quiet --plugin tests/support/plugin_library.mlx -o "$work/libplugin.so" 2> "$work/errors"; then
    if python3 - "$work/libplugin.so" > "$work/output" 2>&1 <<'PY'
import ctypes, sys, threading
library = ctypes.CDLL(sys.argv[1])
library.mlx_plugin_sum.restype = ctypes.c_int64
library.mlx_plugin_sum.argtypes = [ctypes.c_int64]
library.mlx_plugin_twice.restype = ctypes.c_int64
library.mlx_plugin_twice.argtypes = [ctypes.c_int64]
def resident():
    return int(open('/proc/self/statm').read().split()[1])
failures = []
def work():
    for _ in range(2000):
        if library.mlx_plugin_sum(1000) != 4 * 999 * 1000 // 2:
            failures.append('sum')
        if library.mlx_plugin_twice(21) != 42:
            failures.append('twice')
before = resident()
threads = [threading.Thread(target=work) for _ in range(4)]
for thread in threads: thread.start()
for thread in threads: thread.join()
grown = resident() - before
assert not failures, failures[:3]
assert grown < 4096, grown
PY
    then
        echo "ok   --plugin: a C host calls its exports on four threads, each call on its own arena"
    else
        echo "FAIL --plugin"
        cat "$work/output"
        failures=$((failures + 1))
    fi
else
    echo "FAIL (compile) tests/support/plugin_library.mlx"
    cat "$work/errors"
    failures=$((failures + 1))
fi

# A program without foreign or exported functions stays a static executable.
"$compiler" --quiet tests/07_functions.mlx -o "$work/static"
if [[ "$(head -c 20 "$work/static" | od -An -tx1 -j16 -N2 | tr -d ' ')" != "0200" ]] || grep -q "ld-linux" "$work/static"; then
    echo "FAIL static output changed for programs without foreign functions"
    failures=$((failures + 1))
else
    echo "ok   static executables stay static"
fi

if [[ $failures -ne 0 ]]; then
    echo "$failures failure(s)"
    exit 1
fi
