#!/usr/bin/env bash
# mlx-sh (projects/shell) end to end:
#   - `-c` snippets against dash: words, quotes, parameters and their
#     operators, arithmetic, command substitution, globbing, pipelines,
#     redirections and heredocs, lists, if/for/while/until/case, functions,
#     builtins, exit statuses (stdout and the status must agree);
#   - what dash lacks (substrings, local, history) against expectations;
#   - a script file (by path, by shebang, from stdin, with -s) against dash;
#   - --parse dumps the tree (and says what is incomplete or wrong);
#   - the line editor through a pty: cursor keys, Ctrl-A/K, history recall,
#     completion, Ctrl-C, Ctrl-D, the history file.
#
#   tools/check_shell.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; [[ -f "${2:-}" ]] && cat "$2" >&2; exit 1; }

"$compiler" --quiet projects/shell/main.mlx -o "$work/mlx-sh"
sh=$work/mlx-sh
echo "ok   mlx-sh builds"
grep -q mlxCompositorCrashed "$sh" || fail "mlx-sh has no crash handler"

# --- -c snippets, against dash.
command -v dash > /dev/null || fail "dash is needed for the comparison"
export HOME=$work/home
mkdir -p "$HOME"
cd "$work"
printf 'one\ntwo\n' > two.txt
touch alpha.txt beta.txt gamma.md
snippets=(
    'echo hello world'
    "echo 'single \$x' \"double \\\$x\" plain\\ escaped"
    'x=5; echo "$x" | cat; echo ${x}y "${x}z" $x$x'
    'x=1 y=2; echo "$x-$y"; z=3 env | grep ^z=; echo "[$z]"'
    'echo $((1+2*3)) $((0x10 + 010 + 7 % 3)) $((1<<4)) $((5 > 3)) $(( (2+3)*4 )) $(( 7 / 2 )) $(( -7 / 2 ))'
    'x=3; echo $((x*x)) $((x += 2)) $x $((x ? 10 : 20)) $((!x)) $((x == 5 && 1))'
    'echo ${u:-def} ${HOME:+set} ${u-unset} "${u:-}" ${u:=now} $u'
    'x=abcdef; echo ${x#ab} ${x##*d} ${x%ef} ${x%%c*} ${#x} ${#}'
    'echo $(echo sub) `echo bq` "$(echo a; echo b)" "$(printf "x\n\n\n")"'
    'echo *.txt; echo "*.txt"; echo *.none; echo [ab]*.txt; echo ?lpha.txt'
    'set -f; echo *.txt'
    'echo a b c | wc -w; printf "%s\n" one two | tail -1'
    'echo "a  b" > out.txt; cat < out.txt; echo more >> out.txt; wc -l < out.txt'
    'echo to-stderr >&2 2> /dev/null; echo visible 2>&1; echo hidden > /dev/null; echo end'
    'cat <<EOF
heredoc $((2+2)) $(echo sub)
EOF
cat <<"EOF"
literal $((2+2))
EOF
cat <<-EOF
	indented
	EOF'
    'for i in a b c; do echo "[$i]"; done; for w in; do echo never; done'
    'x="a b"; for w in $x; do echo "<$w>"; done; IFS=:; for p in a:b:c; do echo "<$p>"; done'
    'if true; then echo yes; else echo no; fi; if false; then echo a; elif true; then echo b; else echo c; fi'
    'i=0; while [ $i -lt 3 ]; do i=$((i+1)); done; echo $i; until [ -n "$d" ]; do d=yes; done; echo $d'
    'for i in 1 2 3 4; do if [ $i = 2 ]; then continue; fi; if [ $i = 4 ]; then break; fi; echo $i; done'
    'case foo in f*) echo yes;; *) echo no;; esac; case x in a|x) echo alt;; esac; case "" in "") echo empty;; esac'
    'f() { echo "args=$# first=$1"; return 3; }; f one two; echo "status=$?"'
    'g() { echo "$@"; }; g "a b" c; h() ( echo sub; ); h'
    'set -- p q r; echo "$@" "$*" $#; shift; echo $1; set -- a b; printf "[%s]" "$@"; echo; set --; printf "[%s]" "$@"; echo'
    'printf "[%s]" "" "$u" '"''"'; echo; printf "%d-%s-%5s|%-3s|%%\n" 42 str ab cd'
    'echo -n no-newline; echo; echo a\tb; echo "a\tb"'
    'cd /; pwd; cd /tmp && pwd; (cd /usr; pwd); pwd'
    'if [ 1 -lt 2 ] && test -d / && [ -f two.txt ] && [ ! -e none ] && [ -n x ] && [ -z "" ] && [ a = a ] && [ a != b ]; then echo tests; fi'
    'test "a" = "a" -a 1 -eq 1 && echo both; [ 1 -eq 2 -o 2 -eq 2 ] && echo either'
    'nonexistent_cmd 2>/dev/null; echo "status=$?"; /bin/false; echo $?; true; echo $?; (exit 5); echo $?; x=$(false); echo $?'
    'false || echo fallback; true && echo and; ! false && echo negated; false && echo no || echo yes'
    'echo a; { echo b; echo c; } | wc -l; ( echo d; echo e ) | wc -l'
    'set -e; false; echo not-reached'
    'set -e; false || true; echo reached; if false; then :; fi; echo still'
    'exec echo exec-replaced; echo not-here'
    'eval "echo ev\$((1+1))"; x="echo nested"; eval $x'
    'export FOO=bar; env | grep ^FOO; unset FOO; echo "[$FOO]"'
    'readonly R=1; (R=2) 2>/dev/null; echo $R'
    'type echo; type nonexistent_cmd 2>/dev/null; echo $?; command -v echo; command echo direct'
    'read a b <<EOF
one two three
EOF
echo "a=$a b=$b"; echo "x  y" | { read l; echo "<$l>"; }'
    'echo abc | tr a-z A-Z; echo one two | { read a b; echo $b $a; }'
    'sleep 0.1 & wait; echo waited; sleep 0.1 & wait $!; echo $?'
    'umask 022; umask; umask 077; umask'
    'trap "echo bye" EXIT; echo main'
    'echo done; exit 7'
    'x=$(printf "%s" "$(seq 1 3000 | tr -d "\n")"); echo ${#x}'
    'echo one two three four five six seven eight nine ten eleven twelve thirteen fourteen fifteen'
    'echo ~ | grep -c /; echo "~"; echo ~/x | grep -c home'
    'echo $(( 2 ** 3 )) 2>/dev/null || echo no-power'
    'a=1; b=$a; a=2; echo $b $a'
    'echo "line1
line2"; echo "tab	here"'
    'echo a\
b'
    'echo $((1 + $(echo 2) + ${x:-3}))'
    ': ; echo colon; true; echo $?'
    'echo "$(echo "nested \"quotes\"")"'
    'f() { return; }; f; echo $?; g() { false; }; g; echo $?'
    'while read line; do echo "got $line"; done < two.txt'
    'printf "%s\n" a b c | while read l; do echo "<$l>"; done; echo after'
    'if echo piped | grep -q piped; then echo found; fi'
    'x=; echo "[${x:-empty}]" "[${x-set}]"'
    'echo ${#*} $# "${*:-none}"'
)
index=0
for snippet in "${snippets[@]}"; do
    index=$((index + 1))
    set +e
    dash -c "$snippet" > "$work/dash.out" 2> /dev/null
    dash_status=$?
    timeout 10 "$sh" -c "$snippet" > "$work/mlx.out" 2> "$work/mlx.err"
    mlx_status=$?
    set -e
    if [[ $dash_status -ne $mlx_status ]] || ! cmp -s "$work/dash.out" "$work/mlx.out"; then
        {
            echo "snippet $index: $snippet"
            echo "--- dash (exit $dash_status)"; cat "$work/dash.out"
            echo "--- mlx-sh (exit $mlx_status)"; cat "$work/mlx.out"; cat "$work/mlx.err"
        } > "$work/diff.txt"
        fail "mlx-sh and dash disagree on snippet $index" "$work/diff.txt"
    fi
