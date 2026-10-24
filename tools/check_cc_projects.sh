#!/usr/bin/env bash
# mlx-cc on real C projects (docs/ROADMAP.md, phase 1 step 4):
#
#   - zlib 1.3.1: the library and its test program (test/example.c), run
#     on the system's C library and on mlxlibc alone;
#   - Lua 5.4.7: the interpreter as POSIX systems build it (LUA_USE_POSIX:
#     sigaction, popen, mkstemp; not dlopen, whose tests need shared
#     objects), then the official Lua 5.4.7 test suite (user mode,
#     `_U=true`) file by file;
#   - SQLite 3.46.1: the library (the amalgamation) and its shell, in the
#     single-threaded and in the default thread-safe configuration
#     (pthread.h, dlfcn.h); the shell runs compiler/cc/tests/sqlite.sql
#     and prints what gcc's build of it printed (sqlite.expected), on the
#     system's C library and on mlxlibc alone.
#
# The sources are downloaded from their projects (or taken from
# --sources DIR, which holds the four tarballs) and must match the
# SHA-256 sums below. Each is unpacked in a directory of its own and only
# compiled and run, never sourced.
#
#   tools/check_cc_projects.sh [--sources DIR] [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
sources=""
compiler=""
while [[ $# -gt 0 ]]; do
    case $1 in
    --sources) sources=$2; shift ;;
    -h|--help) sed -n '2,/^set -euo/p' "$0" | sed '$d; s/^# \{0,1\}//'; exit 0 ;;
    *) compiler=$1 ;;
    esac
    shift
done
compiler=${compiler:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
work=$(mktemp -d /tmp/mlx-cc-projects.XXXXXX)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; exit 1; }

fetch() {
    local file=$1 url=$2 sum=$3
    if [[ -n "$sources" && -f "$sources/$file" ]]; then
        cp "$sources/$file" "$work/$file"
    else
        curl -sSfL --max-time 120 -o "$work/$file" "$url" || fail "cannot download $url (give --sources DIR)"
    fi
    echo "$sum  $work/$file" | sha256sum -c --quiet - || fail "$file does not match its SHA-256"
    mkdir -p "$work/src-${file%%.*}"
    tar -xzf "$work/$file" -C "$work/src-${file%%.*}"
}
fetch zlib.tar.gz https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz 9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23
fetch lua.tar.gz https://www.lua.org/ftp/lua-5.4.7.tar.gz 9fbf5e28ef86c69858f6d3d34eccc32e911c1a28b4120ff3e84aaa70cfbf1e30
fetch luatests.tar.gz https://www.lua.org/tests/lua-5.4.7-tests.tar.gz 8a4898ffe4c7613c8009327a0722db7a41ef861d526c77c5b46114e59ebf811e
fetch sqlite.tar.gz https://www.sqlite.org/2024/sqlite-autoconf-3460100.tar.gz 67d3fe6d268e6eaddcae3727fce58fcc8e9c53869bdd07a0c61e38ddf2965071

"$compiler" --quiet compiler/cc/main.mlx -o "$work/mlx-cc" || fail "mlx-cc does not build"
"$compiler" --quiet compiler/ld/main.mlx -o "$work/mlx-ld" || fail "mlx-ld does not build"
cp -r compiler/cc/include "$work/include"
cc="$work/mlx-cc"
tools/build_mlxlibc.sh --compiler "$compiler" --out "$work/lib" > /dev/null
lib="$work/lib/libmlxc.so.1"

# zlib and its example.
zlib="$work/src-zlib/zlib-1.3.1"
zfiles=()
for name in adler32 compress crc32 deflate gzclose gzlib gzread gzwrite infback inffast inflate inftrees trees uncompr zutil; do
    zfiles+=("$zlib/$name.c")
done
"$cc" -o "$work/example" -I"$zlib" "${zfiles[@]}" "$zlib/test/example.c" 2> "$work/zlib.cc" || fail "zlib does not compile: $(grep -v warning "$work/zlib.cc" | head -5)"
mkdir -p "$work/zrun"
(cd "$work/zrun" && timeout 60 "$work/example" > "$work/zlib.out" 2>&1) || fail "zlib's example failed: $(tail -5 "$work/zlib.out")"
grep -q '^inflate with dictionary: hello, hello!$' "$work/zlib.out" || fail "zlib's example did not finish: $(tail -3 "$work/zlib.out")"
(cd "$work/zrun" && env -i timeout 60 "$lib" "$work/example" > "$work/zlib.mlxlibc" 2>&1) || fail "zlib's example on mlxlibc failed: $(tail -5 "$work/zlib.mlxlibc")"
cmp -s "$work/zlib.out" "$work/zlib.mlxlibc" || fail "zlib's example prints otherwise on mlxlibc"
echo "ok   zlib 1.3.1: $(wc -l < "$work/zlib.out") checks of test/example.c pass, on glibc and on mlxlibc"

