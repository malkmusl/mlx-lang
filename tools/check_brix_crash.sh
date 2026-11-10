#!/usr/bin/env bash
# std.crash's reports in brixOS (std/src/crash.mlx), as good as the host's:
# tests/support/crash_report.mlx, the program tools/check_crash_report.sh
# crashes on the host, built for brixOS and run from flux in QEMU. brixOS
# has no /proc/self/maps and no /proc/self/comm and maps no files through
# its Linux layer; the report finds its way without them (the program's
# own file says where its segments are and what it is called, and is read
# into memory for its symbol and line tables):
#   - a store through address 0 three calls below main names the program,
#     the fault's address, the function, each caller and their lines, also
#     in a program that takes brixOS's events itself (std.events);
#   - a failed runtime check, an abort, a stack overflow (the recursion
#     said once) and a full value arena are said as on the host;
#   - a failure (crash.fail) is reported with its callers (fail calls the
#     handler: a signal with its reason cannot be queued there);
#   - what the program said it was doing is said after the reason, and
#     left out when its memory is gone (mincore says so);
#   - a fault in the handler itself still reports the original crash, its
#     frames as the file and the offset;
#   - each ends with the status it has on the host, and none made a
#     system call brixOS lacks.
#
#   tools/check_brix_crash.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}

if ! command -v qemu-system-x86_64 > /dev/null; then
    echo "skip brixOS: no qemu-system-x86_64"
    exit 0
fi

work=$(mktemp -d)
qemu_pid=
cleanup() {
    exec 3>&- 2> /dev/null || true
    [[ -n "$qemu_pid" ]] && kill "$qemu_pid" 2> /dev/null || true
    rm -rf -- "$work"
}
trap cleanup EXIT
fail() { echo "FAIL $1" >&2; [[ -f "${2:-$work/serial.txt}" ]] && cat -v "${2:-$work/serial.txt}" | tail -n 30 >&2; exit 1; }

brix=--target=x86_64-brixos
"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
"$compiler" --quiet $brix projects/shell/main.mlx -o "$work/flux"
"$compiler" --quiet $brix tests/support/crash_report.mlx -o "$work/crashy"
"$work/brix-image" "$work/kernel" "$work/brix.img" "flux=$work/flux" "crashy=$work/crashy"
echo "ok   the kernel, flux and the crashing program for brixOS, and a boot image build"

rm -f "$work/input"
mkfifo "$work/input"
: > "$work/serial.txt"
qemu-system-x86_64 -m 256M -display none -monitor none -no-reboot -cpu max \
    -serial stdio -kernel "$work/brix.img" < "$work/input" > "$work/serial.txt" 2>&1 &
qemu_pid=$!
exec 3> "$work/input"

# The serial output with the line editor's redraws taken out: carriage
# returns as line ends, erase-to-end-of-line sequences dropped.
output() { sed 's/\x1b\[K//g' "$work/serial.txt" | tr '\r' '\n'; }
# Waits (up to 30 s) until the output has $2 (default 1) lines matching $1.
wait_for() {
    local tries=0 want=${2:-1}
    while (( $(output | grep -c -- "$1" || true) < want )); do
        (( tries++ < 300 )) || fail "no \"$1\" (x$want) on the serial line"
        kill -0 "$qemu_pid" 2> /dev/null || fail "QEMU ended"
        sleep 0.1
    done
}
prompts=0
# Runs `crashy $1` at the next prompt; what it wrote goes to $work/$1.out,
# and it must end with the status $2 (the host's: 128 + the signal, 1 for
# a failure).
run() {
    prompts=$((prompts + 1))
    wait_for '^root@brix:[^#]*# $' "$prompts"
    printf '%s\r' "echo @@ $1; crashy $1; echo @@ $1 ended \$?" >&3
    wait_for "^@@ $1 ended [0-9]*\$"
    output | sed -n "/^@@ $1\$/,/^@@ $1 ended /p" | sed '1d' | grep -v '^$' > "$work/$1.out"
    [[ $(tail -n 1 "$work/$1.out") == "@@ $1 ended $2" ]] || fail "crashy $1 did not end with status $2" "$work/$1.out"
}

