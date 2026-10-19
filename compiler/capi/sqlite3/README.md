# mlx-sqlite3

SQLite for the C and C++ programs built here, in Mlx, with SQLite's C API
(3.45): WebKit's databases (WebCore's SQLiteDatabase, SQLiteStatement and
SQLiteTransaction, and the stores over them: IndexedDB, Web SQL's
tracker, push subscriptions, service worker registrations, LocalStorage,
resource load statistics, private click measurement, the icon database)
include `<sqlite3.h>` as they would SQLite's header and link
`libmlxsqlite3.so` in place of `libsqlite3.so.0`. The database engine is
the standard library's `std.sql` (std/src/sql/): SQLite's file format,
its SQL, its planner's choices where they show (row orders, which index a
scan uses) and its errors. This directory is the C API over it.

The interface is written once, in Mlx: `api/sqlite3.mlx` declares the
types, constants, functions and macros C sees (`sqlite3_prepare_v2`,
`SQLITE_ROW`, `sqlite3_destructor_type`, `SQLITE_TRANSIENT`...).
`mlx-cheaders` (compiler/cc) makes `mlx-out/include/mlx-sqlite3/sqlite3.h`
of it; there is no header written by hand.

    ./mlx build sqlite3          the library and its header
    tools/check_sqlite.sh        builds it, runs the tests (and them against the system's SQLite)
    tests/372_sql_runtime.mlx    std.sql itself (tests/run_std_modules.sh)

## The parts

| File | What it has |
|------|-------------|
| `sqlite3.mlx` | The library's root (`--plugin --stack-arena`), importing the parts |
| `common.mlx` | The C library's memory for std.sql, C strings, UTF-16, C's variable arguments, result codes as a connection reports them |
| `main.mlx` | Initialization and `sqlite3_config`, open and close, errors, `exec`, `get_table`, busy handlers, the authorizer, hooks, memory, limits, status, `table_column_metadata`, the small utilities |
| `statements.mlx` | `prepare` (UTF-8 and UTF-16, legacy and v2/v3, with tails), `step`, `reset`, `finalize`, the binds, the column accessors, `sql`, `expanded_sql`, `stmt_readonly`, `stmt_status` |
| `values.mlx` | `sqlite3_value_*`, the results and context of application functions, `create_function` (scalar and aggregate), `create_collation` |
| `blob.mlx` | Incremental blob I/O |
| `printf.mlx` | SQLite's printf (`%q`, `%Q`, `%w`, `%z` with C's) |

## What std.sql does

- **Files**: SQLite's format 4 (any page size, the header's counters and
  cookies, overflow chains, the freelist), read and written so that
  SQLite reads ours and we read SQLite's; the rollback journal
  (DELETE, TRUNCATE, PERSIST, MEMORY, OFF) with its checksums, hot
  journals played back (SQLite's too); SQLite's file locks (fcntl on the
  lock bytes, the per-file lock table shared by a process's connections),
  so processes using either library take turns; a recursive mutex per
  connection. `:memory:` databases.
- **SQL**: SELECT (joins, LEFT JOIN, subqueries in every place, compound
  SELECTs, GROUP BY, HAVING, DISTINCT, ORDER BY with collations and NULLS
  FIRST/LAST, LIMIT/OFFSET, WITH's common table expressions,
  table-valued pragma functions), INSERT
  (VALUES, SELECT, DEFAULT VALUES, OR conflict resolutions, upserts:
  ON CONFLICT DO NOTHING and DO UPDATE), REPLACE,
  UPDATE, DELETE; NOT NULL, UNIQUE, PRIMARY KEY, CHECK and FOREIGN KEY
  constraints (CASCADE, SET NULL, SET DEFAULT, RESTRICT) with their
  conflict clauses; INTEGER PRIMARY KEY and AUTOINCREMENT; partial
  indexes; CREATE/DROP TABLE, INDEX and VIEW, ALTER TABLE RENAME, RENAME
  COLUMN and ADD COLUMN; transactions and savepoints; VACUUM; the PRAGMAs
  WebKit uses (and table_info, index_list, index_info, foreign_key_list,
  integrity_check...). Values, affinities, comparisons, collations
  (BINARY, NOCASE, RTRIM, an application's), NULL logic and the text of
  reals are SQLite's. The built-in functions are SQLite's core ones, the
  date and time functions, printf/format, and the aggregates.
- **Planning**: rowid and index lookups (equalities, ranges, IN), the
  covering index SQLite's costs pick for a scan, ORDER BY taken from an
  index's order, nested loops in FROM order.
- **The C API**: every function of `api/sqlite3.mlx` (191, all that
  WebKit calls among them); the authorizer is asked as SQLite asks it
  (SELECT, READ, FUNCTION, the DDL's statements over sqlite_master).

What it does not do (yet): triggers (refused; a file's are passed
over), recursive WITH tables, window functions, RETURNING, WITHOUT
ROWID tables, ATTACH, virtual tables (FTS, R-tree), the JSON and math functions, EXPLAIN. A write-ahead
log: `PRAGMA journal_mode=WAL` answers `wal` but the file keeps a
rollback journal, and checkpoints have nothing to do. `auto_vacuum` is
kept in the header only (no pointer map pages). `integrity_check` counts
each index's entries against its table rather than walking every page.
The commit, rollback and update hooks are kept but not called. It is
slower than SQLite: inserting 20000 indexed rows takes about a second
where SQLite takes 40 ms (pages are rewritten from their cells when they
fill or empty).

## Tests

`tools/check_sqlite.sh` builds the library and its header, checks that
every function the header declares is in the library and that the header
compiles as C and as C++, and runs the C programs of `tests/` built by
mlx-cc against the library: each must print what `NAME.expected` holds,
which is what the system's SQLite 3.45 prints for the same program (with
gcc and the system's library at hand, the script checks that it still
does). `files.c` also crosses over: a database written by either library
is read by the other, and a transaction either leaves half done (its
process gone, its journal hot) is rolled back by the other.

| Test | What it covers |
|------|----------------|
| `sql.c` | Expressions, NULLs, types, affinity, collations, LIKE and GLOB, CASE, CAST, the functions, aggregates, GROUP BY, ORDER BY, joins, subqueries, compounds, views, WITH, errors |
| `api.c` | Opening, prepare and tails, binds of every type, column accessors and conversions, names and declared types, reset, expanded SQL, changes, error codes and messages, exec, get_table, printf, UTF-16 |
| `writes.c` | INSERT, UPDATE, DELETE, conflict resolutions, constraints, partial indexes, AUTOINCREMENT, foreign keys, transactions and savepoints, DDL and its errors |
| `webkit.c` | Every statement of WebKit's databases (`webkit_corpus.h`, 409 statements from WebKit 2.54.1), a database per source file, run with bound values |
| `extend.c` | Application functions and aggregates, collations, the authorizer, blob I/O, two connections and busy handlers |
| `files.c` | The file format and the journal, across the two libraries |
| `threads.c` | Connections on four threads writing and two reading; threads sharing a connection |