# Lua and its test suite.
lua="$work/src-lua/lua-5.4.7/src"
lfiles=()
for source in "$lua"/*.c; do
    [[ $(basename "$source") == luac.c ]] || lfiles+=("$source")
done
"$cc" -o "$work/lua" -DLUA_USE_POSIX "${lfiles[@]}" -lm 2> "$work/lua.cc" || fail "Lua does not compile: $(head -5 "$work/lua.cc")"
[[ ! -s "$work/lua.cc" ]] || fail "Lua: the compiler said: $(head -5 "$work/lua.cc")"
answer=$("$work/lua" -e 'print(("%d %.3f %s"):format(6 * 7, math.pi, ("ok"):upper()))')
[[ "$answer" == "42 3.142 OK" ]] || fail "Lua answered: $answer"
suite=$(ls -d "$work"/src-luatests/*/)
passed=()
# files.lua depends on the machine (it fails with gcc's build here too).
for name in strings math sort nextvar pm utf8 tpack vararg closure calls constructs events literals locals goto bitwise coroutine errors attrib db gc; do
    (cd "$suite" && timeout 300 "$work/lua" -e "_U=true" "$name.lua" > "$work/lua-$name.log" 2>&1) || fail "Lua test $name.lua: $(tail -5 "$work/lua-$name.log")"
    passed+=("$name")
done
echo "ok   Lua 5.4.7: the interpreter, and ${#passed[@]} files of the official test suite pass"

# Lua again from objects: each file with -c, the library an archive of
# mlx-ar's, the interpreter linked against it with -L and -l.
"$compiler" --quiet compiler/cc/ar.mlx -o "$work/mlx-ar" || fail "mlx-ar does not build"
mkdir -p "$work/lua-objects"
lobjects=()
for source in "${lfiles[@]}"; do
    name=$(basename "$source" .c)
    (cd "$work/lua-objects" && "$cc" -c -DLUA_USE_POSIX "$source") 2>> "$work/lua-objects.cc" || fail "Lua -c $name: $(head -5 "$work/lua-objects.cc")"
    [[ $name == lua ]] || lobjects+=("$work/lua-objects/$name.o")
done
"$work/mlx-ar" rcs "$work/lua-objects/liblua.a" "${lobjects[@]}" || fail "mlx-ar: liblua.a"
"$cc" -o "$work/lua-linked" "$work/lua-objects/lua.o" -L"$work/lua-objects" -llua -lm 2>> "$work/lua-objects.cc" || fail "Lua does not link from objects: $(head -5 "$work/lua-objects.cc")"
answer=$("$work/lua-linked" -e 'print(("%d %.3f %s"):format(6 * 7, math.pi, ("ok"):upper()))')
[[ "$answer" == "42 3.142 OK" ]] || fail "Lua from objects answered: $answer"
for name in strings math closure; do
    (cd "$suite" && timeout 300 "$work/lua-linked" -e "_U=true" "$name.lua" > "$work/lua-linked-$name.log" 2>&1) || fail "Lua from objects, test $name.lua: $(tail -5 "$work/lua-linked-$name.log")"
done
echo "ok   Lua from ${#lobjects[@]} objects and an archive of mlx-ar's (-c, -L, -l): the interpreter and 3 test files pass"

# SQLite: the amalgamation and the shell, single-threaded and thread-safe.
sqlite=$(ls -d "$work"/src-sqlite/*/)
script=$repo_root/compiler/cc/tests/sqlite.sql
expected=$repo_root/compiler/cc/tests/sqlite.expected
"$cc" -o "$work/sqlite3" -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_DQS=0 "$sqlite/sqlite3.c" "$sqlite/shell.c" -lm 2> "$work/sqlite.cc" || fail "SQLite does not compile: $(head -5 "$work/sqlite.cc")"
[[ ! -s "$work/sqlite.cc" ]] || fail "SQLite: the compiler said: $(head -5 "$work/sqlite.cc")"
"$cc" -o "$work/sqlite3-threads" -DSQLITE_DQS=0 -DSQLITE_ENABLE_MATH_FUNCTIONS "$sqlite/sqlite3.c" "$sqlite/shell.c" -lm -lpthread -ldl 2> "$work/sqlite-threads.cc" || fail "SQLite (thread-safe) does not compile: $(head -5 "$work/sqlite-threads.cc")"
for build in sqlite3 sqlite3-threads; do
    status=0
    (cd "$work/zrun" && timeout 120 "$work/$build" -batch :memory: < "$script" > "$work/$build.out" 2>&1) || status=$?
    echo "exit $status" >> "$work/$build.out"
    cmp -s "$expected" "$work/$build.out" || fail "$build prints otherwise than gcc's build (expected < > got): $(diff "$expected" "$work/$build.out" | head -12)"
done
answer=$(echo 'select round(sqrt(2), 6), round(pow(2, 10)), ceil(2.1), floor(-2.1);' | "$work/sqlite3-threads" -batch :memory:)
[[ "$answer" == "1.414214|1024.0|3.0|-3.0" ]] || fail "SQLite's math functions answered: $answer"
status=0
# On mlxlibc, -lm is the libm.so.6 built next to libmlxc.so.1.
(cd "$work/zrun" && env -i LD_LIBRARY_PATH="$work/lib" timeout 120 "$lib" "$work/sqlite3" -batch :memory: < "$script" > "$work/sqlite3.mlxlibc" 2>&1) || status=$?
echo "exit $status" >> "$work/sqlite3.mlxlibc"
cmp -s "$expected" "$work/sqlite3.mlxlibc" || fail "SQLite on mlxlibc prints otherwise (expected < > got): $(diff "$expected" "$work/sqlite3.mlxlibc" | head -12)"
echo "ok   SQLite 3.46.1: the library and its shell, single-threaded and thread-safe, run $(grep -c ';' "$script") statements as gcc's build does, on glibc and on mlxlibc"
echo "check_cc_projects.sh: all checks passed"