done
echo "ok   $index -c snippets agree with dash (output and status)"

# --- What dash lacks.
expect() {
    local snippet=$1 expected=$2 status=${3:-0}
    set +e
    timeout 10 "$sh" -c "$snippet" > "$work/mlx.out" 2> "$work/mlx.err"
    local got=$?
    set -e
    if [[ $got -ne $status ]] || [[ "$(cat "$work/mlx.out")" != "$expected" ]]; then
        { echo "snippet: $snippet"; echo "expected (exit $status): $expected"; echo "got (exit $got):"; cat "$work/mlx.out" "$work/mlx.err"; } > "$work/diff.txt"
        fail "mlx-sh: unexpected result" "$work/diff.txt"
    fi
}
expect 'x=abcdef; echo ${x:2:2} ${x:3} ${x:0:1}' 'cd def a'
expect 'f() { local v=in; echo $v; }; v=out; f; echo $v' $'in\nout'
expect 'local x 2>&1' 'mlx-sh: local: not in a function' 1
expect 'echo $((1+2' '' 2
expect 'echo ${x!y}; echo not-reached' '' 2
expect 'help | tr " " "\n" | grep -c "^cd$"' '1'
expect 'echo ${x:?unset}; echo not-reached' '' 2
expect 'echo ${x:?unset} || echo not-reached' '' 2
expect '(echo ${x:?unset}) 2>&1; echo "reached $?"' $'mlx-sh: x: unset\nreached 2'
echo "ok   substrings, local, help, \${x:?} ends a script as expected"

