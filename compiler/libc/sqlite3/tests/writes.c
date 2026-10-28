/* mlx-sqlite3's writes: INSERT (VALUES, SELECT, DEFAULT VALUES, column
   lists, defaults), INSERT OR REPLACE / IGNORE / ABORT / FAIL /
   ROLLBACK, REPLACE, upserts (ON CONFLICT DO NOTHING / DO UPDATE),
   UPDATE and DELETE; NOT NULL, UNIQUE, PRIMARY KEY
   and CHECK constraints with their conflict clauses; partial indexes;
   INTEGER PRIMARY KEY and rowids, AUTOINCREMENT and sqlite_sequence;
   foreign keys (with and without PRAGMA foreign_keys, CASCADE, SET
   NULL); transactions, savepoints and rollbacks; CREATE / DROP TABLE
   and INDEX (IF [NOT] EXISTS, UNIQUE indexes over existing rows), ALTER
   TABLE (RENAME, ADD COLUMN, RENAME COLUMN) and their errors; and the
   schema in sqlite_master. It prints what SQLite 3.45 prints for the
   same program. */
#include "common.h"

static const char *inserts =
    "CREATE TABLE t(id INTEGER PRIMARY KEY, name TEXT NOT NULL DEFAULT 'none', n INT DEFAULT 7, u TEXT UNIQUE, created DEFAULT (1 + 2));"
    "INSERT INTO t(name) VALUES ('a');"
    "INSERT INTO t DEFAULT VALUES;"
    "INSERT INTO t(id, name, u) VALUES (10, 'b', 'x');"
    "INSERT INTO t(name, u) VALUES ('c', 'y'), ('d', NULL), ('e', NULL);"
    "INSERT INTO t(name, n) SELECT name || '2', n * 2 FROM t WHERE id < 3;"
    "SELECT * FROM t ORDER BY id;"
    "INSERT INTO t(name, u) VALUES ('dup', 'x');";

static const char *conflicts =
    "INSERT OR IGNORE INTO t(name, u) VALUES ('dup', 'x');"
    "SELECT changes();"
    "INSERT OR REPLACE INTO t(id, name, u) VALUES (10, 'replaced', 'z');"
    "REPLACE INTO t(name, u) VALUES ('took y', 'y');"
    "SELECT id, name, u FROM t ORDER BY id;"
    "INSERT INTO t(id, name) VALUES (1, 'again');";

static const char *more =
    "INSERT INTO t(name) VALUES (NULL);";

static const char *updates =
    "CREATE TABLE kv(k TEXT PRIMARY KEY, v INTEGER, hits INTEGER DEFAULT 0);"
    "INSERT INTO kv VALUES ('a', 1, 0), ('b', 2, 0);"
    "INSERT OR REPLACE INTO kv VALUES ('a', 10, 1);"
    "INSERT OR IGNORE INTO kv(k, v) VALUES ('b', 20);"
    "INSERT INTO kv(k, v) VALUES ('c', 30);"
    "SELECT * FROM kv ORDER BY k;"
    "UPDATE kv SET v = v + 1;"
    "SELECT changes(), total_changes() > 0;"
    "UPDATE kv SET v = NULL, hits = 5 WHERE k = 'b';"
    "UPDATE OR IGNORE kv SET k = 'a' WHERE k = 'c';"
    "SELECT * FROM kv ORDER BY k;"
    "DELETE FROM kv WHERE v IS NULL;"
    "SELECT changes(), count(*) FROM kv;"
    "UPDATE kv SET k = 'a' WHERE k = 'c';";

static const char *upserts =
    "CREATE TABLE up(k TEXT PRIMARY KEY, v INTEGER, hits INTEGER DEFAULT 0, u TEXT UNIQUE);"
    "INSERT INTO up(k, v, u) VALUES ('a', 1, 'ua'), ('b', 2, 'ub');"
    "INSERT INTO up(k, v) VALUES ('a', 10) ON CONFLICT(k) DO UPDATE SET v = excluded.v, hits = hits + 1;"
    "INSERT INTO up(k, v) VALUES ('b', 20) ON CONFLICT DO NOTHING;"
    "INSERT INTO up(k, v) VALUES ('c', 30) ON CONFLICT(k) DO UPDATE SET hits = hits + 1;"
    "INSERT INTO up(k, v) VALUES ('c', 31) ON CONFLICT(k) DO UPDATE SET hits = hits + 1 WHERE excluded.v > 100;"
    "INSERT INTO up(k, v) VALUES ('c', 131), ('d', 4), ('c', 132) ON CONFLICT(k) DO UPDATE SET hits = hits + 1, v = up.v + excluded.v WHERE excluded.v > 100;"
    "SELECT changes();"
    "INSERT INTO up(k, v, u) VALUES ('z', 0, 'ua') ON CONFLICT(u) DO UPDATE SET hits = 100 ON CONFLICT(k) DO NOTHING;"
    "INSERT INTO up(k, v, u) VALUES ('a', 0, 'new') ON CONFLICT(u) DO UPDATE SET hits = 200 ON CONFLICT(k) DO NOTHING;"
    "SELECT * FROM up ORDER BY k;"
    "CREATE TABLE ur(id INTEGER PRIMARY KEY, n);"
    "INSERT INTO ur VALUES (1, 'x');"
    "INSERT INTO ur VALUES (1, 'y') ON CONFLICT(id) DO UPDATE SET n = n || excluded.n;"
    "INSERT INTO ur VALUES (1, 'z') ON CONFLICT DO UPDATE SET id = 5;"
    "SELECT * FROM ur;"
    "INSERT INTO up VALUES ('a', 1, 1, NULL) ON CONFLICT(k) DO UPDATE SET k = 'b';";

