#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/../.." && pwd)
bin_dir=${MLX_COREUTILS_BIN_DIR:-"$repo_root/mlx-out/bin/coreutils"}
fixture=${1:-/tmp/mlx-coreutils-benchmark.bin}

if [[ ! -f "$fixture" ]]; then
    set +o pipefail
    yes 'mlx coreutils benchmark data for word and line scanning' |
        dd of="$fixture" bs=1M count=128 iflag=fullblock status=none
    generation_status=${PIPESTATUS[1]}
    set -o pipefail
    [[ "$generation_status" -eq 0 ]] || exit "$generation_status"
fi

benchmark() {
    local iterations=$1
    local label=$2
    shift 2
    local started ended
    started=$(date +%s%N)
    for ((i = 0; i < iterations; i++)); do "$@"; done
    ended=$(date +%s%N)
    awk -v label="$label" -v elapsed="$((ended - started))" -v count="$iterations" \
        'BEGIN { printf "%-18s %9.3f ms/run\n", label, elapsed / count / 1000000 }'
}

throughput() {
    local iterations=$1
    local label=$2
    local bytes=$3
    shift 3
    local started ended
    started=$(date +%s%N)
    for ((i = 0; i < iterations; i++)); do "$@"; done
    ended=$(date +%s%N)
    awk -v label="$label" -v elapsed="$((ended - started))" -v count="$iterations" -v bytes="$bytes" \
        'BEGIN { printf "%-18s %9.3f ms/run  %8.1f MiB/s\n", label, elapsed / count / 1000000, bytes * count / 1048576 / (elapsed / 1000000000) }'
}

startup_iterations=${MLX_BENCH_STARTUP_ITERATIONS:-1000}
io_iterations=${MLX_BENCH_IO_ITERATIONS:-5}
scan_iterations=${MLX_BENCH_SCAN_ITERATIONS:-1}
fixture_bytes=$(stat -c %s "$fixture")
head_bytes=$((fixture_bytes < 1048576 ? fixture_bytes : 1048576))

/usr/bin/cat "$fixture" >/dev/null
benchmark "$startup_iterations" "system true" /usr/bin/true
benchmark "$startup_iterations" "mlx true" "$bin_dir/true"
benchmark "$startup_iterations" "system pwd" sh -c "/usr/bin/pwd >/dev/null"
benchmark "$startup_iterations" "mlx pwd" sh -c "'$bin_dir/pwd' >/dev/null"
benchmark "$startup_iterations" "system basename" sh -c "/usr/bin/basename /usr/bin/basename >/dev/null"
benchmark "$startup_iterations" "mlx basename" sh -c "'$bin_dir/basename' /usr/bin/basename >/dev/null"
benchmark "$startup_iterations" "system dirname" sh -c "/usr/bin/dirname /usr/bin/dirname >/dev/null"
benchmark "$startup_iterations" "mlx dirname" sh -c "'$bin_dir/dirname' /usr/bin/dirname >/dev/null"
benchmark "$startup_iterations" "system sleep 0" /usr/bin/sleep 0
benchmark "$startup_iterations" "mlx sleep 0" "$bin_dir/sleep" 0
benchmark "$startup_iterations" "system uname" sh -c "/usr/bin/uname >/dev/null"
benchmark "$startup_iterations" "mlx uname" sh -c "'$bin_dir/uname' >/dev/null"
benchmark "$startup_iterations" "system printenv" sh -c "ALPHA=one /usr/bin/printenv ALPHA >/dev/null"
benchmark "$startup_iterations" "mlx printenv" sh -c "ALPHA=one '$bin_dir/printenv' ALPHA >/dev/null"
benchmark "$startup_iterations" "system env" sh -c "/usr/bin/env -i ALPHA=one >/dev/null"
benchmark "$startup_iterations" "mlx env" sh -c "'$bin_dir/env' -i ALPHA=one >/dev/null"
throughput "$io_iterations" "system cat 128M" "$fixture_bytes" sh -c "/usr/bin/cat '$fixture' >/dev/null"
throughput "$io_iterations" "mlx cat 128M" "$fixture_bytes" sh -c "'$bin_dir/cat' '$fixture' >/dev/null"
throughput "$io_iterations" "system head 1M" "$head_bytes" sh -c "/usr/bin/head -c '$head_bytes' '$fixture' >/dev/null"
throughput "$io_iterations" "mlx head 1M" "$head_bytes" sh -c "'$bin_dir/head' -c '$head_bytes' '$fixture' >/dev/null"
throughput "$io_iterations" "system tail 1M" "$head_bytes" sh -c "/usr/bin/tail -c '$head_bytes' '$fixture' >/dev/null"
throughput "$io_iterations" "mlx tail 1M" "$head_bytes" sh -c "'$bin_dir/tail' -c '$head_bytes' '$fixture' >/dev/null"
throughput "$io_iterations" "system tee 128M" "$fixture_bytes" sh -c "/usr/bin/tee /dev/null < '$fixture' >/dev/null"
throughput "$io_iterations" "mlx tee 128M" "$fixture_bytes" sh -c "'$bin_dir/tee' /dev/null < '$fixture' >/dev/null"
throughput "$io_iterations" "system yes 128M" "$fixture_bytes" sh -c "/usr/bin/yes | /usr/bin/head -c '$fixture_bytes' >/dev/null"
throughput "$io_iterations" "mlx yes 128M" "$fixture_bytes" sh -c "'$bin_dir/yes' | /usr/bin/head -c '$fixture_bytes' >/dev/null"
throughput "$io_iterations" "system wc 128M" "$fixture_bytes" sh -c "/usr/bin/wc -c '$fixture' >/dev/null"
throughput "$io_iterations" "mlx wc 128M" "$fixture_bytes" sh -c "'$bin_dir/wc' -c '$fixture' >/dev/null"
throughput "$scan_iterations" "system wc default" "$fixture_bytes" sh -c "/usr/bin/wc '$fixture' >/dev/null"
throughput "$scan_iterations" "mlx wc default" "$fixture_bytes" sh -c "'$bin_dir/wc' '$fixture' >/dev/null"