# --- Scripts.
cat > "$work/script.sh" <<'SCRIPT'
#!/bin/sh
count=0
for name in "$@"; do
    count=$((count + 1))
    case $name in
        -*) echo "option $name" ;;
        *.txt) echo "text file $name" ;;
        *) echo "other $name" ;;
    esac
done
echo "count=$count"
greet() {
    local who=$1
    echo "hello, ${who:-world}"
}
greet
greet mlx
if [ "$count" -gt 2 ]; then echo many; elif [ "$count" -eq 2 ]; then echo two; else echo few; fi
total=0
i=1
while [ $i -le 10 ]; do
    total=$((total + i))
    i=$((i + 1))
done
echo "total=$total"
echo "lines: $(printf 'a\nb\nc\n' | wc -l)"
x="spaced   out"
printf '[%s]\n' $x
printf '[%s]\n' "$x"
echo "$#: $1 $2"
shift 1
echo "$#: $1"
exit 3
SCRIPT
set +e
dash "$work/script.sh" -v notes.txt other > "$work/dash.out" 2>&1
dash_status=$?
timeout 10 "$sh" "$work/script.sh" -v notes.txt other > "$work/mlx.out" 2>&1
mlx_status=$?
set -e
[[ $dash_status -eq 3 && $mlx_status -eq 3 ]] || fail "the script's exit status: dash $dash_status, mlx-sh $mlx_status" "$work/mlx.out"
cmp -s "$work/dash.out" "$work/mlx.out" || { diff "$work/dash.out" "$work/mlx.out" > "$work/diff.txt" || true; fail "the script's output differs from dash" "$work/diff.txt"; }
echo "ok   a script by path: as dash (exit 3)"

sed -i "1s|.*|#!$sh|" "$work/script.sh"
chmod +x "$work/script.sh"
set +e
"$work/script.sh" a > "$work/mlx.out" 2>&1
set -e
head -1 "$work/mlx.out" | grep -q '^other a$' || fail "the script by shebang" "$work/mlx.out"
printf 'echo from stdin\necho "$1"\n' | timeout 10 "$sh" -s argone > "$work/mlx.out" 2>&1
[[ "$(cat "$work/mlx.out")" == $'from stdin\nargone' ]] || fail "a script from stdin with -s" "$work/mlx.out"
printf 'echo plain stdin\n' | timeout 10 "$sh" > "$work/mlx.out" 2>&1
[[ "$(cat "$work/mlx.out")" == 'plain stdin' ]] || fail "a script from stdin" "$work/mlx.out"
echo "ok   a script by shebang, from stdin, with -s"

