#!/usr/bin/env bash
# mlx-cc on real C projects (docs/ROADMAP.md, phase 1 step 4):
#
#   - zlib 1.3.1: the library and its test program (test/example.c), run
#     on the system's C library and on mlxlibc alone;
#   - Lua 5.4.7: the interpreter, then the official Lua 5.4.7 test suite
#     (user mode, `_U=true`) file by file.
#
# The sources are downloaded from their projects (or taken from
# --sources DIR, which holds the three tarballs) and must match the
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

"$compiler" --quiet projects/cc/main.mlx -o "$work/mlx-cc" || fail "mlx-cc does not build"
cp -r projects/cc/include "$work/include"
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
"$cc" -o "$work/lua" "${lfiles[@]}" -lm 2> "$work/lua.cc" || fail "Lua does not compile: $(head -5 "$work/lua.cc")"
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
echo "check_cc_projects.sh: all checks passed"