wait_for '^brixOS: kernel at 0x200000'

# The program, the fault's address (a page fault, whatever si_code the
# kernel gives it), the function with its line, and every caller.
run zero 139
grep -q '^crashy: crashed: segmentation fault touching 0x0$' "$work/zero.out" || fail "the store through address 0 is not said, or not by the program's name" "$work/zero.out"
grep -q '^  registers r8=0x[0-9a-f]* .* rsp=0x[0-9a-f]*$' "$work/zero.out" || fail "no registers with the fault" "$work/zero.out"
grep -q '^  at tests/support/crash_report.mlx:21:store+0x[0-9a-f]* at tests/support/crash_report.mlx:22$' "$work/zero.out" || fail "the fault is not placed in store, line 22" "$work/zero.out"
grep -q '^  from tests/support/crash_report.mlx:87:inner+0x[0-9a-f]* at tests/support/crash_report.mlx:89$' "$work/zero.out" || fail "inner (line 89) is not among the callers" "$work/zero.out"
grep -q '^  from tests/support/crash_report.mlx:77:outer+0x[0-9a-f]* at tests/support/crash_report.mlx:81$' "$work/zero.out" || fail "outer (line 81) is not among the callers" "$work/zero.out"
grep -q '^  from tests/support/crash_report.mlx:31:main+0x[0-9a-f]* at tests/support/crash_report.mlx:35$' "$work/zero.out" || fail "main (line 35) is not among the callers" "$work/zero.out"
grep -q '^  from _start+0x[0-9a-f]*' "$work/zero.out" || fail "_start is not among the callers" "$work/zero.out"
echo "ok   a store through address 0 three calls deep names the program, the function, its callers and their lines"

# A program that takes events of brixOS's itself (std.events: the kernel
# then gives it its events, not signals) still has its report: a fault it
# did not claim goes to the handler the report installed.
run events 139
grep -q '^crashy: crashed: segmentation fault touching 0x0$' "$work/events.out" || fail "a program that claimed an event has no crash report" "$work/events.out"
grep -q '^  at tests/support/crash_report.mlx:21:store+0x[0-9a-f]* at tests/support/crash_report.mlx:22$' "$work/events.out" || fail "the fault of a program that claimed an event is not placed in store" "$work/events.out"
grep -q '^  from tests/support/crash_report.mlx:31:main+0x[0-9a-f]* at tests/support/crash_report.mlx:35$' "$work/events.out" || fail "main is not among the callers of a program that claimed an event" "$work/events.out"
echo "ok   a program that takes brixOS's events itself keeps its crash report"

run index 132
grep -q '^crashy: crashed: illegal instruction (a failed runtime check' "$work/index.out" || fail "no crash line" "$work/index.out"
grep -q '^  at tests/support/crash_report.mlx:16:middle+0x[0-9a-f]* at tests/support/crash_report.mlx:13$' "$work/index.out" || fail "the failed check is not placed in middle, line 13 (pick, inlined)" "$work/index.out"
grep -q '^  from tests/support/crash_report.mlx:31:main+0x[0-9a-f]* at tests/support/crash_report.mlx:38$' "$work/index.out" || fail "main (line 38) is not among the callers" "$work/index.out"
grep -q '^  while ' "$work/index.out" && fail "a program that said no context has a while line" "$work/index.out"
echo "ok   a failed runtime check names its function, the callers and their lines"

run abort 134
grep -q '^crashy: crashed: aborted' "$work/abort.out" || fail "an abort is not reported" "$work/abort.out"
grep -q '^  from tests/support/crash_report.mlx:[0-9]*:main+0x' "$work/abort.out" || fail "the abort does not name main" "$work/abort.out"
echo "ok   an abort is reported too"