# --- --parse.
"$sh" --parse 'if [ -f x ]; then cat x | wc -l > n; fi; f() { echo "$@"; }' > "$work/parse.out" 2>&1
for expected in '^list$' '^  andor$' '^      if$' 'word \[\[\]' 'word \[wc\]' 'redirect' '^      function \[f\]'; do
    grep -q -- "$expected" "$work/parse.out" || fail "--parse: no $expected in the dump" "$work/parse.out"
done
set +e
"$sh" --parse 'if true; then' > "$work/parse.out" 2>&1
incomplete_status=$?
"$sh" --parse 'fi' > "$work/parse2.out" 2>&1
syntax_status=$?
set -e
grep -qi 'incomplete' "$work/parse.out" || fail "--parse of an unfinished if does not say incomplete" "$work/parse.out"
grep -qi 'syntax error' "$work/parse2.out" || fail "--parse of a stray fi does not say syntax error" "$work/parse2.out"
[[ $incomplete_status -ne 0 && $syntax_status -ne 0 ]] || fail "--parse exits 0 on bad input"
echo "ok   --parse: the tree, incomplete input, a syntax error"

# --- The line editor on a pty.
command -v python3 > /dev/null || { echo "skip the line editor (no python3)"; echo PASS; exit 0; }
rm -f "$HOME/.mlx_sh_history"
cat > "$work/drive.py" <<'PY'
import os, pty, sys, time, select
shell = sys.argv[1]
pid, fd = pty.fork()
if pid == 0:
    os.environ["TERM"] = "xterm-256color"
    os.environ["PS1"] = "P> "
    os.execv(shell, [shell, "-i"])
def read_for(seconds):
    out = b""
    end = time.time() + seconds
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.05)
        if r:
            try:
                data = os.read(fd, 4096)
            except OSError:
                break
            if not data:
                break
            out += data
    return out
def send(data, wait=0.4):
    os.write(fd, data)
    return read_for(wait)
log = read_for(0.8)
log += send(b"echo hello wrld")
log += send(b"\x1b[D\x1b[D\x1b[D")      # left, before the r
log += send(b"o")
log += send(b"\r", 0.6)
log += send(b"\x1b[A")                   # the line again from the history
log += send(b"\x01")                     # Ctrl-A
log += send(b"\x0b")                     # Ctrl-K
log += send(b"echo second\r", 0.6)
log += send(b"ech\t")                    # completes the command
log += send(b"/us\t")                    # completes the path
log += send(b"\r", 0.6)
log += send(b"echo cancelled")
log += send(b"\x03", 0.6)                # Ctrl-C drops the line
log += send(b"echo after\r", 0.6)
log += send(b"\x04", 1.0)                # Ctrl-D leaves
time.sleep(0.3)
try:
    _, status = os.waitpid(pid, os.WNOHANG)
except ChildProcessError:
    status = -1
sys.stdout.buffer.write(log)
sys.stdout.write("\n--- status %d\n" % status)
PY
timeout 60 python3 -I "$work/drive.py" "$sh" > "$work/pty.log" 2>&1 || fail "the pty driver failed" "$work/pty.log"
plain=$(sed 's/\x1b\[[0-9;]*[A-Za-z]//g; s/\r//g' "$work/pty.log")
for expected in 'hello world' 'second' '^/usr/' 'after' '--- status 0'; do
    grep -q -- "$expected" <<< "$plain" || fail "the line editor: no '$expected' in the session" "$work/pty.log"
done
grep -q 'cancelled' <<< "$plain" && { grep -q '^cancelled$' <<< "$plain" && fail "Ctrl-C ran the cancelled line" "$work/pty.log"; }
[[ "$(cat "$HOME/.mlx_sh_history")" == $'echo hello world\necho second\necho /usr/\necho after' ]] || fail "the history file" "$HOME/.mlx_sh_history"
echo "ok   the line editor: cursor keys, Ctrl-A/K, history, completion, Ctrl-C, Ctrl-D, the history file"

echo PASS
