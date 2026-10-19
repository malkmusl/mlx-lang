/* mlx-sqlite3's files are SQLite's: `files write PATH` makes a database
   (tables of every type of value, texts and blobs long enough for
   overflow pages, thousands of rows in B-trees of several levels,
   indexes, rows deleted and pages freed, a page size of 1024, the user
   version, AUTOINCREMENT) and `files read PATH` prints what it holds,
   integrity check first. `files crash PATH` leaves a transaction half
   written (its process ends in the middle, its rollback journal hot) and
   `files read` after it must see the database as it was before. With
   no arguments it does all of these in one process. tools/check_sqlite.sh
   writes with one library and reads with the other, both ways: what is
   read is the same as what SQLite reads from its own file. */
#include "common.h"
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static unsigned int seed;

static unsigned int next_random(void)
{
    seed = seed * 1103515245u + 12345u;
    return (seed >> 8) & 0xffffff;
}

static void fill(unsigned char *data, int length, unsigned int start)
{
    seed = start;
    for (int i = 0; i < length; i++)
        data[i] = (unsigned char)next_random();
}

static int check(sqlite3 *db, int rc, const char *what)
{
    if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW)
        printf("%s: %d %s\n", what, rc, sqlite3_errmsg(db));
    return rc;
}

static void write_database(const char *path)
{
    sqlite3 *db = NULL;
    remove(path);
    check(db, sqlite3_open(path, &db), "open");
    run(db, "PRAGMA page_size = 1024; PRAGMA user_version = 7;");
    run(db, "CREATE TABLE kinds(id INTEGER PRIMARY KEY, i INTEGER, r REAL, t TEXT, b BLOB, n);");
    run(db, "INSERT INTO kinds VALUES (1, 0, 0.5, '', x'', NULL), (2, 1, -1.25, 'one', x'01', 1), (3, -1, 1e300, 'é ü ß', x'00ff', 'x'), (4, 127, 3.14159265358979, 'a''b', x'deadbeef', 2.5), (5, 9223372036854775807, -0.0, '1', zeroblob(10), -9223372036854775808), (6, 255, 1e-300, 'long', NULL, 32767), (7, 65536, 100.0, NULL, x'7f', 8388608), (8, 4294967296, 0.1, 'x', x'80', 140737488355328);");
    run(db, "CREATE TABLE big(id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, payload BLOB, note TEXT);");
    run(db, "CREATE INDEX big_name ON big(name);");
    sqlite3_stmt *s = NULL;
    check(db, sqlite3_prepare_v2(db, "INSERT INTO big(name, payload, note) VALUES (?, ?, ?)", -1, &s, NULL), "prepare");
    run(db, "BEGIN;");
    static unsigned char blob[40000];
    static char text[6000];
    for (int i = 0; i < 3000; i++) {
        char name[32];
        snprintf(name, sizeof name, "name-%05d", (i * 7919) % 3000);
        int length = (i % 50 == 0) ? 1000 + (i * 37) % 39000 : (i * 13) % 200;
        fill(blob, length, (unsigned int)i);
        int textLength = (i % 97 == 0) ? 5000 : i % 40;
        for (int k = 0; k < textLength; k++)
            text[k] = (char)('a' + (k * 7 + i) % 26);
        text[textLength] = 0;
        sqlite3_bind_text(s, 1, name, -1, SQLITE_TRANSIENT);
        sqlite3_bind_blob(s, 2, blob, length, SQLITE_TRANSIENT);
        sqlite3_bind_text(s, 3, text, textLength, SQLITE_TRANSIENT);
        check(db, sqlite3_step(s), "insert");
        sqlite3_reset(s);
    }
    run(db, "COMMIT;");
    sqlite3_finalize(s);
    run(db, "DELETE FROM big WHERE id % 3 = 0;");
    run(db, "UPDATE big SET note = 'updated ' || id WHERE id % 5 = 1;");
    run(db, "CREATE TABLE kv(k TEXT UNIQUE, v);");
    run(db, "INSERT INTO kv SELECT name, length(payload) FROM big WHERE id < 400;");
    run(db, "DROP TABLE IF EXISTS gone; CREATE TABLE gone(x); INSERT INTO gone SELECT payload FROM big WHERE id < 100; DROP TABLE gone;");
    check(db, sqlite3_close(db), "close");
}