static const char *upsert_errors[] = {
    "INSERT INTO up(k, v) VALUES ('a', 1) ON CONFLICT(v) DO NOTHING;",
    "INSERT INTO up(k, v) VALUES ('a', 1) ON CONFLICT(nosuch) DO NOTHING;",
    "INSERT INTO up(k, v) VALUES ('a', 1) ON CONFLICT DO NOTHING ON CONFLICT(k) DO NOTHING;",
    "INSERT INTO up(k, v) VALUES ('a', 1) ON CONFLICT(k) DO UPDATE SET nosuch = 1;",
    NULL,
};

static const char *rowids =
    "CREATE TABLE r(a, b);"
    "INSERT INTO r VALUES ('x', 1), ('y', 2);"
    "INSERT INTO r(rowid, a) VALUES (100, 'z');"
    "INSERT INTO r(a) VALUES ('after');"
    "SELECT rowid, oid, _rowid_, a FROM r ORDER BY 1;"
    "UPDATE r SET rowid = 50 WHERE a = 'z';"
    "DELETE FROM r WHERE rowid = 101;"
    "INSERT INTO r(a) VALUES ('next');"
    "SELECT rowid, a FROM r ORDER BY rowid;"
    "CREATE TABLE ai(id INTEGER PRIMARY KEY AUTOINCREMENT, v);"
    "INSERT INTO ai(v) VALUES (1), (2), (3);"
    "DELETE FROM ai WHERE id = 3;"
    "INSERT INTO ai(v) VALUES (4);"
    "SELECT * FROM ai;"
    "SELECT name, seq FROM sqlite_sequence;"
    "DELETE FROM ai;"
    "INSERT INTO ai(v) VALUES (5);"
    "SELECT * FROM ai;"
    "INSERT INTO ai(id, v) VALUES (1000, 6);"
    "SELECT name, seq FROM sqlite_sequence;"
    "CREATE TABLE ipk(id INTEGER PRIMARY KEY, v);"
    "INSERT INTO ipk VALUES ('12', 'text key');"
    "INSERT INTO ipk VALUES (12.0, 'real key');";

static const char *rowid_errors =
    "INSERT INTO ipk VALUES ('abc', 'bad');";

static const char *foreign =
    "CREATE TABLE parent(id INTEGER PRIMARY KEY, name TEXT UNIQUE);"
    "CREATE TABLE child(id INTEGER PRIMARY KEY, parent INTEGER REFERENCES parent(id) ON DELETE CASCADE, pname TEXT REFERENCES parent(name) ON UPDATE CASCADE ON DELETE SET NULL);"
    "INSERT INTO parent VALUES (1, 'one'), (2, 'two'), (3, 'three');"
    "INSERT INTO child VALUES (10, 1, 'one'), (11, 1, 'two'), (12, 2, 'two'), (13, NULL, NULL);"
    "INSERT INTO child VALUES (14, 99, NULL);"
    "PRAGMA foreign_keys;"
    "PRAGMA foreign_keys = ON;"
    "PRAGMA foreign_keys;"
    "DELETE FROM parent WHERE id = 1;"
    "SELECT * FROM child ORDER BY id;"
    "UPDATE parent SET name = 'TWO' WHERE id = 2;"
    "SELECT * FROM child ORDER BY id;"
    "DELETE FROM parent WHERE name = 'TWO';"
    "SELECT * FROM child ORDER BY id;"
    "SELECT * FROM parent ORDER BY id;"
    "CREATE TABLE strict_child(p INTEGER NOT NULL REFERENCES parent(id));"
    "INSERT INTO strict_child VALUES (3);"
    "INSERT INTO strict_child VALUES (4);";

static const char *foreign2 =
    "DELETE FROM parent WHERE id = 3;";

