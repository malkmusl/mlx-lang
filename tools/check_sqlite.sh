#!/usr/bin/env bash
# mlx-sqlite3, SQLite's C API in Mlx (compiler/capi/sqlite3, over std.sql):
#
#   - `./mlx build sqlite3` builds the library (libmlxsqlite3.so) and its
#     header: mlx-cheaders makes mlx-out/include/mlx-sqlite3/sqlite3.h of
#     the API's Mlx module (compiler/capi/sqlite3/api);
#   - every function the header declares is in the library;
#   - the header compiles as C and as C++;
#   - C programs built by mlx-cc against the library print what
#     NAME.expected says (what the system's SQLite 3.45 prints for the
#     same program, with the exit status last):
#       tests/sql.c      expressions, NULLs, types and affinity, collations,
#                        LIKE and GLOB, the scalar, date and aggregate
#                        functions, GROUP BY, ORDER BY, joins, subqueries,
#                        compounds, and their errors
#       tests/api.c      open, prepare and its tails, bind, column, reset,
#                        expanded_sql, changes, errors and their codes,
#                        exec, get_table, the printf family, UTF-16
#       tests/writes.c   INSERT, UPDATE, DELETE with every conflict
#                        resolution, constraints (CHECK too), partial
#                        indexes, AUTOINCREMENT, foreign keys, savepoints,
#                        DDL and ALTER TABLE
#       tests/webkit.c   every SQL statement of WebKit's databases
#                        (webkit_corpus.h), prepared, bound and run
#       tests/extend.c   application functions and aggregates, collations,
#                        the authorizer, blob I/O, busy handlers
#       tests/files.c    a database written, read, and a transaction left
#                        half done and rolled back from its hot journal
#       tests/threads.c  connections on threads; one shared by threads
#   - with gcc and the system's libsqlite3 installed, the same programs
#     built against libsqlite3 print the same (the expected files are still
#     SQLite's answers), and the files each library writes read the same
#     with the other (tests/files-read.expected), a hot journal either
#     leaves rolled back by the other.
#
#   tools/check_sqlite.sh
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
work=$(mktemp -d /tmp/mlx-sqlite3.XXXXXX)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; exit 1; }
headers=mlx-out/include/mlx-sqlite3
library=mlx-out/lib/libmlxsqlite3.so
tests=compiler/capi/sqlite3/tests

./mlx build -j 1 cc sqlite3 > "$work/build.log" 2>&1 || { cat "$work/build.log"; fail "./mlx build sqlite3"; }
echo "ok   ./mlx build sqlite3"

# Every `pub extern("c") fn` of the API module is defined by the library.
grep -ho 'pub extern("c") fn [A-Za-z0-9_]*' compiler/capi/sqlite3/api/*.mlx | awk '{print $4}' | sort -u > "$work/declared"
nm -D --defined-only "$library" | awk '{print $NF}' | sort -u > "$work/defined"
comm -23 "$work/declared" "$work/defined" > "$work/missing"
[ ! -s "$work/missing" ] || { cat "$work/missing"; fail "functions the header declares are not in the library"; }
echo "ok   the library defines the $(wc -l < "$work/declared") functions the header declares"

printf '#include <sqlite3.h>\nint main(void) { return sqlite3_libversion_number() == SQLITE_VERSION_NUMBER ? 0 : 1; }\n' > "$work/all.c"
mlx-out/bin/mlx-cc -I"$headers" -c "$work/all.c" -o "$work/all.o" || fail "the header does not compile as C"
mlx-out/bin/mlx-cc -I"$headers" -x c++ -std=c++20 -c "$work/all.c" -o "$work/all-cpp.o" || fail "the header does not compile as C++"
echo "ok   the header in C and C++"

system=0
if command -v gcc > /dev/null && printf '#include <sqlite3.h>\nint main(void){return 0;}\n' | gcc -x c - -o "$work/probe" -lsqlite3 2> /dev/null; then
    system=1
fi

# Runs a test program in a directory of its own: its output, and its exit
# status last.
run_test() {
    local program=$1 output=$2 status=0
    rm -rf "$work/run" && mkdir "$work/run"
    (cd "$work/run" && LD_LIBRARY_PATH="$repo_root/mlx-out/lib" timeout 300 "$program" > "$output" 2> "$output.err") || status=$?
    echo "exit $status" >> "$output"
}

for test in sql api writes webkit extend files threads; do
    mlx-out/bin/mlx-cc -I"$headers" "$tests/$test.c" -o "$work/$test" "$library" -lpthread || fail "tests/$test.c does not build"
    run_test "$work/$test" "$work/$test.out"
    if ! diff -u "$tests/$test.expected" "$work/$test.out" > "$work/$test.diff"; then
        head -40 "$work/$test.diff"
        fail "tests/$test.c does not print what $test.expected says"
    fi
    if [ "$system" = 1 ]; then
        gcc -o "$work/$test.system" "$tests/$test.c" -lsqlite3 -lpthread
        run_test "$work/$test.system" "$work/$test.system.out"
        diff -u "$tests/$test.expected" "$work/$test.system.out" > "$work/$test.system.diff" || { head -40 "$work/$test.system.diff"; fail "the system's SQLite no longer prints $test.expected"; }
        echo "ok   tests/$test.c (and the system's SQLite prints the same)"
    else
        echo "ok   tests/$test.c"
    fi
done

if [ "$system" = 1 ]; then
    # The files of each library read by the other; a transaction each
    # leaves half done rolled back by the other.
    ours() { LD_LIBRARY_PATH="$repo_root/mlx-out/lib" timeout 300 "$work/files" "$@"; }
    theirs() { "$work/files.system" "$@"; }
    mkdir -p "$work/files-run"
    cd "$work/files-run"
    theirs write sqlite.db > /dev/null
    ours write mlx.db > /dev/null
    for pair in "theirs sqlite.db" "ours sqlite.db" "theirs mlx.db" "ours mlx.db"; do
        set -- $pair
        $1 read "$2" > read.out
        diff -u "$repo_root/$tests/files-read.expected" read.out > read.diff || { head -40 read.diff; fail "$2 read by $1 is not what files-read.expected says"; }
    done
    echo "ok   files SQLite writes read by mlx-sqlite3, and mlx-sqlite3's by SQLite"
    theirs crash sqlite.db > /dev/null
    [ -e sqlite.db-journal ] || fail "SQLite's crash left no hot journal"
    ours read sqlite.db > read.out
    diff -u "$repo_root/$tests/files-read.expected" read.out > read.diff || { head -40 read.diff; fail "mlx-sqlite3 did not roll back SQLite's hot journal"; }
    [ ! -e sqlite.db-journal ] || fail "mlx-sqlite3 left SQLite's hot journal"
    ours crash mlx.db > /dev/null
    theirs read mlx.db > read.out
    diff -u "$repo_root/$tests/files-read.expected" read.out > read.diff || { head -40 read.diff; fail "SQLite did not find mlx-sqlite3's file as it was after a crash"; }
    echo "ok   a transaction left half done by either is rolled back by the other"
    cd "$repo_root"
fi
echo "check_sqlite.sh: all checks passed"
