#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/../.." && pwd)
bin_dir=${MLX_COREUTILS_BIN_DIR:-"$repo_root/mlx-out/bin/coreutils"}
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

compare_wc() {
    local path=$1
    local mlx_lines mlx_words mlx_bytes mlx_name
    local gnu_lines gnu_words gnu_bytes gnu_name
    read -r mlx_lines mlx_words mlx_bytes mlx_name < <(LC_ALL=C "$bin_dir/wc" "$path")
    read -r gnu_lines gnu_words gnu_bytes gnu_name < <(LC_ALL=C /usr/bin/wc "$path")
    [[ "$mlx_lines" == "$gnu_lines" && "$mlx_words" == "$gnu_words" &&
       "$mlx_bytes" == "$gnu_bytes" && "$mlx_name" == "$gnu_name" ]]
}

compare_basename() {
    "$bin_dir/basename" "$@" > "$work_dir/actual"
    /usr/bin/basename "$@" > "$work_dir/expected"
    cmp "$work_dir/actual" "$work_dir/expected"
}

compare_dirname() {
    "$bin_dir/dirname" "$@" > "$work_dir/actual"
    /usr/bin/dirname "$@" > "$work_dir/expected"
    cmp "$work_dir/actual" "$work_dir/expected"
}

"$bin_dir/true" || fail 'true returned a failure status'
if "$bin_dir/false"; then
    fail 'false returned success'
elif [[ $? -ne 1 ]]; then
    fail 'false did not return status 1'
fi

"$bin_dir/echo" alpha beta > "$work_dir/actual"
/usr/bin/echo alpha beta > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'echo output differs'

"$bin_dir/echo" -n alpha beta > "$work_dir/actual"
/usr/bin/echo -n alpha beta > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'echo -n output differs'

"$bin_dir/echo" -- alpha > "$work_dir/actual"
/usr/bin/echo -- alpha > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'echo -- output differs'

printf 'alpha beta\ngamma\n' > "$work_dir/a"
printf 'delta\n' > "$work_dir/b"
"$bin_dir/cat" "$work_dir/a" "$work_dir/b" > "$work_dir/actual"
/usr/bin/cat "$work_dir/a" "$work_dir/b" > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'cat file output differs'

printf 'middle\n' | "$bin_dir/cat" "$work_dir/a" - "$work_dir/b" > "$work_dir/actual"
printf 'alpha beta\ngamma\nmiddle\ndelta\n' > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'cat stdin output differs'

printf 'stdin through --\n' | "$bin_dir/cat" -- > "$work_dir/actual"
printf 'stdin through --\n' > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'cat -- stdin output differs'

"$bin_dir/wc" "$work_dir/a" > "$work_dir/actual"
read -r lines words bytes name < "$work_dir/actual"
[[ "$lines" == 2 && "$words" == 3 && "$bytes" == 17 && "$name" == "$work_dir/a" ]] || \
    fail 'wc default counts differ'

printf 'one two\nthree' | "$bin_dir/wc" -l -w -c > "$work_dir/actual"
read -r lines words bytes < "$work_dir/actual"
[[ "$lines" == 1 && "$words" == 3 && "$bytes" == 13 ]] || fail 'wc stdin counts differ'

"$bin_dir/wc" -c "$work_dir/a" "$work_dir/b" > "$work_dir/actual"
read -r bytes name < <(tail -n 1 "$work_dir/actual")
[[ "$bytes" == 23 && "$name" == total ]] || fail 'wc total differs'

printf 'a b\tc\nd\ve\ff\rg\240h\n' > "$work_dir/whitespace"
compare_wc "$work_dir/whitespace" || fail 'wc whitespace classification differs'

dd if=/dev/urandom of="$work_dir/random" bs=64K count=1 status=none
compare_wc "$work_dir/random" || fail 'wc binary input counts differ'

"$bin_dir/pwd" -P > "$work_dir/actual"
/usr/bin/pwd -P > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'pwd -P output differs'

ln -s "$repo_root" "$work_dir/logical"
(
    cd "$work_dir/logical"
    PWD="$work_dir/logical" "$bin_dir/pwd" -L
) > "$work_dir/actual"
printf '%s/logical\n' "$work_dir" > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'pwd -L output differs'

(
    umask 000
    "$bin_dir/mkdir" -m 750 "$work_dir/mode-dir"
)
[[ "$(stat -c %a "$work_dir/mode-dir")" == 750 ]] || fail 'mkdir -m mode differs'

"$bin_dir/mkdir" -p "$work_dir/parents/child/leaf"
[[ -d "$work_dir/parents/child/leaf" ]] || fail 'mkdir -p did not create parents'

"$bin_dir/rmdir" "$work_dir/mode-dir"
[[ ! -e "$work_dir/mode-dir" ]] || fail 'rmdir did not remove an empty directory'

(
    cd "$work_dir"
    "$bin_dir/rmdir" -p parents/child/leaf
)
[[ ! -e "$work_dir/parents" ]] || fail 'rmdir -p did not remove parents'

compare_basename /usr/bin/sort || fail 'basename path output differs'
compare_basename /usr/bin/sort sort || fail 'basename suffix output differs'
compare_basename / || fail 'basename root output differs'
compare_basename /a/b/// || fail 'basename trailing slash output differs'
compare_basename -a foo/bar baz.txt / || fail 'basename -a output differs'
compare_basename -s .txt foo.txt bar.txt || fail 'basename -s output differs'
compare_basename -s.txt foo.txt bar.txt || fail 'basename attached suffix output differs'
compare_basename -- -strange || fail 'basename -- output differs'
"$bin_dir/basename" -az foo/bar baz > "$work_dir/actual"
/usr/bin/basename -az foo/bar baz > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'basename -z output differs'

compare_dirname /usr/bin/sort || fail 'dirname path output differs'
compare_dirname /usr/bin/ / /a foo ./foo ../foo a//b// || fail 'dirname multiple path output differs'
compare_dirname -- -strange || fail 'dirname -- output differs'
"$bin_dir/dirname" -z /usr/bin/sort foo > "$work_dir/actual"
/usr/bin/dirname -z /usr/bin/sort foo > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'dirname -z output differs'

printf 'all coreutils smoke tests passed\n'
