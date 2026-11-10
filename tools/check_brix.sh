#!/usr/bin/env bash
# brixOS (projects/brix) end to end: the kernel (--target=x86_64-freestanding)
# and brix-image build, the image boots in QEMU (the test machine; nothing
# of it is in what is built), the kernel says itself on the serial line and
# starts flux from the boot archive in user mode; commands typed at its
# prompt run (builtins, arithmetic, the working directory, a glob over the
# archive's programs in /system/programs: brixOS's own layout); programs of
# the archive run (fork, execve, wait4, the child's pages shared copy on
# write),
# also in pipelines, command substitutions, subshells and in the
# background, with what the child changes staying its own; files are made,
# written, appended to, moved, copied, linked and removed in the tmpfs, with
# directories, the working directory, the umask, the devices; Linux
# programs' /tmp, /dev and /etc/passwd lead to /temporary, /devices and
# /system/settings; a copy shares the pages of its file until it is
# written to (copy on write); snapshots of a tree (read-only or not, also
# of / into itself) share the files' pages, stay apart from the tree when
# either changes, refuse changes when read-only, and are taken out; a
# virtio disk is found and read and written as /devices/disk0 (what is
# written reaches the image); brixfs formats it in brixOS and the kernel
# mounts it at /volumes/disk0: files and directories written there reach
# the disk (the host's brixfs lists and reads them), a snapshot on the disk
# keeps the old state; after a second boot both are there, the snapshot
# is taken out and its blocks are free again (links and FIFOs on the disk
# too), every coreutils program runs as on the host, an appended file keeps its
# unchanged blocks, inode numbers are used again, and more files than
# the tree has nodes for are there (unloaded, read again); a script runs (by
# /proc/self/exe), a program
# copied into /temporary runs, and a program of /system/programs written
# to is copied up; Ctrl-C ends the foreground program (a busy one too: the
# serial line's interrupt), the timer shares the processor (each process
# keeps its SSE registers); signals behave as on the host (handlers, masks,
# EINTR, sigsuspend, a fault on the alternate stack; the shell's trap);
# job control works (Ctrl-Z, fg, bg, SIGTTIN, kill -STOP and -CONT);
# Ctrl-D ends its input;
# `exit 3` ends the shell and the kernel starts it again; and no system
# call the programs made was missing.
#
#   tools/check_brix.sh [compiler]
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
fail() { echo "FAIL $1" >&2; [[ -f "$work/serial.txt" ]] && cat -v "$work/serial.txt" | tail -n 30 >&2; exit 1; }