# The reason cannot come with a queued signal here (no rt_tgsigqueueinfo):
# fail() calls the handler itself, from its own frame, and asks the kernel
# for nothing.
run fail 1
grep -q '^crashy: failed: the compositor ended the connection (a test)$' "$work/fail.out" || fail "a failure (crash.fail) is not reported" "$work/fail.out"
grep -q '^  at tests/support/crash_report.mlx:[0-9]*:main+0x' "$work/fail.out" || fail "the failure does not name main" "$work/fail.out"
grep -q ':fail+0x' "$work/fail.out" && fail "the failure's report starts in fail itself" "$work/fail.out"
echo "ok   a failure the program cannot go on after (crash.fail) is reported with its callers"

run deep 139
grep -q 'the stack ran out' "$work/deep.out" || fail "the overflow is not recognised" "$work/deep.out"
grep -q '^  from tests/support/crash_report.mlx:25:deeper+0x[0-9a-f]* at tests/support/crash_report.mlx:28 ([0-9]* more times)$' "$work/deep.out" || fail "the recursion is not said once" "$work/deep.out"
echo "ok   a stack overflow names the recursion once"

# The arena is found without /proc/self/maps (std.turns asks the kernel
# page by page), and the handler's own values have room in it: what it
# maps lies above the arena here.
run values 132
grep -q "^crashy: crashed: illegal instruction (the values' arena is full" "$work/values.out" || fail "a full value arena is not said" "$work/values.out"
grep -q '^  at tests/support/crash_report.mlx:[0-9]*:hoard+0x' "$work/values.out" || fail "the full arena does not name hoard" "$work/values.out"
grep -q '^  from tests/support/crash_report.mlx:[0-9]*:hoardAll+0x' "$work/values.out" || fail "the full arena does not name hoard's caller" "$work/values.out"
echo "ok   a full value arena is said, in the function that asked for more"

run context 132
[[ $(sed -n 2p "$work/context.out") == "  while compiling src/widget.c:12" ]] || fail "the context is not said after the reason" "$work/context.out"
grep -q '^  at tests/support/crash_report.mlx:16:middle+0x' "$work/context.out" || fail "the crash with a context does not name middle" "$work/context.out"
run where 139
grep -q '^crashy: crashed: segmentation fault touching 0x10$' "$work/where.out" || fail "the crash with a gone context has no crash line" "$work/where.out"
grep -q '^  while ' "$work/where.out" && fail "a context in unmapped memory was read" "$work/where.out"
grep -q '^  at tests/support/crash_report.mlx:21:store+0x' "$work/where.out" || fail "the crash with a gone context does not name store" "$work/where.out"
grep -q '^  from tests/support/crash_report.mlx:[0-9]*:gone+0x' "$work/where.out" || fail "the crash with a gone context does not name gone" "$work/where.out"
echo "ok   what the program was doing is said after the reason, and left out when its memory is gone"

run broken 132
grep -q '^crashy: crashed: illegal instruction (a failed runtime check' "$work/broken.out" || fail "a fault in the handler loses the original crash" "$work/broken.out"
grep -q '^  at crashy+0x[0-9a-f]*$' "$work/broken.out" || fail "a fault in the handler does not give the frames as file and offset" "$work/broken.out"
grep -q '^  from crashy+0x[0-9a-f]*$' "$work/broken.out" || fail "a fault in the handler drops the callers" "$work/broken.out"
grep -q '^  (the report failed naming the frames: signal 11 at [^ ]* in the crash handler; the frames from there are unnamed)$' "$work/broken.out" || fail "a fault in the handler is not said" "$work/broken.out"
echo "ok   a fault in the crash handler itself still reports the original crash, its frames unnamed"

# The shell is still there after nine programs crashed under it.
prompts=$((prompts + 1))
wait_for '^root@brix:[^#]*# $' "$prompts"
printf '%s\r' 'echo still $((6 * 7))' >&3
wait_for '^still 42$'
echo "ok   flux goes on after its programs' crashes"

if output | grep -q '^\[brixOS: system call'; then
    fail "system calls the reports made are missing: $(output | grep '^\[brixOS: system call' | tr '\n' ' ')"
fi
echo "ok   no system call the reports made was missing"
