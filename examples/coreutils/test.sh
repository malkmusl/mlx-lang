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

compare_wc_flags() {
    LC_ALL=C "$bin_dir/wc" "$@" > "$work_dir/actual" || return 1
    LC_ALL=C /usr/bin/wc "$@" > "$work_dir/expected" || return 1
    cmp "$work_dir/actual" "$work_dir/expected"
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

compare_head() {
    "$bin_dir/head" "$@" > "$work_dir/actual" || return 1
    /usr/bin/head "$@" > "$work_dir/expected" || return 1
    cmp "$work_dir/actual" "$work_dir/expected"
}

compare_tail() {
    "$bin_dir/tail" "$@" > "$work_dir/actual" || return 1
    /usr/bin/tail "$@" > "$work_dir/expected" || return 1
    cmp "$work_dir/actual" "$work_dir/expected"
}

compare_uname() {
    "$bin_dir/uname" "$@" > "$work_dir/actual" || return 1
    /usr/bin/uname "$@" > "$work_dir/expected" || return 1
    cmp "$work_dir/actual" "$work_dir/expected"
}

compare_printenv() {
    local mlx_status gnu_status
    set +e
    /usr/bin/env -i ALPHA=one EMPTY= ZED=last "$bin_dir/printenv" "$@" > "$work_dir/actual"
    mlx_status=$?
    /usr/bin/env -i ALPHA=one EMPTY= ZED=last /usr/bin/printenv "$@" > "$work_dir/expected"
    gnu_status=$?
    set -e
    [[ "$mlx_status" -eq "$gnu_status" ]] || return 1
    cmp "$work_dir/actual" "$work_dir/expected"
}

compare_echo() {
    "$bin_dir/echo" "$@" > "$work_dir/actual"
    /usr/bin/echo "$@" > "$work_dir/expected"
    cmp "$work_dir/actual" "$work_dir/expected"
}

compare_cat() {
    LC_ALL=C "$bin_dir/cat" "$@" > "$work_dir/actual" || return 1
    LC_ALL=C /usr/bin/cat "$@" > "$work_dir/expected" || return 1
    cmp "$work_dir/actual" "$work_dir/expected"
}

compare_yes() {
    set +o pipefail
    "$bin_dir/yes" "$@" | /usr/bin/head -c 4096 > "$work_dir/actual"
    local mlx_reader_status=${PIPESTATUS[1]}
    /usr/bin/yes "$@" | /usr/bin/head -c 4096 > "$work_dir/expected"
    local gnu_reader_status=${PIPESTATUS[1]}
    set -o pipefail
    [[ "$mlx_reader_status" -eq 0 && "$gnu_reader_status" -eq 0 ]] || return 1
    cmp "$work_dir/actual" "$work_dir/expected"
}

"$bin_dir/true" || fail 'true returned a failure status'
"$bin_dir/true" ignored operands || fail 'true rejected ignored operands'
"$bin_dir/true" --help > "$work_dir/actual" || fail 'true --help failed'
grep -q '^Usage: true' "$work_dir/actual" || fail 'true --help output differs'
"$bin_dir/true" --version > "$work_dir/actual" || fail 'true --version failed'
grep -q '^mlx true ' "$work_dir/actual" || fail 'true --version output differs'
if "$bin_dir/false"; then
    fail 'false returned success'
elif [[ $? -ne 1 ]]; then
    fail 'false did not return status 1'
fi
if "$bin_dir/false" ignored operands; then
    fail 'false accepted ignored operands with success'
elif [[ $? -ne 1 ]]; then
    fail 'false ignored operands changed status'
fi
"$bin_dir/false" --help > "$work_dir/actual" || fail 'false --help failed'
grep -q '^Usage: false' "$work_dir/actual" || fail 'false --help output differs'
"$bin_dir/false" --version > "$work_dir/actual" || fail 'false --version failed'
grep -q '^mlx false ' "$work_dir/actual" || fail 'false --version output differs'

"$bin_dir/echo" alpha beta > "$work_dir/actual"
/usr/bin/echo alpha beta > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'echo output differs'

"$bin_dir/echo" -n alpha beta > "$work_dir/actual"
/usr/bin/echo -n alpha beta > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'echo -n output differs'

"$bin_dir/echo" -- alpha > "$work_dir/actual"
/usr/bin/echo -- alpha > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'echo -- output differs'
compare_echo -e 'one\ntwo' || fail 'echo newline escape differs'
compare_echo -ne 'tab\tvalue' || fail 'echo combined options differ'
compare_echo -n -e 'hex=\x41 octal=\0102' || fail 'echo numeric escapes differ'
compare_echo -e 'alert=\a back=\b escape=\e form=\f return=\r vertical=\v slash=\\' || fail 'echo control escapes differ'
compare_echo -e 'stop\cignored' trailing || fail 'echo stop escape differs'
compare_echo -e -E 'literal\nvalue' || fail 'echo escape disabling differs'
compare_echo -unknown value || fail 'echo unknown option operand differs'

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

printf '\nfirst\n\n\nsecond\tcolumn\n' > "$work_dir/cat-flags-a"
printf 'third\n\001\177\200\377\n' > "$work_dir/cat-flags-b"
compare_cat -n "$work_dir/cat-flags-a" "$work_dir/cat-flags-b" || fail 'cat line numbering differs'
compare_cat -b "$work_dir/cat-flags-a" || fail 'cat nonblank numbering differs'
compare_cat -s "$work_dir/cat-flags-a" || fail 'cat blank squeezing differs'
compare_cat -E "$work_dir/cat-flags-a" || fail 'cat end markers differ'
compare_cat -T "$work_dir/cat-flags-a" || fail 'cat tab markers differ'
compare_cat -v "$work_dir/cat-flags-b" || fail 'cat nonprinting output differs'
compare_cat -A "$work_dir/cat-flags-a" "$work_dir/cat-flags-b" || fail 'cat show-all differs'
compare_cat -benstuvET "$work_dir/cat-flags-a" || fail 'cat combined flags differ'
compare_cat --number --squeeze-blank --show-ends "$work_dir/cat-flags-a" || fail 'cat long flags differ'
if "$bin_dir/cat" --unknown "$work_dir/cat-flags-a" >/dev/null 2>&1; then
    fail 'cat accepted an unknown option'
fi

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
printf 'ab\tcd\n12345\nlast' > "$work_dir/wc-flags"
compare_wc_flags -m "$work_dir/wc-flags" || fail 'wc character count differs'
compare_wc_flags -L "$work_dir/wc-flags" || fail 'wc maximum line length differs'
compare_wc_flags -lwmcL "$work_dir/wc-flags" || fail 'wc combined counts differ'
compare_wc_flags --lines --words --chars --bytes --max-line-length "$work_dir/wc-flags" || fail 'wc long count flags differ'
printf 'x\342\202\254y\n' > "$work_dir/wc-utf8"
LC_ALL=C.UTF-8 "$bin_dir/wc" -m "$work_dir/wc-utf8" > "$work_dir/actual"
LC_ALL=C.UTF-8 /usr/bin/wc -m "$work_dir/wc-utf8" > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'wc UTF-8 character count differs'
printf '%s\0%s\0' "$work_dir/a" "$work_dir/b" > "$work_dir/wc-files0"
compare_wc_flags -c --files0-from="$work_dir/wc-files0" || fail 'wc files0 input differs'
compare_wc_flags -c --total=always "$work_dir/a" || fail 'wc always total differs'
compare_wc_flags -c --total=only "$work_dir/a" "$work_dir/b" || fail 'wc only total differs'
compare_wc_flags -c --total=never "$work_dir/a" "$work_dir/b" || fail 'wc never total differs'
LC_ALL=C "$bin_dir/wc" --debug -l "$work_dir/a" > "$work_dir/actual" 2> "$work_dir/wc-debug"
grep -q 'mlx-wc:' "$work_dir/wc-debug" || fail 'wc --debug emitted no strategy diagnostic'
if "$bin_dir/wc" --files0-from="$work_dir/wc-files0" "$work_dir/a" >/dev/null 2>&1; then
    fail 'wc combined files0 and operands'
fi

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

(
    umask 077
    "$bin_dir/mkdir" -m 'u=rwx,g=rx,o=' "$work_dir/mode-symbolic"
    "$bin_dir/mkdir" -m 000 "$work_dir/mode-zero"
    "$bin_dir/mkdir" -p -m 750 "$work_dir/mode-parents/child"
)
[[ "$(stat -c %a "$work_dir/mode-symbolic")" == 750 ]] || fail 'mkdir symbolic mode differs'
[[ "$(stat -c %a "$work_dir/mode-zero")" == 0 ]] || fail 'mkdir zero mode differs'
[[ "$(stat -c %a "$work_dir/mode-parents")" == 700 ]] || fail 'mkdir parent mode should follow umask'
[[ "$(stat -c %a "$work_dir/mode-parents/child")" == 750 ]] || fail 'mkdir final parent mode differs'
(
    umask 027
    "$bin_dir/mkdir" -m '=rw' "$work_dir/mode-implicit-who"
)
[[ "$(stat -c %a "$work_dir/mode-implicit-who")" == 640 ]] || fail 'mkdir implicit symbolic who ignored umask'
"$bin_dir/mkdir" -Z "$work_dir/mode-context-default"
"$bin_dir/mkdir" --context=mlx-test "$work_dir/mode-context-explicit" 2> "$work_dir/mkdir-context-warning"
[[ -d "$work_dir/mode-context-default" && -d "$work_dir/mode-context-explicit" ]] || fail 'mkdir context flags failed'
if [[ ! -e /sys/fs/selinux/enforce && ! -e /sys/fs/smackfs ]]; then
    grep -q 'warning: ignoring --context' "$work_dir/mkdir-context-warning" || fail 'mkdir omitted unsupported context warning'
fi

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

printf '01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n' > "$work_dir/head-a"
printf 'alpha\nbeta\ngamma\n' > "$work_dir/head-b"
compare_head "$work_dir/head-a" || fail 'head default output differs'
compare_head -n 2 "$work_dir/head-a" || fail 'head -n output differs'
compare_head -n2 "$work_dir/head-a" || fail 'head attached line count differs'
compare_head --lines=3 "$work_dir/head-a" || fail 'head --lines output differs'
compare_head -n 0 "$work_dir/head-a" || fail 'head zero line count differs'
compare_head -c 7 "$work_dir/head-a" || fail 'head -c output differs'
compare_head --bytes=5 "$work_dir/head-a" || fail 'head --bytes output differs'
compare_head -n -2 "$work_dir/head-a" || fail 'head negative line count differs'
compare_head --lines=-3 "$work_dir/head-a" || fail 'head long negative line count differs'
compare_head -c -7 "$work_dir/head-a" || fail 'head negative byte count differs'
compare_head -c 1K "$work_dir/head-a" || fail 'head binary suffix count differs'
compare_head -c 0 "$work_dir/head-a" || fail 'head zero byte count differs'
compare_head -n 1 "$work_dir/head-a" "$work_dir/head-b" || fail 'head multiple-file headers differ'
compare_head -q -n 1 "$work_dir/head-a" "$work_dir/head-b" || fail 'head quiet output differs'
compare_head -v -n 1 "$work_dir/head-a" || fail 'head verbose output differs'
printf 'left\nright\nlast\n' | "$bin_dir/head" -n 2 > "$work_dir/actual"
printf 'left\nright\nlast\n' | /usr/bin/head -n 2 > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'head stdin output differs'
printf 'one\0two\0three\0' > "$work_dir/head-zero"
"$bin_dir/head" -z -n 2 "$work_dir/head-zero" > "$work_dir/actual"
/usr/bin/head -z -n 2 "$work_dir/head-zero" > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'head zero-terminated output differs'
if "$bin_dir/head" -n 18446744073709551616 "$work_dir/head-a" >/dev/null 2>&1; then
    fail 'head accepted an overflowing count'
fi
if "$bin_dir/head" "$work_dir/missing-head-input" >/dev/null 2>&1; then
    fail 'head accepted a missing input file'
fi

printf 'tee stdin\nsecond line\n' | "$bin_dir/tee" > "$work_dir/actual"
printf 'tee stdin\nsecond line\n' > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'tee stdout output differs'

printf 'one target\n' | "$bin_dir/tee" "$work_dir/tee-a" > "$work_dir/actual"
printf 'one target\n' > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'tee stdout with target differs'
cmp "$work_dir/tee-a" "$work_dir/expected" || fail 'tee target output differs'

printf 'multiple targets\n' | "$bin_dir/tee" "$work_dir/tee-a" "$work_dir/tee-b" > "$work_dir/actual"
cmp "$work_dir/tee-a" "$work_dir/actual" || fail 'tee first multiple target differs'
cmp "$work_dir/tee-b" "$work_dir/actual" || fail 'tee second multiple target differs'

printf 'before\n' > "$work_dir/tee-append"
printf 'after\n' | "$bin_dir/tee" -a "$work_dir/tee-append" > "$work_dir/actual"
printf 'before\nafter\n' > "$work_dir/expected"
cmp "$work_dir/tee-append" "$work_dir/expected" || fail 'tee append output differs'

(
    cd "$work_dir"
    printf 'dash target\n' | "$bin_dir/tee" -- -output >/dev/null
)
printf 'dash target\n' > "$work_dir/expected"
cmp "$work_dir/-output" "$work_dir/expected" || fail 'tee -- target differs'
if printf 'error path\n' | "$bin_dir/tee" "$work_dir/missing/target" >"$work_dir/actual" 2>/dev/null; then
    fail 'tee accepted an unopenable output'
fi
printf 'error path\n' > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'tee lost stdout after an output-open error'
for tee_mode in -i -p --output-error=warn --output-error=warn-nopipe --output-error=exit --output-error=exit-nopipe; do
    printf 'mode output\n' | "$bin_dir/tee" "$tee_mode" > "$work_dir/actual"
    printf 'mode output\n' | /usr/bin/tee "$tee_mode" > "$work_dir/expected"
    cmp "$work_dir/actual" "$work_dir/expected" || fail "tee $tee_mode output differs"
done
if "$bin_dir/tee" --output-error=invalid </dev/null >/dev/null 2>&1; then
    fail 'tee accepted an invalid output-error mode'
fi
set +e
set +o pipefail
/usr/bin/yes x | "$bin_dir/tee" -p | /usr/bin/head -c 1 >/dev/null
tee_pipe_status=${PIPESTATUS[1]}
set -o pipefail
set -e
[[ "$tee_pipe_status" -eq 0 ]] || fail 'tee -p did not handle a closed pipe'

compare_yes || fail 'yes default output differs'
compare_yes hello || fail 'yes single-string output differs'
compare_yes hello mlx world || fail 'yes joined-string output differs'
compare_yes -- -value || fail 'yes -- output differs'
set +e
/usr/bin/timeout 1 "$bin_dir/yes" -n >/dev/null 2>&1
yes_invalid_status=$?
set -e
[[ "$yes_invalid_status" -eq 1 ]] || fail 'yes accepted an unknown option'

"$bin_dir/sleep" 0
"$bin_dir/sleep" 0s 0m 0h 0d
sleep_started=$(date +%s%N)
"$bin_dir/sleep" 0.02s 0.01s
sleep_elapsed=$(( $(date +%s%N) - sleep_started ))
[[ "$sleep_elapsed" -ge 20000000 && "$sleep_elapsed" -lt 2000000000 ]] || fail 'sleep decimal duration differs'
if "$bin_dir/sleep" invalid >/dev/null 2>&1; then
    fail 'sleep accepted an invalid duration'
fi
if "$bin_dir/sleep" 18446744073709551615d >/dev/null 2>&1; then
    fail 'sleep accepted an overflowing duration'
fi

printf '01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n' > "$work_dir/tail-a"
printf 'alpha\nbeta\ngamma' > "$work_dir/tail-b"
compare_tail "$work_dir/tail-a" || fail 'tail default output differs'
compare_tail -n 2 "$work_dir/tail-a" || fail 'tail -n output differs'
compare_tail -n2 "$work_dir/tail-b" || fail 'tail attached line count differs'
compare_tail -2 "$work_dir/tail-a" || fail 'tail historical line count differs'
compare_tail --lines=3 "$work_dir/tail-b" || fail 'tail --lines output differs'
compare_tail -n 0 "$work_dir/tail-a" || fail 'tail zero line count differs'
compare_tail -c 7 "$work_dir/tail-a" || fail 'tail -c output differs'
compare_tail --bytes=5 "$work_dir/tail-b" || fail 'tail --bytes output differs'
compare_tail -n +2 "$work_dir/tail-a" || fail 'tail line start offset differs'
compare_tail --lines=+3 "$work_dir/tail-b" || fail 'tail attached line start differs'
compare_tail -c +3 "$work_dir/tail-a" || fail 'tail byte start offset differs'
compare_tail -c 1K "$work_dir/tail-a" || fail 'tail binary suffix count differs'
compare_tail -n 2KB "$work_dir/tail-b" || fail 'tail decimal suffix count differs'
compare_tail -q -n 1 "$work_dir/tail-a" "$work_dir/tail-b" || fail 'tail quiet output differs'
compare_tail -v -n 1 "$work_dir/tail-a" || fail 'tail verbose output differs'
printf 'left\nright\nlast' | "$bin_dir/tail" -n 2 > "$work_dir/actual"
printf 'left\nright\nlast' | /usr/bin/tail -n 2 > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'tail stdin output differs'
printf 'one\0two\0three' > "$work_dir/tail-zero"
"$bin_dir/tail" -z -n 2 "$work_dir/tail-zero" > "$work_dir/actual"
/usr/bin/tail -z -n 2 "$work_dir/tail-zero" > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'tail zero-terminated output differs'
if "$bin_dir/tail" -n 18446744073709551616 "$work_dir/tail-a" >/dev/null 2>&1; then
    fail 'tail accepted an overflowing count'
fi
if "$bin_dir/tail" "$work_dir/missing-tail-input" >/dev/null 2>&1; then
    fail 'tail accepted a missing input file'
fi
timeout 3 "$bin_dir/tail" --pid=999999 -f -s 0.01 "$work_dir/tail-a" > "$work_dir/actual"
/usr/bin/tail --pid=999999 -f -s 0.01 "$work_dir/tail-a" > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'tail descriptor follow output differs'
timeout 3 "$bin_dir/tail" --debug --pid=999999 -F -s 0.01 "$work_dir/tail-b" > "$work_dir/actual" 2> "$work_dir/tail-debug"
/usr/bin/tail --pid=999999 -F -s 0.01 "$work_dir/tail-b" > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'tail name follow output differs'
grep -q 'mlx-tail:' "$work_dir/tail-debug" || fail 'tail --debug emitted no strategy diagnostic'
printf 'follow-start\n' > "$work_dir/tail-follow"
(
    sleep 0.05
    printf 'follow-next\n' >> "$work_dir/tail-follow"
    sleep 0.05
) &
tail_writer=$!
timeout 3 "$bin_dir/tail" --pid="$tail_writer" -f -s.01 "$work_dir/tail-follow" > "$work_dir/actual"
printf 'follow-start\nfollow-next\n' > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'tail did not stream appended data'

compare_uname || fail 'uname default output differs'
compare_uname -a || fail 'uname -a output differs'
compare_uname -snrvm || fail 'uname combined output differs'
compare_uname -p -i -o || fail 'uname extended fields differ'
compare_uname --kernel-name --nodename --kernel-release --kernel-version --machine --operating-system || \
    fail 'uname long-option output differs'
if "$bin_dir/uname" --unknown >/dev/null 2>&1; then
    fail 'uname accepted an unknown option'
fi

compare_printenv || fail 'printenv complete environment differs'
compare_printenv ALPHA EMPTY ZED || fail 'printenv selected values differ'
compare_printenv ALPHA MISSING ZED || fail 'printenv missing-value status differs'
compare_printenv -0 ALPHA EMPTY ZED || fail 'printenv NUL output differs'
compare_printenv --null ALPHA || fail 'printenv long NUL output differs'
if "$bin_dir/printenv" --unknown >/dev/null 2>&1; then
    fail 'printenv accepted an unknown option'
fi

/usr/bin/env -i OLD=drop KEEP=yes "$bin_dir/env" -i ALPHA=one EMPTY= > "$work_dir/actual"
/usr/bin/env -i OLD=drop KEEP=yes /usr/bin/env -i ALPHA=one EMPTY= > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'env clean environment differs'

/usr/bin/env -i OLD=drop KEEP=yes SECOND=drop "$bin_dir/env" -u OLD --unset=SECOND > "$work_dir/actual"
/usr/bin/env -i OLD=drop KEEP=yes SECOND=drop /usr/bin/env -u OLD --unset=SECOND > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'env unset environment differs'

/usr/bin/env -i OLD=before KEEP=yes "$bin_dir/env" OLD=after NEXT=two > "$work_dir/actual"
/usr/bin/env -i OLD=before KEEP=yes /usr/bin/env OLD=after NEXT=two > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'env assignments differ'

/usr/bin/env -i "$bin_dir/env" A=one B=two A=three > "$work_dir/actual"
/usr/bin/env -i /usr/bin/env A=one B=two A=three > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'env repeated assignments differ'

/usr/bin/env -i OLD=before KEEP=yes "$bin_dir/env" -u OLD OLD=after NEXT=two > "$work_dir/actual"
/usr/bin/env -i OLD=before KEEP=yes /usr/bin/env -u OLD OLD=after NEXT=two > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'env unset and assignment order differs'

/usr/bin/env -i ALPHA=one EMPTY= "$bin_dir/env" -0 > "$work_dir/actual"
/usr/bin/env -i ALPHA=one EMPTY= /usr/bin/env -0 > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'env NUL output differs'

/usr/bin/env -i "$bin_dir/env" ANSWER=42 /usr/bin/printenv ANSWER > "$work_dir/actual"
printf '42\n' > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'env direct execution differs'

/usr/bin/env -i "$bin_dir/env" PATH=/usr/bin ANSWER=43 printenv ANSWER > "$work_dir/actual"
printf '43\n' > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'env PATH execution differs'

set +e
/usr/bin/env -i "$bin_dir/env" /bin/sh -c 'exit 7'
env_command_status=$?
/usr/bin/env -i "$bin_dir/env" missing-mlx-command >/dev/null 2>&1
env_missing_status=$?
/usr/bin/env -i "$bin_dir/env" -0 /bin/true >/dev/null 2>&1
env_zero_command_status=$?
/usr/bin/env -i "$bin_dir/env" --unset= >/dev/null 2>&1
env_empty_unset_status=$?
set -e
[[ "$env_command_status" -eq 7 ]] || fail 'env lost child exit status'
[[ "$env_missing_status" -eq 127 ]] || fail 'env missing-command status differs'
[[ "$env_zero_command_status" -eq 125 ]] || fail 'env accepted NUL output with a command'
[[ "$env_empty_unset_status" -eq 125 ]] || fail 'env accepted an empty unset name'
if "$bin_dir/env" --unknown >/dev/null 2>&1; then
    fail 'env accepted an unknown option'
fi
"$bin_dir/env" -a mlx-argv0 /bin/sh -c 'printf "%s\n" "$0"' > "$work_dir/actual"
printf 'mlx-argv0\n' > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'env --argv0 output differs'
"$bin_dir/env" --chdir="$work_dir" /bin/pwd > "$work_dir/actual"
printf '%s\n' "$work_dir" > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'env --chdir output differs'
"$bin_dir/env" --debug MLX_ENV_DEBUG=value /usr/bin/printenv MLX_ENV_DEBUG > "$work_dir/actual" 2> "$work_dir/env-debug"
printf 'value\n' > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'env --debug changed command output'
grep -q 'mlx-env:' "$work_dir/env-debug" || fail 'env --debug emitted no trace'
"$bin_dir/env" --ignore-signal=PIPE /bin/sh -c 'kill -s PIPE $$; printf survived' > "$work_dir/actual"
[[ "$(cat "$work_dir/actual")" == survived ]] || fail 'env --ignore-signal did not preserve the command'
"$bin_dir/env" --block-signal=TERM /bin/sh -c 'kill -s TERM $$; printf blocked' > "$work_dir/actual"
[[ "$(cat "$work_dir/actual")" == blocked ]] || fail 'env --block-signal did not block delivery'
"$bin_dir/env" --list-signal-handling --ignore-signal=PIPE /bin/true 2> "$work_dir/env-signals"
grep -q 'signals ignored' "$work_dir/env-signals" || fail 'env did not list signal handling'
"$bin_dir/env" -vS '/usr/bin/printf "split:%s:%s\n" one' two > "$work_dir/actual" 2> "$work_dir/env-split-debug"
/usr/bin/env -S '/usr/bin/printf "split:%s:%s\n" one' two > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'env --split-string output differs'
grep -q 'mlx-env:' "$work_dir/env-split-debug" || fail 'env -vS emitted no trace'

"$bin_dir/nproc" > "$work_dir/actual"
/usr/bin/nproc > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'nproc available count differs'
"$bin_dir/nproc" --all > "$work_dir/actual"
/usr/bin/nproc --all > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'nproc configured count differs'
"$bin_dir/nproc" --ignore=1 > "$work_dir/actual"
/usr/bin/nproc --ignore=1 > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'nproc attached ignore differs'
"$bin_dir/nproc" --ignore 999999 > "$work_dir/actual"
/usr/bin/nproc --ignore 999999 > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'nproc saturated ignore differs'
affinity_list=$(taskset -pc $$ | sed 's/.*: //')
first_cpu=${affinity_list%%[-,]*}
taskset -c "$first_cpu" "$bin_dir/nproc" > "$work_dir/actual"
taskset -c "$first_cpu" /usr/bin/nproc > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'nproc affinity count differs'
if "$bin_dir/nproc" --ignore=invalid >/dev/null 2>&1; then
    fail 'nproc accepted an invalid ignore count'
fi

printf 'hard link payload\n' > "$work_dir/link-source"
"$bin_dir/link" "$work_dir/link-source" "$work_dir/link-target"
cmp "$work_dir/link-source" "$work_dir/link-target" || fail 'link content differs'
[[ "$(stat -c %i "$work_dir/link-source")" == "$(stat -c %i "$work_dir/link-target")" ]] || fail 'link inode differs'
if "$bin_dir/link" "$work_dir/missing-link-source" "$work_dir/link-missing-target" >/dev/null 2>&1; then
    fail 'link accepted a missing source'
fi
if "$bin_dir/link" "$work_dir/link-source" >/dev/null 2>&1; then
    fail 'link accepted a missing destination'
fi

"$bin_dir/unlink" -- "$work_dir/link-target"
[[ ! -e "$work_dir/link-target" ]] || fail 'unlink did not remove its operand'
if "$bin_dir/unlink" "$work_dir/link-target" >/dev/null 2>&1; then
    fail 'unlink accepted a missing file'
fi
if "$bin_dir/unlink" "$work_dir/link-source" "$work_dir/link-target" >/dev/null 2>&1; then
    fail 'unlink accepted multiple operands'
fi

printf 'touch payload\n' > "$work_dir/touch-existing"
/usr/bin/touch -d @1 "$work_dir/touch-existing"
"$bin_dir/touch" "$work_dir/touch-existing" "$work_dir/touch-created"
[[ "$(stat -c %Y "$work_dir/touch-existing")" -gt 1 ]] || fail 'touch did not update timestamp'
[[ -f "$work_dir/touch-created" ]] || fail 'touch did not create a missing file'
grep -q 'touch payload' "$work_dir/touch-existing" || fail 'touch changed file contents'
"$bin_dir/touch" -c "$work_dir/touch-no-create"
[[ ! -e "$work_dir/touch-no-create" ]] || fail 'touch -c created a missing file'
printf 'touch selected times\n' > "$work_dir/touch-selected"
/usr/bin/touch -a -d @10 "$work_dir/touch-selected"
/usr/bin/touch -m -d @20 "$work_dir/touch-selected"
"$bin_dir/touch" -a -d @30 "$work_dir/touch-selected"
[[ "$(stat -c %X "$work_dir/touch-selected")" == 30 && "$(stat -c %Y "$work_dir/touch-selected")" == 20 ]] || fail 'touch -a changed the wrong timestamps'
"$bin_dir/touch" --time=mtime -d @40 "$work_dir/touch-selected"
[[ "$(stat -c %X "$work_dir/touch-selected")" == 30 && "$(stat -c %Y "$work_dir/touch-selected")" == 40 ]] || fail 'touch --time=mtime changed the wrong timestamps'
printf reference > "$work_dir/touch-reference"
/usr/bin/touch -a -d @51 "$work_dir/touch-reference"
/usr/bin/touch -m -d @52 "$work_dir/touch-reference"
"$bin_dir/touch" -r "$work_dir/touch-reference" "$work_dir/touch-selected"
[[ "$(stat -c %X "$work_dir/touch-selected")" == 51 && "$(stat -c %Y "$work_dir/touch-selected")" == 52 ]] || fail 'touch reference times differ'
printf target > "$work_dir/touch-link-target"
ln -s "$work_dir/touch-link-target" "$work_dir/touch-link"
"$bin_dir/touch" -h -d @61 "$work_dir/touch-link"
[[ "$(stat -c %Y "$work_dir/touch-link-target")" != 61 && "$(stat -c %Y "$work_dir/touch-link")" == 61 ]] || fail 'touch -h did not update the symlink'
TZ=UTC0 "$bin_dir/touch" -t 202401020304.05 "$work_dir/touch-selected"
[[ "$(stat -c %Y "$work_dir/touch-selected")" == "$(date -u -d '2024-01-02 03:04:05' +%s)" ]] || fail 'touch -t timestamp differs'
"$bin_dir/touch" -f -d @70 "$work_dir/touch-selected"
[[ "$(stat -c %Y "$work_dir/touch-selected")" == 70 ]] || fail 'touch -f compatibility flag differs'
"$bin_dir/touch" -d @80.123456789 "$work_dir/touch-selected"
[[ "$(stat -c %y "$work_dir/touch-selected")" == *".123456789 "* ]] || fail 'touch fractional timestamp differs'
if "$bin_dir/touch" -d not-a-date "$work_dir/touch-selected" >/dev/null 2>&1; then
    fail 'touch accepted an invalid date'
fi
if "$bin_dir/touch" -h "$work_dir/touch-missing-link" >/dev/null 2>&1; then
    fail 'touch -h accepted a missing symlink without -c'
fi
"$bin_dir/touch" -ch "$work_dir/touch-missing-link"
[[ ! -e "$work_dir/touch-missing-link" ]] || fail 'touch -ch created a missing symlink'
(
    cd "$work_dir"
    "$bin_dir/touch" -- -touch-dash
)
[[ -f "$work_dir/-touch-dash" ]] || fail 'touch -- did not handle a dash path'
if "$bin_dir/touch" "$work_dir/missing-touch-parent/file" >/dev/null 2>&1; then
    fail 'touch accepted an invalid parent path'
fi

"$bin_dir/truncate" -s 10 "$work_dir/truncate-created"
[[ "$(stat -c %s "$work_dir/truncate-created")" == 10 ]] || fail 'truncate did not create requested size'
printf 'abcdefghijklmnop' > "$work_dir/truncate-a"
"$bin_dir/truncate" --size=3 "$work_dir/truncate-a"
[[ "$(stat -c %s "$work_dir/truncate-a")" == 3 ]] || fail 'truncate did not shrink a file'
"$bin_dir/truncate" -s 2K "$work_dir/truncate-a" "$work_dir/truncate-b"
[[ "$(stat -c %s "$work_dir/truncate-a")" == 2048 ]] || fail 'truncate suffix size differs'
[[ "$(stat -c %s "$work_dir/truncate-b")" == 2048 ]] || fail 'truncate multiple files differ'
"$bin_dir/truncate" -c -s 7 "$work_dir/truncate-no-create"
[[ ! -e "$work_dir/truncate-no-create" ]] || fail 'truncate -c created a missing file'
if "$bin_dir/truncate" -s invalid "$work_dir/truncate-invalid" >/dev/null 2>&1; then
    fail 'truncate accepted an invalid size'
fi
printf '12345678901234567' > "$work_dir/truncate-reference"
"$bin_dir/truncate" -r "$work_dir/truncate-reference" "$work_dir/truncate-created"
[[ "$(stat -c %s "$work_dir/truncate-created")" == 17 ]] || fail 'truncate reference size differs'
: > "$work_dir/truncate-empty-reference"
"$bin_dir/truncate" -r "$work_dir/truncate-empty-reference" "$work_dir/truncate-created"
[[ "$(stat -c %s "$work_dir/truncate-created")" == 0 ]] || fail 'truncate empty reference size differs'
"$bin_dir/truncate" -s 0 "$work_dir/truncate-created"
[[ "$(stat -c %s "$work_dir/truncate-created")" == 0 ]] || fail 'truncate zero size differs'
"$bin_dir/truncate" -r "$work_dir/truncate-reference" "$work_dir/truncate-created"
"$bin_dir/truncate" -s +3 "$work_dir/truncate-created"
[[ "$(stat -c %s "$work_dir/truncate-created")" == 20 ]] || fail 'truncate extend operation differs'
"$bin_dir/truncate" -s -5 "$work_dir/truncate-created"
[[ "$(stat -c %s "$work_dir/truncate-created")" == 15 ]] || fail 'truncate reduce operation differs'
"$bin_dir/truncate" -s '<10' "$work_dir/truncate-created"
[[ "$(stat -c %s "$work_dir/truncate-created")" == 10 ]] || fail 'truncate at-most operation differs'
"$bin_dir/truncate" -s '>12' "$work_dir/truncate-created"
[[ "$(stat -c %s "$work_dir/truncate-created")" == 12 ]] || fail 'truncate at-least operation differs'
"$bin_dir/truncate" -s 2KB "$work_dir/truncate-created"
[[ "$(stat -c %s "$work_dir/truncate-created")" == 2000 ]] || fail 'truncate decimal suffix differs'
"$bin_dir/truncate" -s 2KiB "$work_dir/truncate-created"
[[ "$(stat -c %s "$work_dir/truncate-created")" == 2048 ]] || fail 'truncate binary prefix differs'
"$bin_dir/truncate" -s /1000 "$work_dir/truncate-created"
[[ "$(stat -c %s "$work_dir/truncate-created")" == 2000 ]] || fail 'truncate round-down differs'
"$bin_dir/truncate" -s %1024 "$work_dir/truncate-created"
[[ "$(stat -c %s "$work_dir/truncate-created")" == 2048 ]] || fail 'truncate round-up differs'
"$bin_dir/truncate" -o -s 1 "$work_dir/truncate-created"
[[ "$(stat -c %s "$work_dir/truncate-created")" == "$(stat -c %o "$work_dir/truncate-created")" ]] || fail 'truncate IO block size differs'

(
    umask 000
    "$bin_dir/mkfifo" -m 640 "$work_dir/fifo-a" "$work_dir/fifo-b"
)
[[ -p "$work_dir/fifo-a" && -p "$work_dir/fifo-b" ]] || fail 'mkfifo did not create FIFOs'
[[ "$(stat -c %a "$work_dir/fifo-a")" == 640 ]] || fail 'mkfifo mode differs'
(
    umask 077
    "$bin_dir/mkfifo" -m 'u=rw,g=r,o=' "$work_dir/fifo-symbolic"
    "$bin_dir/mkfifo" -m 666 "$work_dir/fifo-exact"
)
[[ "$(stat -c %a "$work_dir/fifo-symbolic")" == 640 ]] || fail 'mkfifo symbolic mode differs'
[[ "$(stat -c %a "$work_dir/fifo-exact")" == 666 ]] || fail 'mkfifo explicit mode followed umask'
(
    umask 027
    "$bin_dir/mkfifo" -m '=rw' "$work_dir/fifo-implicit-who"
)
[[ "$(stat -c %a "$work_dir/fifo-implicit-who")" == 640 ]] || fail 'mkfifo implicit symbolic who ignored umask'
"$bin_dir/mkfifo" -Z "$work_dir/fifo-context-default"
"$bin_dir/mkfifo" --context=mlx-test "$work_dir/fifo-context-explicit" 2> "$work_dir/mkfifo-context-warning"
[[ -p "$work_dir/fifo-context-default" && -p "$work_dir/fifo-context-explicit" ]] || fail 'mkfifo context flags failed'
if [[ ! -e /sys/fs/selinux/enforce && ! -e /sys/fs/smackfs ]]; then
    grep -q 'warning: ignoring --context' "$work_dir/mkfifo-context-warning" || fail 'mkfifo omitted unsupported context warning'
fi
if "$bin_dir/mkfifo" "$work_dir/fifo-a" >/dev/null 2>&1; then
    fail 'mkfifo accepted an existing path'
fi
if "$bin_dir/mkfifo" -m invalid "$work_dir/fifo-invalid" >/dev/null 2>&1; then
    fail 'mkfifo accepted an invalid mode'
fi

printf 'copy payload\n' > "$work_dir/cp-source"
chmod 640 "$work_dir/cp-source"
/usr/bin/touch -d @90 "$work_dir/cp-source"
"$bin_dir/cp" "$work_dir/cp-source" "$work_dir/cp-target"
cmp "$work_dir/cp-source" "$work_dir/cp-target" || fail 'cp file content differs'
"$bin_dir/cp" -p "$work_dir/cp-source" "$work_dir/cp-preserved"
[[ "$(stat -c %a "$work_dir/cp-preserved")" == 640 && "$(stat -c %Y "$work_dir/cp-preserved")" == 90 ]] || fail 'cp preserve metadata differs'
mkdir "$work_dir/cp-directory"
"$bin_dir/cp" "$work_dir/cp-source" "$work_dir/tail-a" "$work_dir/cp-directory"
cmp "$work_dir/cp-source" "$work_dir/cp-directory/cp-source" || fail 'cp multiple-source directory copy differs'
cmp "$work_dir/tail-a" "$work_dir/cp-directory/tail-a" || fail 'cp second directory target differs'
printf 'keep\n' > "$work_dir/cp-no-clobber"
"$bin_dir/cp" -n "$work_dir/cp-source" "$work_dir/cp-no-clobber"
[[ "$(cat "$work_dir/cp-no-clobber")" == keep ]] || fail 'cp -n overwrote a destination'
"$bin_dir/cp" -l "$work_dir/cp-source" "$work_dir/cp-link"
[[ "$(stat -c %i "$work_dir/cp-source")" == "$(stat -c %i "$work_dir/cp-link")" ]] || fail 'cp -l did not create a hard link'
if "$bin_dir/cp" "$work_dir/cp-source" "$work_dir/cp-source" >/dev/null 2>&1; then
    fail 'cp accepted identical source and destination'
fi
if "$bin_dir/cp" --target-directory= "$work_dir/cp-source" >/dev/null 2>&1; then
    fail 'cp accepted an empty target directory'
fi

printf 'move payload\n' > "$work_dir/mv-source"
"$bin_dir/mv" "$work_dir/mv-source" "$work_dir/mv-target"
[[ ! -e "$work_dir/mv-source" && "$(cat "$work_dir/mv-target")" == 'move payload' ]] || fail 'mv basic rename differs'
mkdir "$work_dir/mv-directory"
printf 'first\n' > "$work_dir/mv-first"
printf 'second\n' > "$work_dir/mv-second"
"$bin_dir/mv" "$work_dir/mv-first" "$work_dir/mv-second" "$work_dir/mv-directory"
[[ "$(cat "$work_dir/mv-directory/mv-first")" == first ]] || fail 'mv first directory target differs'
[[ "$(cat "$work_dir/mv-directory/mv-second")" == second ]] || fail 'mv second directory target differs'
printf 'source\n' > "$work_dir/mv-no-clobber-source"
printf 'destination\n' > "$work_dir/mv-no-clobber-target"
"$bin_dir/mv" -n "$work_dir/mv-no-clobber-source" "$work_dir/mv-no-clobber-target"
[[ -e "$work_dir/mv-no-clobber-source" && "$(cat "$work_dir/mv-no-clobber-target")" == destination ]] || fail 'mv -n overwrote a destination'
printf 'older source\n' > "$work_dir/mv-update-source"
printf 'newer destination\n' > "$work_dir/mv-update-target"
/usr/bin/touch -d @90 "$work_dir/mv-update-source"
/usr/bin/touch -d @100 "$work_dir/mv-update-target"
"$bin_dir/mv" -u "$work_dir/mv-update-source" "$work_dir/mv-update-target"
[[ -e "$work_dir/mv-update-source" && "$(cat "$work_dir/mv-update-target")" == 'newer destination' ]] || fail 'mv -u replaced a newer destination'
printf 'left\n' > "$work_dir/mv-exchange-left"
printf 'right\n' > "$work_dir/mv-exchange-right"
"$bin_dir/mv" --exchange "$work_dir/mv-exchange-left" "$work_dir/mv-exchange-right"
[[ "$(cat "$work_dir/mv-exchange-left")" == right && "$(cat "$work_dir/mv-exchange-right")" == left ]] || fail 'mv --exchange did not swap paths'
if "$bin_dir/mv" "$work_dir/mv-target" "$work_dir/mv-target" >/dev/null 2>&1; then
    fail 'mv accepted identical source and destination'
fi
if "$bin_dir/mv" --target-directory= "$work_dir/mv-target" >/dev/null 2>&1; then
    fail 'mv accepted an empty target directory'
fi

printf 'remove me\n' > "$work_dir/rm-file"
"$bin_dir/rm" "$work_dir/rm-file"
[[ ! -e "$work_dir/rm-file" ]] || fail 'rm did not remove a regular file'
"$bin_dir/rm" -f "$work_dir/rm-missing"
mkdir "$work_dir/rm-empty"
if "$bin_dir/rm" "$work_dir/rm-empty" >/dev/null 2>&1; then
    fail 'rm removed a directory without -d or recursion'
fi
"$bin_dir/rm" -d "$work_dir/rm-empty"
[[ ! -e "$work_dir/rm-empty" ]] || fail 'rm -d did not remove an empty directory'
mkdir -p "$work_dir/rm-tree/first/second"
printf 'nested\n' > "$work_dir/rm-tree/first/second/file"
printf 'outside\n' > "$work_dir/rm-outside"
ln -s "$work_dir/rm-outside" "$work_dir/rm-tree/outside-link"
"$bin_dir/rm" -rv "$work_dir/rm-tree" > "$work_dir/rm-verbose"
[[ ! -e "$work_dir/rm-tree" && "$(cat "$work_dir/rm-outside")" == outside ]] || fail 'rm recursive traversal followed a symlink or left the tree'
grep -q "removed.*rm-tree/first/second/file" "$work_dir/rm-verbose" || fail 'rm -v omitted a removed path'
printf 'keep\n' > "$work_dir/rm-interactive-no"
printf 'n\n' | "$bin_dir/rm" -i "$work_dir/rm-interactive-no" 2>/dev/null
[[ -e "$work_dir/rm-interactive-no" ]] || fail 'rm -i ignored a negative answer'
printf 'delete\n' > "$work_dir/rm-interactive-yes"
printf 'y\n' | "$bin_dir/rm" -i "$work_dir/rm-interactive-yes" 2>/dev/null
[[ ! -e "$work_dir/rm-interactive-yes" ]] || fail 'rm -i ignored an affirmative answer'
mkdir -p "$work_dir/rm-once/subdirectory"
printf 'keep\n' > "$work_dir/rm-once/subdirectory/file"
printf 'n\n' | "$bin_dir/rm" -rI "$work_dir/rm-once" 2>/dev/null
[[ -e "$work_dir/rm-once/subdirectory/file" ]] || fail 'rm -I ignored a negative batch answer'
if "$bin_dir/rm" "$work_dir/rm-once/." >/dev/null 2>&1; then
    fail 'rm accepted a final dot component'
fi

printf 'link payload\n' > "$work_dir/ln-source"
"$bin_dir/ln" "$work_dir/ln-source" "$work_dir/ln-hard"
[[ "$(stat -c %i "$work_dir/ln-source")" == "$(stat -c %i "$work_dir/ln-hard")" ]] || fail 'ln did not create a hard link'
"$bin_dir/ln" -s "$work_dir/ln-source" "$work_dir/ln-symbolic"
[[ "$(readlink "$work_dir/ln-symbolic")" == "$work_dir/ln-source" ]] || fail 'ln -s target differs'
mkdir "$work_dir/ln-directory"
printf 'first\n' > "$work_dir/ln-first"
printf 'second\n' > "$work_dir/ln-second"
"$bin_dir/ln" "$work_dir/ln-first" "$work_dir/ln-second" "$work_dir/ln-directory"
[[ "$(stat -c %i "$work_dir/ln-first")" == "$(stat -c %i "$work_dir/ln-directory/ln-first")" ]] || fail 'ln first directory target differs'
[[ "$(stat -c %i "$work_dir/ln-second")" == "$(stat -c %i "$work_dir/ln-directory/ln-second")" ]] || fail 'ln second directory target differs'
printf 'replace me\n' > "$work_dir/ln-force"
"$bin_dir/ln" -sf "$work_dir/ln-source" "$work_dir/ln-force"
[[ "$(readlink "$work_dir/ln-force")" == "$work_dir/ln-source" ]] || fail 'ln -f did not replace the destination'
printf 'back me up\n' > "$work_dir/ln-backup"
"$bin_dir/ln" -sfbS .old "$work_dir/ln-source" "$work_dir/ln-backup"
[[ "$(cat "$work_dir/ln-backup.old")" == 'back me up' ]] || fail 'ln backup contents differ'
ln -s "$work_dir/ln-source" "$work_dir/ln-source-link"
"$bin_dir/ln" -P "$work_dir/ln-source-link" "$work_dir/ln-physical"
"$bin_dir/ln" -L "$work_dir/ln-source-link" "$work_dir/ln-logical"
[[ "$(stat -c %i "$work_dir/ln-source-link")" == "$(stat -c %i "$work_dir/ln-physical")" ]] || fail 'ln -P followed a symbolic source'
[[ "$(stat -Lc %i "$work_dir/ln-source-link")" == "$(stat -c %i "$work_dir/ln-logical")" ]] || fail 'ln -L did not follow a symbolic source'
printf 'keep\n' > "$work_dir/ln-interactive"
if printf 'n\n' | "$bin_dir/ln" -si "$work_dir/ln-source" "$work_dir/ln-interactive" >/dev/null 2>/dev/null; then
    fail 'ln -i reported success after a rejected replacement'
fi
[[ "$(cat "$work_dir/ln-interactive")" == keep ]] || fail 'ln -i replaced a rejected destination'
if "$bin_dir/ln" -f "$work_dir/ln-source" "$work_dir/ln-source" >/dev/null 2>&1; then
    fail 'ln -f accepted identical source and destination'
fi

printf 'mode\n' > "$work_dir/chmod-file"
chmod 640 "$work_dir/chmod-file"
"$bin_dir/chmod" 600 "$work_dir/chmod-file"
[[ "$(stat -c %a "$work_dir/chmod-file")" == 600 ]] || fail 'chmod numeric mode differs'
"$bin_dir/chmod" 'u+x,g=u,o=' "$work_dir/chmod-file"
[[ "$(stat -c %a "$work_dir/chmod-file")" == 770 ]] || fail 'chmod symbolic copy mode differs'
chmod 600 "$work_dir/chmod-file"
"$bin_dir/chmod" +110 "$work_dir/chmod-file"
[[ "$(stat -c %a "$work_dir/chmod-file")" == 710 ]] || fail 'chmod symbolic numeric operation differs'
printf 'reference\n' > "$work_dir/chmod-reference"
chmod 754 "$work_dir/chmod-reference"
"$bin_dir/chmod" --reference="$work_dir/chmod-reference" "$work_dir/chmod-file"
[[ "$(stat -c %a "$work_dir/chmod-file")" == 754 ]] || fail 'chmod reference mode differs'
mkdir -p "$work_dir/chmod-tree/first/second"
printf 'leaf\n' > "$work_dir/chmod-tree/first/second/file"
chmod -R 777 "$work_dir/chmod-tree"
"$bin_dir/chmod" -R 'a=,u=rwX' "$work_dir/chmod-tree"
[[ "$(stat -c %a "$work_dir/chmod-tree")" == 700 && "$(stat -c %a "$work_dir/chmod-tree/first/second")" == 700 ]] || fail 'chmod recursive directory mode differs'
[[ "$(stat -c %a "$work_dir/chmod-tree/first/second/file")" == 600 ]] || fail 'chmod recursive X handling differs'
mkdir "$work_dir/chmod-linked-directory"
printf 'linked\n' > "$work_dir/chmod-linked-directory/file"
chmod 755 "$work_dir/chmod-linked-directory"
chmod 644 "$work_dir/chmod-linked-directory/file"
ln -s "$work_dir/chmod-linked-directory" "$work_dir/chmod-directory-link"
"$bin_dir/chmod" -RP 700 "$work_dir/chmod-directory-link"
[[ "$(stat -c %a "$work_dir/chmod-linked-directory")" == 755 && "$(stat -c %a "$work_dir/chmod-linked-directory/file")" == 644 ]] || fail 'chmod -P traversed a command-line symlink'
"$bin_dir/chmod" -RH 700 "$work_dir/chmod-directory-link"
[[ "$(stat -c %a "$work_dir/chmod-linked-directory")" == 700 && "$(stat -c %a "$work_dir/chmod-linked-directory/file")" == 700 ]] || fail 'chmod -H did not traverse a command-line symlink'

mkdir -p "$work_dir/path-base/real/subdirectory" "$work_dir/path-base/other"
printf 'resolved\n' > "$work_dir/path-base/real/file"
ln -s real "$work_dir/path-base/link"
ln -s ../real/file "$work_dir/path-base/other/relative-link"
[[ "$("$bin_dir/readlink" "$work_dir/path-base/link")" == real ]] || fail 'readlink raw target differs'
"$bin_dir/readlink" -n "$work_dir/path-base/link" > "$work_dir/readlink-no-newline"
[[ "$(wc -c < "$work_dir/readlink-no-newline")" == 4 ]] || fail 'readlink -n wrote a delimiter'
"$bin_dir/readlink" -z "$work_dir/path-base/link" > "$work_dir/readlink-zero"
printf 'real\0' > "$work_dir/readlink-zero-expected"
cmp "$work_dir/readlink-zero" "$work_dir/readlink-zero-expected" || fail 'readlink -z delimiter differs'
"$bin_dir/readlink" -f "$work_dir/path-base/link/subdirectory/../file" > "$work_dir/actual"
/usr/bin/readlink -f "$work_dir/path-base/link/subdirectory/../file" > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'readlink -f canonical path differs'
if "$bin_dir/readlink" -e "$work_dir/path-base/link/missing" >/dev/null; then
    fail 'readlink -e accepted a missing component'
fi
"$bin_dir/readlink" -m "$work_dir/path-base/missing/../future" > "$work_dir/actual"
/usr/bin/readlink -m "$work_dir/path-base/missing/../future" > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'readlink -m missing path differs'
"$bin_dir/realpath" "$work_dir/path-base/other/relative-link" > "$work_dir/actual"
/usr/bin/realpath "$work_dir/path-base/other/relative-link" > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'realpath symlink resolution differs'
"$bin_dir/realpath" --relative-to="$work_dir/path-base/real/subdirectory" "$work_dir/path-base/other/relative-link" > "$work_dir/actual"
/usr/bin/realpath --relative-to="$work_dir/path-base/real/subdirectory" "$work_dir/path-base/other/relative-link" > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'realpath --relative-to differs'
"$bin_dir/realpath" --relative-base="$work_dir/path-base" "$work_dir/path-base/other/relative-link" > "$work_dir/actual"
/usr/bin/realpath --relative-base="$work_dir/path-base" "$work_dir/path-base/other/relative-link" > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'realpath --relative-base differs'
ln -s real/subdirectory "$work_dir/path-base/deep-link"
"$bin_dir/realpath" -L "$work_dir/path-base/deep-link/../other" > "$work_dir/actual"
/usr/bin/realpath -L "$work_dir/path-base/deep-link/../other" > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'realpath -L ordering differs'
"$bin_dir/realpath" -s "$work_dir/path-base/link/missing" > "$work_dir/actual"
/usr/bin/realpath -s "$work_dir/path-base/link/missing" > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'realpath -s lexical path differs'
mkdir -p "$work_dir/ln-relative/source" "$work_dir/ln-relative/destination/deep"
printf 'relative\n' > "$work_dir/ln-relative/source/file"
"$bin_dir/ln" -sr "$work_dir/ln-relative/source/file" "$work_dir/ln-relative/destination/deep/link"
[[ "$(readlink "$work_dir/ln-relative/destination/deep/link")" == ../../source/file ]] || fail 'ln -r target differs'
[[ "$(cat "$work_dir/ln-relative/destination/deep/link")" == relative ]] || fail 'ln -r did not resolve to the source'

owner_uid=$(id -u)
owner_gid=$(id -g)
owner_name=$(id -un)
group_name=$(id -gn)
printf 'ownership\n' > "$work_dir/chown-file"
"$bin_dir/chown" "$owner_uid:$owner_gid" "$work_dir/chown-file"
[[ "$(stat -c %u:%g "$work_dir/chown-file")" == "$owner_uid:$owner_gid" ]] || fail 'chown numeric ownership differs'
"$bin_dir/chown" "$owner_name:$group_name" "$work_dir/chown-file"
[[ "$(stat -c %u:%g "$work_dir/chown-file")" == "$owner_uid:$owner_gid" ]] || fail 'chown named ownership differs'
"$bin_dir/chown" "$owner_name:" "$work_dir/chown-file"
[[ "$(stat -c %u:%g "$work_dir/chown-file")" == "$owner_uid:$owner_gid" ]] || fail 'chown implied login group differs'
"$bin_dir/chgrp" "$group_name" "$work_dir/chown-file"
[[ "$(stat -c %g "$work_dir/chown-file")" == "$owner_gid" ]] || fail 'chgrp named group differs'
printf 'reference\n' > "$work_dir/chown-reference"
"$bin_dir/chown" --reference="$work_dir/chown-reference" "$work_dir/chown-file"
[[ "$(stat -c %u:%g "$work_dir/chown-file")" == "$(stat -c %u:%g "$work_dir/chown-reference")" ]] || fail 'chown reference ownership differs'
"$bin_dir/chown" --from=4294967294:4294967294 "$owner_uid:$owner_gid" "$work_dir/chown-file"
[[ "$(cat "$work_dir/chown-file")" == ownership ]] || fail 'chown --from modified file contents'
mkdir -p "$work_dir/chown-tree/first/second"
printf 'leaf\n' > "$work_dir/chown-tree/first/second/file"
"$bin_dir/chown" -R "$owner_uid:$owner_gid" "$work_dir/chown-tree"
[[ "$(stat -c %u:%g "$work_dir/chown-tree/first/second/file")" == "$owner_uid:$owner_gid" ]] || fail 'chown recursive ownership differs'
ln -s "$work_dir/chown-file" "$work_dir/chown-link"
"$bin_dir/chown" -h "$owner_uid:$owner_gid" "$work_dir/chown-link"
[[ "$(stat -c %u:%g "$work_dir/chown-link")" == "$owner_uid:$owner_gid" ]] || fail 'chown -h symlink ownership differs'

printf 'sync payload\n' > "$work_dir/sync-file"
"$bin_dir/sync"
"$bin_dir/sync" "$work_dir/sync-file"
"$bin_dir/sync" -d "$work_dir/sync-file"
"$bin_dir/sync" -f "$work_dir/sync-file"
if "$bin_dir/sync" "$work_dir/missing-sync-file" >/dev/null 2>&1; then
    fail 'sync accepted a missing file'
fi
if "$bin_dir/sync" -d -f "$work_dir/sync-file" >/dev/null 2>&1; then
    fail 'sync accepted mutually exclusive modes'
fi

printf 'stat payload\n' > "$work_dir/stat-file"
chmod 6754 "$work_dir/stat-file"
ln -s stat-file "$work_dir/stat-link"
stat_format='%a|%A|%b|%B|%d|%D|%Hd|%Ld|%f|%F|%g|%G|%h|%i|%n|%o|%s|%r|%R|%Hr|%Lr|%t|%T|%u|%U|%W|%X|%Y|%Z'
LC_ALL=C "$bin_dir/stat" -c "$stat_format" "$work_dir/stat-file" > "$work_dir/actual"
LC_ALL=C /usr/bin/stat -c "$stat_format" "$work_dir/stat-file" > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'stat numeric format output differs'
LC_ALL=C "$bin_dir/stat" -c '%a|%A|%F|%N' "$work_dir/stat-link" > "$work_dir/actual"
LC_ALL=C /usr/bin/stat -c '%a|%A|%F|%N' "$work_dir/stat-link" > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'stat symbolic-link output differs'
LC_ALL=C "$bin_dir/stat" -Lc '%a|%A|%F|%N' "$work_dir/stat-link" > "$work_dir/actual"
LC_ALL=C /usr/bin/stat -Lc '%a|%A|%F|%N' "$work_dir/stat-link" > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'stat -L output differs'
LC_ALL=C "$bin_dir/stat" -f -c '%l|%s|%S|%t|%T' "$work_dir/stat-file" > "$work_dir/actual"
LC_ALL=C /usr/bin/stat -f -c '%l|%s|%S|%t|%T' "$work_dir/stat-file" > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'stat file-system format output differs'
"$bin_dir/stat" --printf='name=%n\\nsize=%s\\t%%\n' "$work_dir/stat-file" > "$work_dir/actual"
/usr/bin/stat --printf='name=%n\\nsize=%s\\t%%\n' "$work_dir/stat-file" > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'stat --printf escapes differ'
if "$bin_dir/stat" "$work_dir/stat-missing" >/dev/null 2>&1; then
    fail 'stat accepted a missing file'
fi

mkdir -p "$work_dir/ls-tree/subdirectory"
printf 'a\n' > "$work_dir/ls-tree/alpha.txt"
printf 'bbbb\n' > "$work_dir/ls-tree/beta.log"
printf 'hidden\n' > "$work_dir/ls-tree/.hidden"
printf 'run\n' > "$work_dir/ls-tree/executable"
chmod 755 "$work_dir/ls-tree/executable"
ln -s alpha.txt "$work_dir/ls-tree/link"
printf 'nested\n' > "$work_dir/ls-tree/subdirectory/nested"
for ls_flags in -1 -a1 -A1 -r1 -F1 -p1 -S1 -X1 -v1 -R1; do
    LC_ALL=C "$bin_dir/ls" "$ls_flags" "$work_dir/ls-tree" > "$work_dir/actual"
    LC_ALL=C /usr/bin/ls "$ls_flags" "$work_dir/ls-tree" > "$work_dir/expected"
    cmp "$work_dir/actual" "$work_dir/expected" || fail "ls $ls_flags output differs"
done
LC_ALL=C "$bin_dir/ls" -d1 "$work_dir/ls-tree" > "$work_dir/actual"
LC_ALL=C /usr/bin/ls -d1 "$work_dir/ls-tree" > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'ls -d output differs'
LC_ALL=C "$bin_dir/ls" --zero "$work_dir/ls-tree" > "$work_dir/actual"
LC_ALL=C /usr/bin/ls --zero "$work_dir/ls-tree" > "$work_dir/expected"
cmp "$work_dir/actual" "$work_dir/expected" || fail 'ls --zero output differs'
if "$bin_dir/ls" "$work_dir/ls-missing" >/dev/null 2>&1; then
    fail 'ls accepted a missing operand'
fi

printf 'all coreutils smoke tests passed\n'