# What goes into the image is built for brixOS (--target=x86_64-brixos:
# `@import("builtin").os` is brixos there); the tools the host runs too
# (brixfs, signals, floats: the host's runs are what brixOS's must match)
# also for Linux.
brix=--target=x86_64-brixos
mkdir -p "$work/bin" "$work/linux-tools"
"$compiler" --quiet $brix projects/shell/main.mlx -o "$work/flux"
shell=$work/flux
"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
# Every coreutils program (built here: what is in mlx-out may be older).
programs=()
for main in projects/coreutils/*/main.mlx; do
    programs+=("$(basename "$(dirname "$main")")")
done
printf '%s\n' "${programs[@]}" | xargs -P "$(nproc)" -I{} "$compiler" --quiet $brix "projects/coreutils/{}/main.mlx" -o "$work/bin/{}"
# The host's run of them (std on brixOS reads, writes and ends by brixOS's
# own calls, which Linux does not have): the same sources built for Linux.
mkdir -p "$work/linux-tools/bin"
printf '%s\n' "${programs[@]}" | xargs -P "$(nproc)" -I{} "$compiler" --quiet "projects/coreutils/{}/main.mlx" -o "$work/linux-tools/bin/{}"
"$compiler" --quiet projects/shell/main.mlx -o "$work/linux-tools/flux"
(( ${#programs[@]} >= 73 )) || fail "only ${#programs[@]} coreutils programs"
for tool in snapshot brixfs floats signals native events objects kinds; do
    "$compiler" --quiet $brix "projects/brix/tools/$tool/main.mlx" -o "$work/$tool"
done
for tool in brixfs floats signals; do
    "$compiler" --quiet "projects/brix/tools/$tool/main.mlx" -o "$work/linux-tools/$tool"
done
"$work/brix-image" "$work/kernel" "$work/brix.img" "flux=$shell" "snapshot=$work/snapshot" "brixfs=$work/brixfs" "floats=$work/floats" "signals=$work/signals" "native=$work/native" "events=$work/events" "objects=$work/objects" "kinds=$work/kinds" "linux-cat=$work/linux-tools/bin/cat" "linux-ls=$work/linux-tools/bin/ls" "every-program=tools/brix_programs.sh" "$work/bin/"
echo "ok   the kernel, brix-image and the boot image (flux and ${#programs[@]} coreutils programs) build"

# A disk of 4 MiB that starts with a text of the host's.
head -c 4194304 /dev/zero > "$work/disk.img"
printf 'brix disk' | dd of="$work/disk.img" conv=notrunc status=none

# brixOS in QEMU with the disk, typed at through a FIFO; the serial line
# into serial.txt. Booted again by boot, after stop.
boot() {
    rm -f "$work/input"
    mkfifo "$work/input"
    : > "$work/serial.txt"
    qemu-system-x86_64 -m 256M -display none -monitor none -no-reboot -cpu max \
        -serial stdio -kernel "$work/brix.img" -drive "file=$work/disk.img,format=raw,if=virtio" < "$work/input" > "$work/serial.txt" 2>&1 &
    qemu_pid=$!
    exec 3> "$work/input"
    prompts=0
}
stop() {
    exec 3>&-
    kill "$qemu_pid" 2> /dev/null || true
    wait "$qemu_pid" 2> /dev/null || true
    qemu_pid=
}
boot

# The serial output with the line editor's redraws taken out: carriage
# returns as line ends, erase-to-end-of-line sequences dropped.
output() { sed 's/\x1b\[K//g' "$work/serial.txt" | tr '\r' '\n'; }
# Waits (up to 20 s) until the output has $2 (default 1) lines matching $1.
wait_for() {
    local tries=0 want=${2:-1}
    while (( $(output | grep -c -- "$1" || true) < want )); do
        (( tries++ < 200 )) || fail "no \"$1\" (x$want) on the serial line"
        kill -0 "$qemu_pid" 2> /dev/null || fail "QEMU ended"
        sleep 0.1
    done
}
prompts=0
# Types $1 and Enter at the next prompt.
type_line() {
    prompts=$((prompts + 1))
    wait_for '^root@brix:[^#]*# $' "$prompts"
    printf '%s\r' "$1" >&3
}
# Types $1 (printf's format) to the program that runs, after a moment.
type_to_program() {
    sleep 0.5
    printf "$1" >&3
}

wait_for '^brixOS: kernel at 0x200000'
wait_for '^brixOS: [0-9]* MiB ([0-9]* free), [0-9]* MHz$'
wait_for '^brixOS: /devices/disk0, 4096 KiB (virtio)$'
wait_for '^brixOS: disk0 answers by interrupt$'
echo "ok   the kernel boots and says itself on the serial line"

type_line 'echo hello brix'
wait_for '^hello brix$'
echo "ok   flux runs in user mode and its builtins answer"

type_line 'echo $((6 * 7)) $(( 1 << 10 ))'
wait_for '^42 1024$'
echo "ok   arithmetic expansion"

type_line 'pwd; cd /; echo * /system/* /system/programs/flux /system/programs/m*'
wait_for '^/users/root$'
wait_for '^devices system temporary users volumes /system/programs /system/settings /system/programs/flux /system/programs/md5sum /system/programs/mkdir /system/programs/mkfifo /system/programs/mv$'
echo "ok   the working directory and a glob over the tree (brixOS's layout)"

type_line 'x=brix; echo ${x}OS ${#x}'
wait_for '^brixOS 4$'
echo "ok   variables"

type_line 'uname -s; uname -o; echo hello | cat; seq 5 | wc -l'
wait_for '^brixOS$' 2
wait_for '^hello$'
wait_for '^5$'
echo "ok   programs of the archive run, also in pipelines; built for brixOS, uname -o says so"

type_line 'ls /system/programs | wc -l; cat /system/programs/flux | wc -c'
wait_for "^$(( ${#programs[@]} + 12 ))\$"
wait_for "^$(stat -c %s "$shell")\$"
echo "ok   ls lists the archive, a program reads a file through a pipe"

type_line 'false; echo status $?; nosuch; echo $(echo sub) $(( $(seq 4 | wc -l) * 2 )); (echo inner; exit 4); echo sub $?'
wait_for '^status 1$'
wait_for 'nosuch: command not found'
wait_for '^sub 8$'
wait_for '^inner$'
wait_for '^sub 4$'
echo "ok   exit statuses, a missing program, command substitutions, subshells"

type_line 'cd /temporary && echo hello > a && echo more >> a && cat a | wc -l && mkdir -p d/e && mv a d/e/b && cp d/e/b c && ln c l && ls d/e && stat -c "%s %h" l'
wait_for '^2$'
wait_for '^b$'
wait_for '^11 2$'
type_line 'rm -r d c l && ls -A; pwd; echo gone'
wait_for '^/temporary$'
wait_for '^gone$'
echo "ok   files and directories in the tmpfs: write, append, move, copy, link, remove"

type_line 'umask 077; echo z > f; stat -c %a f; mkdir g; stat -c %a g; echo x > /devices/null; head -c 5 /devices/zero | wc -c'
wait_for '^600$'
wait_for '^700$'
wait_for '^5$'
echo "ok   the umask, /devices/null, /devices/zero"

type_line 'echo linux > /temporary/l; linux-cat /tmp/l; linux-cat /dev/null; linux-cat /etc/passwd; linux-ls /dev'
wait_for '^linux$'
wait_for '^root:x:0:0:root:/users/root:/system/programs/flux$'
wait_for '^console  disk0[[:space:]]*framebuffer  keyboard  klog  mouse  null  tty  zero$'
echo "ok   Linux programs' /tmp, /dev and /etc/passwd"

type_line 'cat /system/programs/flux > big; before=$(stat -f -c %f .); cp big copy; after=$(stat -f -c %f .); echo shared $(( before - after < 64 )) $(wc -c < copy)'
wait_for "^shared 1 $(stat -c %s "$shell")\$"
type_line 'echo more >> copy; wc -c < big; rm big copy; x=1; (x=2; echo child $x); echo parent $x'
wait_for "^$(stat -c %s "$shell")\$" 2
wait_for '^child 2$'
wait_for '^parent 1$'
echo "ok   copy on write: a copy shares its file's pages, a fork's child its parent's"

type_line 'mkdir -p w/sub; echo one > w/a; echo two > w/sub/b; snapshot -r w snap && snapshot w copy && echo changed > w/a && echo edit >> copy/sub/b && cat snap/a copy/a w/a copy/sub/b w/sub/b'
wait_for '^one$' 2
wait_for '^changed$'
wait_for '^edit$'
wait_for '^two$' 2
type_line 'echo x > snap/a; rm -f snap/sub/b; cat snap/sub/b snap/a; snapshot / /temporary/everything && ls /temporary/everything/temporary/snap'
wait_for 'snap/a'
wait_for "rm: cannot remove 'snap/sub/b'"
wait_for '^two$' 3
wait_for '^one$' 3
wait_for '^a  sub$'
type_line 'snapshot -d snap && snapshot -d everything && snapshot -d copy && ls snap copy 2>&1 | wc -l'
wait_for '^2$' 2
echo "ok   snapshots: shared pages, apart when changed, read-only, of / into itself, taken out"

type_line 'head -c 9 /devices/disk0; echo; stat -c "%F %s" /devices/disk0; echo written by brixOS > /devices/disk0; head -c 17 /devices/disk0; echo'
wait_for '^brix disk$'
wait_for '^block special file 4194304$'
wait_for '^written by brixOS$'
[[ "$(head -c 17 "$work/disk.img")" == "written by brixOS" ]] || fail "what brixOS wrote is not in the disk image"
echo "ok   the virtio disk: /devices/disk0 read and written, the image has it"

type_line 'brixfs format /devices/disk0 && cd /volumes/disk0 && mkdir docs && echo hello volume > docs/a && seq 2000 > numbers && cp /system/programs/flux big && sync && ls; cd /temporary'
wait_for '^brixfs: mounted at /volumes/disk0$'
wait_for '^big  docs  numbers$'
"$work/linux-tools/brixfs" list "$work/disk.img" > "$work/list" 2>&1 || fail "the host's brixfs cannot read the volume: $(cat "$work/list")"
grep -q '^/docs/a 13$' "$work/list" && grep -q '^/numbers 8893$' "$work/list" && grep -q "^/big $(stat -c %s "$shell")\$" "$work/list" || fail "the volume lacks what was written: $(cat "$work/list")"
[[ "$("$work/linux-tools/brixfs" cat "$work/disk.img" docs/a)" == "hello volume" ]] || fail "the host reads another docs/a"
type_line 'snapshot -r /volumes/disk0 /volumes/before && rm /volumes/disk0/big && echo changed > /volumes/disk0/docs/a && sync && ls /volumes && cat /volumes/before/docs/a /volumes/disk0/docs/a; echo x > /volumes/before/x'
wait_for '^before[[:space:]]*disk0$'
wait_for '^hello volume$'
wait_for '^changed$'
wait_for 'before/x'
"$work/linux-tools/brixfs" list "$work/disk.img" > "$work/list" 2>&1
grep -q '^snapshot before$' "$work/list" && ! grep -q '^/big ' "$work/list" || fail "the snapshot or the removal is not on the disk: $(cat "$work/list")"
type_line 'ln -s docs/a /volumes/disk0/toa && mkfifo /volumes/disk0/pipe && sync && echo linked $(cat /volumes/disk0/toa)'
wait_for '^linked changed$'
"$work/linux-tools/brixfs" list "$work/disk.img" > "$work/list" 2>&1
grep -q '^/toa -> docs/a$' "$work/list" && grep -q '^/pipe fifo$' "$work/list" || fail "the link or the FIFO is not on the disk: $(cat "$work/list")"
echo "ok   brixfs: formatted and mounted, files on the disk (the host reads them), a read-only snapshot on it"

# Every coreutils program as on the host: tools/brix_programs.sh's output
# in brixOS and on the host (the same sources, built for each) the same.
mkdir "$work/host"
host_shell=$(realpath "$work/linux-tools/flux")
(cd "$work/host" && env -i T="$work/host" PATH="$work/linux-tools/bin" HOME=/users/root "$host_shell" "$repo_root/tools/brix_programs.sh" < /dev/null > "$work/programs.host" 2>&1) || true
type_line 'flux /system/programs/every-program < /devices/null'
tries=0
until output | grep -q '^@@ done$'; do
    (( tries++ < 1200 )) || fail "tools/brix_programs.sh did not end in brixOS"
    sleep 0.1
done
output | sed -n '/^@@ env \[/,/^@@ done$/p' | grep -v '^$' > "$work/programs.brix"
grep -v '^$' "$work/programs.host" | diff - "$work/programs.brix" > "$work/programs.diff" || fail "programs differ from the host's run (< host, > brixOS): $(cat "$work/programs.diff")"
echo "ok   all ${#programs[@]} coreutils programs run as on the host (links, FIFOs included)"

# Two programs at once that compute in the SSE registers for many time
# slices, each keeping its own.
type_line 'floats > /temporary/f1 & floats > /temporary/f2; wait; cat /temporary/f1 /temporary/f2'
sum=$("$work/linux-tools/floats")
wait_for "^$sum$" 2
echo "ok   two programs at once keep their own SSE registers"

# Signals: handlers and their return, masks, sigtimedwait, default and
# ignored actions, EINTR and SA_RESTART, sigsuspend, a fault on the
# alternate stack; as on the host.
type_line 'echo @@ signals; signals; echo @@ end'
wait_for '^@@ end$'
"$work/linux-tools/signals" > "$work/signals.host"
output | sed -n '/^@@ signals$/,/^@@ end$/p' | sed '1d;$d' | grep -v '^$' > "$work/signals.brix"
diff "$work/signals.host" "$work/signals.brix" > "$work/signals.diff" || fail "signals differ from the host's (< host, > brixOS): $(cat "$work/signals.diff")"
# brixOS's own calls (std.os.brix): handles, the one wait, positioned
# and streaming reads and writes, the clock, process_exit.
type_line 'echo @@ native; native; echo @@ end $?'
wait_for '^@@ end 7$'
cat > "$work/native.expected" <<'END'
native calls
output kind (session) 6
output writable
pipe kind (stream) 4
system info words 8
memory, free and processors make sense
wait now (timed out) 5
wait 50 ms (timed out) 5
the deadline held
ready after a write 1
seen (readable) 1
read 1
it was x
seen after the writer closed (readable, peer gone) 5
brix!
hello
file kind (node) 3
the closed writer's handle (invalid handle) 1
the file is in the writer's slot
under another handle
close twice (invalid handle) 1
read from a closed handle (invalid handle) 1
real time after 2023
start roles 4
input is descriptor 0
output is descriptor 1
errors is descriptor 2
session kind (session) 6
no directory role
END
output | sed -n '/^@@ native$/,/^@@ end 7$/p' | sed '1d;$d' | grep -v '^$' > "$work/native.brix"
diff "$work/native.expected" "$work/native.brix" > "$work/native.diff" || fail "the native calls answer otherwise (< expected, > brixOS): $(cat "$work/native.diff")"
echo "ok   brixOS's own calls: handles, the one wait with deadlines, positioned and streaming reads and writes, the clock, process_exit"

# brixOS's events: the queue, the upcall, a wait it interrupts, a default
# action, a fault through the upcall.
type_line 'echo @@ events; events; echo @@ events end $?'
wait_for '^@@ events end 0$'
cat > "$work/events.expected" <<'END'
events
queued 1
its kind (user) 13
its signal 10
sent by itself
upcalls 1
the upcall's signal 12
wait interrupted 12
upcalls 2
child ended by signal 15
fault through the upcall, exception 14
faulting child exited 9
END
output | sed -n '/^@@ events$/,/^@@ events end 0$/p' | sed '1d;$d' | grep -v '^$' > "$work/events.brix"
diff "$work/events.expected" "$work/events.brix" > "$work/events.diff" || fail "the events go otherwise (< expected, > brixOS): $(cat "$work/events.diff")"
echo "ok   brixOS's events: the queue, the upcall and its return, an interrupted wait, a default action, a fault through the upcall"

# brixOS's own nodes, streams, channels and processes.
type_line 'echo @@ objects; objects; echo @@ objects end $?'
wait_for '^@@ objects end 0$'
cat > "$work/objects.expected" <<'END'
objects
make a directory 0
file kind 1
file size 11
open it again exclusively (exists) 7
a link 0
a fifo 0
native (through the link)
  entry a.txt kind 1
  entry to-a kind 3
  entry pipe kind 4
entries 3
remove a full directory (not empty) 15
open a missing one (not found) 6
streamed
send 0
the sent handle here (invalid handle) 1
receive 0
hello channel
handles received 1
native file (through the received handle)
receive again (would block) 4
receive after the peer closed (peer gone) 11
the branch exited 1
with 5
nothing yet (would block) 4
the other ended by an event 2
of kind (terminate) 5
the spawned echo exited 1
with 0
spawned
END
output | sed -n '/^@@ objects$/,/^@@ objects end 0$/p' | sed '1d;$d' | grep -v '^$' > "$work/objects.brix"
diff "$work/objects.expected" "$work/objects.brix" > "$work/objects.diff" || fail "the objects behave otherwise (< expected, > brixOS): $(cat "$work/objects.diff")"
echo "ok   brixOS's own nodes, streams, channels (bytes and a handle), branch, spawn with the handles given, events sent, changes taken"

# brixOS's other objects: a rename, a snapshot by handle, timers, a
# memory object shared with a branch, groups and the session.
type_line 'echo @@ kinds; kinds; echo @@ kinds end $?'
wait_for '^@@ kinds end 0$'
cat > "$work/kinds.expected" <<'END'
kinds
rename 0
the old name (not found) 6
renamed file
a file onto a directory (is a directory) 14
a directory under itself (invalid) 10
snapshot 0
renamed file (in the snapshot)
written in the snapshot (read only) 16
the same name again (exists) 7
timer kind 8
readable before its deadline 0
read (bytes) 8
waited for it 1
expired at least once 1
waited for again 1
expired again 1
once: read (bytes) 8
once: again (no more) 0
a write to a timer (no right) 3
memory kind 7
zeroed 0
from the branch (the branch wrote it)
from the branch (its second page, mapped again)
past its end (invalid) 10
from the branch (still mapped after its handle closed)
session kind 6
own group in front 0
group kind 2
in front 1
the branch's own group in front 0
put in front 0
now in front 1
ours still 0
ours back 0
an event to their group 0
the branch ended by an event 2
of kind (terminate) 5
their group ended 1
an event to it (not found) 6
a group in front of a file (wrong kind) 2
by std.spawn
its ending 0
started by a handle 1
asked to end 1
ended by an event 1
of kind (terminate) 5
into a directory 1
/temporary/kinds (the working directory)
a file there by its name alone 1
piped (through std's pipe)
a handler 1
handled 1
of kind (user) 13
a handler for kill (no) 0
second page (mapped)
first page (the file, after a write to the view)
First page (the view)
ELF (a program of the archive, mapped)
a directory mapped (wrong kind) 2
END
output | sed -n '/^@@ kinds$/,/^@@ kinds end 0$/p' | sed '1d;$d' | grep -v '^$' > "$work/kinds.brix"
diff "$work/kinds.expected" "$work/kinds.brix" > "$work/kinds.diff" || fail "the other objects behave otherwise (< expected, > brixOS): $(cat "$work/kinds.diff")"
echo "ok   brixOS's other objects: rename, a snapshot by handle, timers, a memory object shared with a branch, groups in front of the session; std.spawn by handles; std's working directory, pipes and std.events; files mapped"

type_line "trap 'echo trapped' USR1; kill -USR1 \$\$; echo after trap; trap - USR1"
wait_for '^trapped$'
wait_for '^after trap$'
echo "ok   signals: handlers, masks, sigtimedwait, EINTR and SA_RESTART, sigsuspend, a fault on the alternate stack, the shell's trap"

type_line 'echo "echo script \$1" > s.sh; chmod +x s.sh; ./s.sh one; flux s.sh two'
wait_for '^script one$'
wait_for '^script two$'
type_line 'cp /system/programs/uname /temporary/u; /temporary/u -s; echo copied > /system/programs/seq; cat /system/programs/seq'
wait_for '^brixOS$' 2
wait_for '^copied$'
echo "ok   scripts, a program copied into /temporary, a program written to copied up"

type_line 'sleep 1 & wait; echo waited'
wait_for '^waited$'
echo "ok   a background job and wait"

type_line 'cat'
type_to_program 'typed\r'
wait_for '^typed$' 2
type_to_program '\003'
type_line 'echo after $?'
wait_for '^after 130$'
echo "ok   Ctrl-C ends the foreground program (SIGINT)"

# The timer and the serial line's interrupts: a program that never waits
# is ended by Ctrl-C; a busy one in the background leaves time to the
# others.
type_line 'yes > /devices/null'
type_to_program '\003'
type_line 'echo busy $?'
wait_for '^busy 130$'
type_line 'yes > /devices/null & sleep 1; echo the shell runs; kill $!; wait $!; echo ended $?'
wait_for '^the shell runs$'
wait_for '^ended 143$'
echo "ok   preemption: Ctrl-C ends a busy program, a busy one in the background leaves time to the shell"

# Job control: Ctrl-Z stops the program in front and fg brings it back;
# bg lets a stopped one go on behind; one behind that reads the terminal
# stops (SIGTTIN) until fg; kill -STOP and -CONT.
type_line 'cat'
type_to_program 'one\r'
wait_for '^one$' 2
type_to_program '\032'
wait_for '^\[1\]+ Stopped  cat$'
type_line 'fg'
type_to_program 'two\r'
wait_for '^two$' 2
type_to_program '\004'
type_line 'echo fg $?'
wait_for '^fg 0$'
type_line 'yes > /devices/null'
type_to_program '\032'
wait_for '^\[1\]+ Stopped  yes > /devices/null$'
type_line 'bg; jobs; kill %1; wait; echo bg done'
wait_for '^\[1\]+ Running  yes > /devices/null$'
wait_for '^bg done$'
type_line 'cat &'
type_line 'sleep 1; jobs'
wait_for '^\[1\]+ Stopped  cat$' 2
type_line 'fg'
type_to_program 'three\r'
wait_for '^three$' 2
type_to_program '\004'
type_line 'sleep 30 & kill -STOP $!; sleep 1; jobs; kill -CONT $!; jobs; kill $!; wait; echo stop done'
wait_for '^\[1\]+ Stopped  sleep 30$'
wait_for '^\[1\]+ Running  sleep 30$'
wait_for '^stop done$'
echo "ok   job control: Ctrl-Z and fg, bg, a read behind stops (SIGTTIN), kill -STOP and -CONT"

type_line 'wc -l'
type_to_program 'a\rb\r'
type_to_program '\004'
wait_for '^2$'
echo "ok   Ctrl-D ends a program's input"

type_line 'exit 3'
wait_for '^brixOS: the first process ended (3)$'
type_line 'echo again'
wait_for '^again$'
echo "ok   exit ends the shell and the kernel starts it again"

if output | grep -q '^\[brixOS: system call'; then
    fail "system calls the programs made are missing: $(output | grep '^\[brixOS: system call' | tr '\n' ' ')"
fi
echo "ok   no system call the programs made was missing"

# The second boot: the volume and its snapshot as they were committed.
stop
boot
wait_for '^brixOS: /volumes/disk0 (brixfs, generation [0-9]*)$'
type_line 'ls /volumes; cat /volumes/disk0/docs/a /volumes/before/docs/a; tail -n 1 /volumes/disk0/numbers; ls /volumes/before; echo free $(stat -f -c %f /volumes/disk0); readlink /volumes/disk0/toa; stat -c %F /volumes/disk0/pipe'
wait_for '^before[[:space:]]*disk0$'
wait_for '^changed$'
wait_for '^docs/a$'
wait_for '^fifo$'
wait_for '^hello volume$'
wait_for '^2000$'
wait_for '^big  docs  numbers$'
wait_for '^free [0-9]*$'
free_before=$(output | grep '^free [0-9]*$' | tail -n 1 | cut -d' ' -f2)
type_line 'snapshot -d /volumes/before && ls /volumes && echo free $(stat -f -c %f /volumes/disk0)'
wait_for '^disk0$'
wait_for '^free [0-9]*$' 2
free_after=$(output | grep '^free [0-9]*$' | tail -n 1 | cut -d' ' -f2)
(( free_after > free_before + 100 )) || fail "the snapshot's blocks are not free again ($free_before, then $free_after)"
"$work/linux-tools/brixfs" list "$work/disk.img" > "$work/list" 2>&1
! grep -q '^snapshot ' "$work/list" || fail "the snapshot is still on the disk"
echo "ok   brixfs after a reboot: the files, a link, a FIFO and the snapshot there, the snapshot taken out, its blocks free"

# Only what changed is written: a file of 144 blocks appended to keeps
# every block but its last on the disk.
type_line 'cd /volumes/disk0 && seq 100000 > large && sync && echo written'
wait_for '^written$'
"$work/linux-tools/brixfs" blocks "$work/disk.img" large > "$work/blocks.before" 2>&1 || fail "the host cannot list large's blocks: $(cat "$work/blocks.before")"
[[ $(wc -l < "$work/blocks.before") == 144 ]] || fail "large has $(wc -l < "$work/blocks.before") blocks, not 144"
type_line 'echo 100001 >> large && sync && tail -n 1 large'
wait_for '^100001$'
"$work/linux-tools/brixfs" blocks "$work/disk.img" large > "$work/blocks.after" 2>&1
cmp -s <(head -n 143 "$work/blocks.before") <(head -n 143 "$work/blocks.after") || fail "blocks of large that did not change were written again"
[[ "$(tail -n 1 "$work/blocks.before")" != "$(tail -n 1 "$work/blocks.after")" ]] || fail "the changed block of large was written in place"
# A removed file's inode number is used again.
type_line 'echo a > r1 && sync && i=$(stat -c %i r1) && rm r1 && sync && echo b > r2 && echo reused $(( $(stat -c %i r2) == i ))'
wait_for '^reused 1$'
echo "ok   brixfs: an appended file's unchanged blocks stay, a removed file's inode number is used again"

# More files than the tree has nodes for: directories read from the disk
# leave memory when the system is idle and are read again when used.
type_line 'mkdir m1 m2 m3 && for i in $(seq 400); do : > m1/f$i; done; echo seven > m1/f7; echo made m1'
wait_for '^made m1$'
type_line 'for i in $(seq 400); do : > m2/f$i; done; echo made m2'
wait_for '^made m2$'
type_line 'for i in $(seq 400); do : > m3/f$i; done; echo made m3'
wait_for '^made m3$'
type_line 'cat m1/f7; ls m1 | wc -l'
wait_for '^seven$'
wait_for '^400$'
type_line 'ls m2 | wc -l'
wait_for '^400$' 2
type_line 'ls m3 | wc -l; rm -r m1 m2 m3 && echo removed'
wait_for '^400$' 3
wait_for '^removed$'
echo "ok   brixfs: more files than nodes in memory (directories unloaded when idle, read again)"

# The volume checked: in brixOS on the device, on the host on the image.
type_line 'sync; brixfs check /devices/disk0'
wait_for '^brixfs: the volume is sound$'
"$work/linux-tools/brixfs" check "$work/disk.img" > "$work/check.txt" 2>&1 || fail "the host's brixfs check finds something: $(cat "$work/check.txt")"
grep -q '^brixfs: the volume is sound$' "$work/check.txt" || fail "the host's brixfs check does not say the volume is sound"
echo "ok   brixfs check: the volume is sound, in brixOS and on the host"
