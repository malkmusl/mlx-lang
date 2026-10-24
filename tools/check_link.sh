#!/usr/bin/env bash
# The compiler's object files, archives and linker
# (compiler/selfhost/object/relocatable.mlx, archive.mlx, link.mlx):
#
#   - relocatable_check: an object written and read back keeps its
#     sections, symbols and relocations; an archive its members (GNU ar
#     lists it and finds the members' bytes unchanged);
#   - link_check: two objects an ordinary C compiler made (gcc -c -fPIE),
#     one through an archive mlx-ar made, link into a program on libc
#     that runs as gcc's own link of them does.
#
#   tools/check_link.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
work=$(mktemp -d /tmp/mlx-link.XXXXXX)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; exit 1; }

"$compiler" --quiet compiler/selfhost/object/relocatable_check.mlx -o "$work/relocatable_check" || fail "relocatable_check does not build"
"$compiler" --quiet compiler/selfhost/object/link_check.mlx -o "$work/link_check" || fail "link_check does not build"
"$compiler" --quiet compiler/cc/ar.mlx -o "$work/mlx-ar" || fail "mlx-ar does not build"
echo "ok   relocatable_check, link_check and mlx-ar build"

(cd "$work" && ./relocatable_check > roundtrip.out) || fail "relocatable_check exited $?: $(tail -3 "$work/roundtrip.out")"
grep -q "^sections 4 symbols 8 relocations 3$" "$work/roundtrip.out" || fail "the object read back differs: $(head -3 "$work/roundtrip.out")"
grep -q "^symbol puts bind 1 kind 0 section 18446744073709551615" "$work/roundtrip.out" || fail "the undefined symbol was lost"
grep -q "^reloc section 0 offset 1 kind 4 symbol 7 addend -4$" "$work/roundtrip.out" || fail "the call relocation was lost"
grep -q "^member a_very_long_member_name.o 1200 bytes object true$" "$work/roundtrip.out" || fail "the long member name was lost"
if command -v ar > /dev/null; then
    (cd "$work" && ar t t.a) | grep -q "^a_very_long_member_name.o$" || fail "GNU ar does not list the archive's long name"
    (cd "$work" && ar p t.a first.o | cmp -s - t.o) || fail "GNU ar reads other bytes from the archive than were written"
fi
echo "ok   an object and an archive written are read back unchanged (GNU ar reads the archive too)"

if command -v gcc > /dev/null; then
    cat > "$work/hello.c" <<'EOF_C'
#include <stdio.h>
#include <string.h>
int counter = 5;
static int table[4] = { 1, 2, 3, 4 };
int helper(int x);
const char *greeting(void);
int (*pointer)(int) = helper;
int main(int argc, char **argv) {
    printf("%s: %d %d %d %d\n", greeting(), helper(counter), pointer(2), table[3], argc);
    printf("strlen %zu\n", strlen(greeting()));
    return 7;
}
EOF_C
    cat > "$work/helper.c" <<'EOF_C'
#include <stdio.h>
extern int counter;
static char buffer[64];
const char *greeting(void) { snprintf(buffer, sizeof buffer, "hello(%d)", counter); return buffer; }
int helper(int x) { counter += x; return counter * 10; }
EOF_C
    (cd "$work" && gcc -c -fPIE -O1 hello.c helper.c) || fail "gcc does not compile the test"
    (cd "$work" && ./mlx-ar rcs libhelper.a helper.o) || fail "mlx-ar does not make the archive"
    (cd "$work" && ./link_check prog hello.o libhelper.a) || fail "link_check does not link gcc's objects"
    (cd "$work" && gcc -o reference hello.o helper.o) || fail "gcc does not link the reference"
    status=0
    (cd "$work" && ./prog a b > prog.out 2>&1) || status=$?
    reference_status=0
    (cd "$work" && ./reference a b > reference.out 2>&1) || reference_status=$?
    [[ $status -eq 7 && $reference_status -eq 7 ]] || fail "the linked program exited $status (gcc's $reference_status)"
    cmp -s "$work/prog.out" "$work/reference.out" || fail "the linked program prints otherwise than gcc's: $(diff "$work/prog.out" "$work/reference.out" | head -4)"
    echo "ok   two gcc -fPIE objects, one from an archive, link into a program that runs as gcc's link of them"
else
    echo "skip gcc objects: no gcc"
fi
echo "check_link.sh: all checks passed"