static const char *foreign3 =
    "PRAGMA foreign_key_list(child);"
    "PRAGMA foreign_keys = OFF;"
    "DELETE FROM parent WHERE id = 3;"
    "SELECT count(*) FROM strict_child;";

static const char *transactions =
    "CREATE TABLE x(v);"
    "BEGIN;"
    "INSERT INTO x VALUES (1);"
    "SAVEPOINT a;"
    "INSERT INTO x VALUES (2);"
    "SAVEPOINT b;"
    "INSERT INTO x VALUES (3);"
    "ROLLBACK TO a;"
    "INSERT INTO x VALUES (4);"
    "RELEASE a;"
    "SELECT * FROM x;"
    "ROLLBACK;"
    "SELECT count(*) FROM x;"
    "BEGIN IMMEDIATE;"
    "INSERT INTO x VALUES (5);"
    "COMMIT;"
    "SAVEPOINT outer1;"
    "INSERT INTO x VALUES (6);"
    "RELEASE outer1;"
    "SELECT * FROM x;"
    "BEGIN EXCLUSIVE TRANSACTION;"
    "CREATE TABLE gone(a);"
    "INSERT INTO gone VALUES (1);"
    "ROLLBACK TRANSACTION;"
    "SELECT name FROM sqlite_master WHERE name = 'gone';"
    "END;";

static const char *transaction_errors =
    "COMMIT;";

static const char *fail_modes =
    "CREATE TABLE m(a UNIQUE);"
    "INSERT INTO m VALUES (1), (2), (3);"
    "BEGIN;"
    "INSERT INTO m VALUES (4);"
    "INSERT OR FAIL INTO m SELECT a + 10 FROM m WHERE a < 3 UNION ALL SELECT 2;";

static const char *fail_modes2 =
    "SELECT * FROM m ORDER BY a;"
    "INSERT OR ABORT INTO m SELECT a + 20 FROM m WHERE a < 3 UNION ALL SELECT 2;";

static const char *fail_modes3 =
    "SELECT * FROM m ORDER BY a;"
    "INSERT OR ROLLBACK INTO m VALUES (2);";

static const char *fail_modes4 =
    "SELECT * FROM m ORDER BY a;"
    "UPDATE m SET a = 3 WHERE a = 1;";

static const char *ddl =
    "CREATE TABLE IF NOT EXISTS t(x);"
    "CREATE INDEX t_name ON t(name);"
    "CREATE UNIQUE INDEX IF NOT EXISTS t_n_name ON t(n, name);"
    "CREATE INDEX IF NOT EXISTS t_name ON t(u);"
    "SELECT type, name, tbl_name, sql FROM sqlite_master WHERE tbl_name = 't' ORDER BY name;"
    "PRAGMA index_list(t);"
    "PRAGMA index_info(t_n_name);"
    "DROP INDEX t_name;"
    "DROP INDEX IF EXISTS t_name;"
    "DROP TABLE IF EXISTS nosuch;"
    "ALTER TABLE kv RENAME TO kv2;"
    "ALTER TABLE kv2 ADD COLUMN extra TEXT DEFAULT 'e';"
    "ALTER TABLE kv2 RENAME COLUMN hits TO count;"
    "SELECT * FROM kv2 ORDER BY k;"
    "SELECT sql FROM sqlite_master WHERE name = 'kv2';"
    "PRAGMA table_info(kv2);"
    "CREATE TABLE dupes(a);"
    "INSERT INTO dupes VALUES (1), (1);"
    "CREATE UNIQUE INDEX dupes_a ON dupes(a);";

static const char *ddl_errors[] = {
    "CREATE TABLE t(y);",
    "CREATE INDEX t_n_name ON t(n);",
    "DROP TABLE nosuch;",
    "DROP INDEX nosuch;",
    "ALTER TABLE nosuch RENAME TO x;",
    "ALTER TABLE kv2 ADD COLUMN k TEXT;",
    "ALTER TABLE kv2 ADD COLUMN p INTEGER PRIMARY KEY;",
    "ALTER TABLE kv2 ADD COLUMN q NOT NULL;",
    "CREATE TABLE bad(a, a);",
    "CREATE TABLE bad2(a INTEGER PRIMARY KEY, b PRIMARY KEY);",
    "CREATE TABLE sqlite_x(a);",
    "INSERT INTO kv2(nosuch) VALUES (1);",
    "INSERT INTO kv2 VALUES (1);",
    "UPDATE kv2 SET nosuch = 1;",
    "DELETE FROM nosuch;",
    "INSERT INTO sqlite_master VALUES (1, 2, 3, 4, 5);",
    NULL,
};

