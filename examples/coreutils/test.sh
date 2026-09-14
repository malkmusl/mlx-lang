#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/../.." && pwd)
bin_dir=${MLX_COREUTILS_BIN_DIR:-"$repo_root/zig-out/bin"}
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

"$bin_dir/mlx-true" || fail 'true returned a failure status'
if "$bin_dir/mlx-false"; then
    fail 'false returned success'
elif [[ $? -ne 1 ]]; then
    fail 'false did not return status 1'
fi

"$bin_dir/mlx-echo" alpha beta > "$work_dir/actual"
/usr/bin/echo alpha beta > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'echo output differs'

"$bin_dir/mlx-echo" -n alpha beta > "$work_dir/actual"
/usr/bin/echo -n alpha beta > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'echo -n output differs'

"$bin_dir/mlx-echo" -- alpha > "$work_dir/actual"
/usr/bin/echo -- alpha > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'echo -- output differs'

printf 'alpha beta\ngamma\n' > "$work_dir/a"
printf 'delta\n' > "$work_dir/b"
"$bin_dir/mlx-cat" "$work_dir/a" "$work_dir/b" > "$work_dir/actual"
/usr/bin/cat "$work_dir/a" "$work_dir/b" > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'cat file output differs'

printf 'middle\n' | "$bin_dir/mlx-cat" "$work_dir/a" - "$work_dir/b" > "$work_dir/actual"
printf 'alpha beta\ngamma\nmiddle\ndelta\n' > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'cat stdin output differs'

printf 'stdin through --\n' | "$bin_dir/mlx-cat" -- > "$work_dir/actual"
printf 'stdin through --\n' > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'cat -- stdin output differs'

"$bin_dir/mlx-wc" "$work_dir/a" > "$work_dir/actual"
read -r lines words bytes name < "$work_dir/actual"
[[ "$lines" == 2 && "$words" == 3 && "$bytes" == 17 && "$name" == "$work_dir/a" ]] || \
    fail 'wc default counts differ'

printf 'one two\nthree' | "$bin_dir/mlx-wc" -l -w -c > "$work_dir/actual"
read -r lines words bytes < "$work_dir/actual"
[[ "$lines" == 1 && "$words" == 3 && "$bytes" == 13 ]] || fail 'wc stdin counts differ'

"$bin_dir/mlx-wc" -c "$work_dir/a" "$work_dir/b" > "$work_dir/actual"
read -r bytes name < <(tail -n 1 "$work_dir/actual")
[[ "$bytes" == 23 && "$name" == total ]] || fail 'wc total differs'

printf 'all coreutils smoke tests passed\n'