/* A value's digest: short ones as they are, long ones by length and a
   checksum. */
static void show_digest(sqlite3_stmt *s, int i)
{
    int type = sqlite3_column_type(s, i);
    if ((type == SQLITE_TEXT || type == SQLITE_BLOB) && sqlite3_column_bytes(s, i) > 40) {
        const unsigned char *data = type == SQLITE_TEXT ? sqlite3_column_text(s, i) : sqlite3_column_blob(s, i);
        int n = sqlite3_column_bytes(s, i);
        unsigned int sum = 0;
        for (int k = 0; k < n; k++)
            sum = sum * 31 + data[k];
        printf("%s[%d:%08x]", type == SQLITE_TEXT ? "text" : "blob", n, sum);
        return;
    }
    show_value(s, i);
}

static void dump(sqlite3 *db, const char *sql, int every)
{
    sqlite3_stmt *s = NULL;
    printf("> %s\n", sql);
    if (check(db, sqlite3_prepare_v2(db, sql, -1, &s, NULL), "prepare") != SQLITE_OK)
        return;
    int rows = 0;
    int rc;
    for (;;) {
        rc = sqlite3_step(s);
        if (rc != SQLITE_ROW)
            break;
        if (rows % every == 0) {
            printf("  ");
            for (int i = 0; i < sqlite3_column_count(s); i++) {
                if (i > 0)
                    printf("|");
                show_digest(s, i);
            }
            printf("\n");
        }
        rows++;
    }
    check(db, rc, "step");
    printf("  (%d rows)\n", rows);
    sqlite3_finalize(s);
}

static void read_database(const char *path)
{
    sqlite3 *db = NULL;
    check(db, sqlite3_open_v2(path, &db, SQLITE_OPEN_READWRITE, NULL), "open");
    dump(db, "PRAGMA integrity_check", 1);
    dump(db, "PRAGMA page_size", 1);
    dump(db, "PRAGMA user_version", 1);
    dump(db, "SELECT type, name, tbl_name, sql FROM sqlite_master ORDER BY name", 1);
    dump(db, "SELECT * FROM kinds ORDER BY id", 1);
    dump(db, "SELECT id, name, payload, note FROM big ORDER BY id", 37);
    dump(db, "SELECT count(*), sum(length(payload)), sum(length(note)), max(id) FROM big", 1);
    dump(db, "SELECT id, name FROM big WHERE name >= 'name-01500' ORDER BY name LIMIT 5", 1);
    dump(db, "SELECT name, seq FROM sqlite_sequence", 1);
    dump(db, "SELECT k, v FROM kv WHERE k = 'name-00100'", 1);
    dump(db, "SELECT count(*) FROM kv", 1);
    sqlite3_close(db);
}

/* A transaction left half done: its process ends before COMMIT, with
   pages written to the file (the cache is small) and its journal hot. */
static void crash(const char *path)
{
    fflush(stdout);
    pid_t child = fork();
    if (child == 0) {
        sqlite3 *db = NULL;
        sqlite3_open(path, &db);
        sqlite3_exec(db, "PRAGMA cache_size = 10; BEGIN; DELETE FROM big WHERE id < 2000; UPDATE kinds SET t = 'lost'; INSERT INTO kv VALUES ('lost', 1);", NULL, NULL, NULL);
        sqlite3_exec(db, "INSERT INTO big(name, payload) SELECT name || '-lost', payload FROM big;", NULL, NULL, NULL);
        _exit(0);
    }
    int status = 0;
    waitpid(child, &status, 0);
    printf("crashed: %d\n", status);
}

int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "write") == 0) {
        write_database(argv[2]);
    } else if (argc == 3 && strcmp(argv[1], "read") == 0) {
        read_database(argv[2]);
    } else if (argc == 3 && strcmp(argv[1], "crash") == 0) {
        crash(argv[2]);
    } else {
        write_database("files-test.db");
        read_database("files-test.db");
        crash("files-test.db");
        read_database("files-test.db");
        remove("files-test.db");
        remove("files-test.db-journal");
    }
    return 0;
}