static const char *checks =
    "CREATE TABLE ck(a, b  CONSTRAINT pos CHECK(b > 0) CHECK (b < 100), c CHECK(length(c) < 3), CONSTRAINT x UNIQUE(a), CHECK(a IS NOT NULL OR b IS NULL));"
    "INSERT INTO ck VALUES (1, 2, 'ab');"
    "INSERT OR IGNORE INTO ck VALUES (3, -5, 'x');"
    "UPDATE OR IGNORE ck SET b = 500;"
    "ALTER TABLE ck ADD COLUMN d  INT DEFAULT 3;"
    "SELECT sql FROM sqlite_master WHERE name = 'ck';"
    "SELECT * FROM ck;"
    "CREATE TABLE pi(a, b);"
    "INSERT INTO pi VALUES (1, 'x'), (1, NULL), (2, 'y');"
    "CREATE UNIQUE INDEX pi_a ON pi(a) WHERE b IS NOT NULL;"
    "INSERT INTO pi VALUES (2, NULL);"
    "INSERT INTO pi VALUES (3, 'z');"
    "PRAGMA integrity_check;"
    "CREATE TABLE e(a);"
    "ALTER TABLE e ADD COLUMN c NOT NULL;"
    "SELECT sql FROM sqlite_master WHERE name = 'e';"
    "CREATE TABLE sp(a, b ,  UNIQUE(a, b) );"
    "ALTER TABLE sp ADD COLUMN c  INT  DEFAULT 1;"
    "SELECT sql FROM sqlite_master WHERE name = 'sp';";

static const char *check_errors[] = {
    "INSERT INTO ck VALUES (2, -1, 3);",
    "INSERT INTO ck VALUES (2, 200, 3);",
    "INSERT INTO ck VALUES (2, 20, 'abc');",
    "INSERT INTO ck VALUES (NULL, 20, 'a');",
    "INSERT OR REPLACE INTO ck VALUES (1, -5, 'x');",
    "UPDATE ck SET b = -1;",
    "INSERT INTO pi VALUES (2, 'z');",
    "UPDATE pi SET b = 'q' WHERE b IS NULL AND a = 1;",
    "CREATE UNIQUE INDEX pi_b ON pi(a) WHERE b IS NULL OR b > 'x';",
    "INSERT INTO e VALUES (1, NULL);",
    "ALTER TABLE ck ADD COLUMN f NOT NULL;",
    NULL,
};

static const char *drops =
    "DROP TABLE kv2;"
    "DROP TABLE t;"
    "SELECT type, name FROM sqlite_master ORDER BY name;"
    "PRAGMA integrity_check;";

/* An INSERT finalized after its table was dropped and a wider one made
   in its place (WebKit's IndexedDB store finalizes its cached
   statements late): the statement's values are freed as they were
   made, not as many as the new table has. */
static void finalize_after_schema_change(sqlite3 *db)
{
    sqlite3_exec(db, "CREATE TABLE narrow (a);", 0, 0, 0);
    sqlite3_stmt *insert = 0;
    sqlite3_prepare_v2(db, "INSERT INTO narrow VALUES (?);", -1, &insert, 0);
    sqlite3_bind_text(insert, 1, "kept", -1, SQLITE_TRANSIENT);
    int stepped = sqlite3_step(insert);
    sqlite3_reset(insert);
    sqlite3_exec(db, "DROP TABLE narrow; CREATE TABLE wide (a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p);", 0, 0, 0);
    int finalized = sqlite3_finalize(insert);
    printf("finalized after the schema changed: step %d, finalize %d\n", stepped, finalized);
    sqlite3_exec(db, "DROP TABLE wide;", 0, 0, 0);
}

int main(void)
{
    sqlite3 *db = open_memory();
    run(db, inserts);
    run(db, conflicts);
    run(db, more);
    run(db, updates);
    run(db, upserts);
    for (int i = 0; upsert_errors[i]; i++)
        run(db, upsert_errors[i]);
    run(db, rowids);
    run(db, rowid_errors);
    run(db, foreign);
    run(db, foreign2);
    run_with(db, foreign3, 1);
    run(db, transactions);
    run(db, transaction_errors);
    run(db, fail_modes);
    run(db, fail_modes2);
    printf("autocommit %d\n", sqlite3_get_autocommit(db));
    run(db, fail_modes3);
    printf("autocommit %d\n", sqlite3_get_autocommit(db));
    run(db, fail_modes4);
    run(db, ddl);
    for (int i = 0; ddl_errors[i]; i++)
        run(db, ddl_errors[i]);
    run(db, checks);
    for (int i = 0; check_errors[i]; i++)
        run(db, check_errors[i]);
    run(db, drops);
    finalize_after_schema_change(db);
    sqlite3_close(db);
    return 0;
}
