/* mlx-sqlite3's extension points: application functions (scalar ones
   with their user data, results of every type, errors, a function that
   replaces a built-in as WebCore's SQLiteDatabase replaces printf, the
   sqlite3_value_* accessors of their arguments, aggregates), collations
   (sqlite3_create_collation and _v2 with its destructor, in ORDER BY,
   comparisons, indexes and DISTINCT), the authorizer (the actions and
   names it is asked about, SQLITE_DENY and SQLITE_IGNORE), incremental
   blob I/O, sqlite3_interrupt-free busy handling between two
   connections (sqlite3_busy_handler and _busy_timeout), the WAL hook
   and checkpoints WebCore calls, and sqlite3_next_stmt. It prints what
   SQLite 3.45 prints for the same program. */
#include "common.h"
#include <stdlib.h>

static void describe(sqlite3_context *context, int argc, sqlite3_value **argv)
{
    char text[256];
    int used = snprintf(text, sizeof text, "%s(%d):", (const char *)sqlite3_user_data(context), argc);
    for (int i = 0; i < argc; i++) {
        sqlite3_value *v = argv[i];
        int type = sqlite3_value_type(v);
        if (type == SQLITE_INTEGER)
            used += snprintf(text + used, sizeof text - used, " i%lld", (long long)sqlite3_value_int64(v));
        else if (type == SQLITE_FLOAT)
            used += snprintf(text + used, sizeof text - used, " r%g", sqlite3_value_double(v));
        else if (type == SQLITE_TEXT)
            used += snprintf(text + used, sizeof text - used, " t'%s'/%d", (const char *)sqlite3_value_text(v), sqlite3_value_bytes(v));
        else if (type == SQLITE_BLOB)
            used += snprintf(text + used, sizeof text - used, " b%d", sqlite3_value_bytes(v));
        else
            used += snprintf(text + used, sizeof text - used, " null");
    }
    sqlite3_result_text(context, text, -1, SQLITE_TRANSIENT);
}

static void typed(sqlite3_context *context, int argc, sqlite3_value **argv)
{
    int which = sqlite3_value_int(argv[0]);
    if (which == 0)
        sqlite3_result_int(context, 42);
    else if (which == 1)
        sqlite3_result_int64(context, (sqlite3_int64)1 << 40);
    else if (which == 2)
        sqlite3_result_double(context, 0.25);
    else if (which == 3)
        sqlite3_result_text(context, "text", 2, SQLITE_STATIC);
    else if (which == 4)
        sqlite3_result_blob(context, "\x01\x02", 2, SQLITE_TRANSIENT);
    else if (which == 5)
        sqlite3_result_null(context);
    else if (which == 6)
        sqlite3_result_zeroblob(context, 3);
    else if (which == 7)
        sqlite3_result_value(context, argv[1]);
    else if (which == 8)
        sqlite3_result_error(context, "custom failure", -1);
    else if (which == 9)
        sqlite3_result_error_code(context, SQLITE_CONSTRAINT);
    else
        sqlite3_result_error_nomem(context);
}

/* WebCore's SQLiteDatabase::unauthorizedSQLFunction. */
static void unauthorized(sqlite3_context *context, int argc, sqlite3_value **argv)
{
    (void)argc;
    (void)argv;
    char message[128];
    snprintf(message, sizeof message, "Function %s is unauthorized", (const char *)sqlite3_user_data(context));
    sqlite3_result_error(context, message, -1);
}

struct sum_state {
    long long total;
    int count;
};

static void sum_step(sqlite3_context *context, int argc, sqlite3_value **argv)
{
    (void)argc;
    struct sum_state *state = sqlite3_aggregate_context(context, sizeof *state);
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL)
        return;
    state->total += sqlite3_value_int64(argv[0]);
    state->count++;
}

static void sum_final(sqlite3_context *context)
{
    struct sum_state *state = sqlite3_aggregate_context(context, 0);
    if (!state) {
        sqlite3_result_text(context, "empty", -1, SQLITE_STATIC);
        return;
    }
    char text[64];
    snprintf(text, sizeof text, "%lld/%d", state->total, state->count);
    sqlite3_result_text(context, text, -1, SQLITE_TRANSIENT);
}

/* Compares texts by their length, then as BINARY. */
static int by_length(void *context, int lengthA, const void *a, int lengthB, const void *b)
{
    (void)context;
    if (lengthA != lengthB)
        return lengthA - lengthB;
    return memcmp(a, b, lengthA);
}

static int destroyed;

static void collation_destroyed(void *context)
{
    destroyed++;
    printf("  collation destroyed: %s\n", (const char *)context);
}

static void functions(sqlite3 *db)
{
    printf("== functions\n");
    printf("create %d\n", sqlite3_create_function(db, "describe", -1, SQLITE_UTF8, "describe", describe, NULL, NULL));
    printf("create %d\n", sqlite3_create_function(db, "typed", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC, NULL, typed, NULL, NULL));
    printf("create %d\n", sqlite3_create_function(db, "printf", -1, SQLITE_UTF8, "printf", unauthorized, NULL, NULL));
    printf("create %d\n", sqlite3_create_function(db, "eval", 1, SQLITE_UTF8, "eval", unauthorized, NULL, NULL));
    printf("create %d\n", sqlite3_create_function(db, "mysum", 1, SQLITE_UTF8, NULL, NULL, sum_step, sum_final));
    printf("bad create %d\n", sqlite3_create_function(db, "bad", 200, SQLITE_UTF8, NULL, describe, NULL, NULL));
    run(db, "SELECT describe(1, 2.5, 'x', x'0102', NULL), describe(), DESCRIBE('a' || 'b');");
    for (int i = 0; i <= 7; i++) {
        char sql[64];
        snprintf(sql, sizeof sql, "SELECT typed(%d, 'v'), typeof(typed(%d, 7));", i, i);
        run(db, sql);
    }
    run(db, "SELECT typed(8, 0);");
    run(db, "SELECT typed(9, 0);");
    run(db, "SELECT printf('%d', 1);");
    run(db, "SELECT eval('1');");
    run(db, "SELECT format('%d', 1);");
    run(db, "SELECT typed(1);");
    run(db, "CREATE TABLE f(g, v); INSERT INTO f VALUES (1, 10), (1, 20), (2, NULL), (2, 5), (3, NULL);");
    run(db, "SELECT mysum(v) FROM f;");
    run(db, "SELECT g, mysum(v) FROM f GROUP BY g;");
    run(db, "SELECT mysum(v) FROM f WHERE 0;");
    run(db, "SELECT describe(v, g) FROM f WHERE g = 2 ORDER BY v;");
    printf("delete %d\n", sqlite3_create_function(db, "describe", -1, SQLITE_UTF8, NULL, NULL, NULL, NULL));
    run(db, "SELECT describe(1);");
}

static void collations(sqlite3 *db)
{
    printf("== collations\n");
    printf("create %d\n", sqlite3_create_collation(db, "BYLENGTH", SQLITE_UTF8, NULL, by_length));
    printf("create v2 %d\n", sqlite3_create_collation_v2(db, "bylength2", SQLITE_UTF8, "bylength2", by_length, collation_destroyed));
    run(db, "CREATE TABLE w(s TEXT COLLATE BYLENGTH, t TEXT);");
    run(db, "INSERT INTO w VALUES ('ccc', 'ccc'), ('a', 'a'), ('bb', 'bb'), ('dd', 'dd'), ('e', 'e'), ('ccc', 'x');");
    run(db, "SELECT s FROM w ORDER BY s;");
    run(db, "SELECT t FROM w ORDER BY t COLLATE bylength2 DESC, t;");
    run(db, "SELECT s FROM w WHERE s = 'zz' ORDER BY s, t;");
    run(db, "SELECT DISTINCT s FROM w ORDER BY 1;");
    run(db, "CREATE INDEX w_s ON w(s);");
    run(db, "SELECT s, t FROM w WHERE s > 'z' ORDER BY s, t;");
    run(db, "SELECT max(s), min(s), max(t) FROM w;");
    run(db, "SELECT t FROM w ORDER BY t COLLATE nosuch;");
    printf("replace v2 %d destroyed %d\n", sqlite3_create_collation_v2(db, "bylength2", SQLITE_UTF8, "second", by_length, collation_destroyed), destroyed);
}

static int authorizer_calls;

static int authorizer(void *context, int action, const char *a, const char *b, const char *database, const char *trigger)
{
    (void)trigger;
    int answer = SQLITE_OK;
    const char *deny = context;
    if (a && deny && strcmp(a, deny) == 0)
        answer = SQLITE_DENY;
    if (action == SQLITE_READ && b && strcmp(b, "secret") == 0)
        answer = SQLITE_IGNORE;
    authorizer_calls++;
    printf("  auth %d %s %s %s -> %d\n", action, a ? a : "-", b ? b : "-", database ? database : "-", answer);
    return answer;
}

static void authorizing(sqlite3 *db)
{
    printf("== authorizer\n");
    run(db, "CREATE TABLE au(id INTEGER PRIMARY KEY, name TEXT, secret TEXT); INSERT INTO au VALUES (1, 'n', 's');");
    sqlite3_set_authorizer(db, authorizer, "forbidden");
    run(db, "SELECT name, secret FROM au WHERE id = 1;");
    run(db, "INSERT INTO au(name) VALUES ('m');");
    run(db, "UPDATE au SET name = 'x' WHERE id = 2;");
    run(db, "DELETE FROM au WHERE id = 2;");
    run(db, "CREATE TABLE forbidden(a);");
    run(db, "CREATE TABLE allowed(a);");
    run(db, "CREATE INDEX allowed_a ON allowed(a);");
    run(db, "DROP TABLE allowed;");
    run(db, "PRAGMA user_version;");
    run(db, "BEGIN; COMMIT;");
    run(db, "SELECT count(*) FROM au;");
    sqlite3_set_authorizer(db, NULL, NULL);
    run(db, "SELECT secret FROM au;");
}

static void blobs(sqlite3 *db)
{
    printf("== blobs\n");
    run(db, "CREATE TABLE bl(id INTEGER PRIMARY KEY, data BLOB, note TEXT, n INTEGER); INSERT INTO bl VALUES (1, x'00112233445566', 'hello', 5), (2, zeroblob(4), NULL, 6);");
    run(db, "CREATE INDEX bl_note ON bl(note);");
    sqlite3_blob *b = NULL;
    int rc = sqlite3_blob_open(db, "main", "bl", "data", 1, 1, &b);
    printf("open %d bytes %d\n", rc, sqlite3_blob_bytes(b));
    unsigned char buffer[8] = { 0 };
    rc = sqlite3_blob_read(b, buffer, 3, 2);
    printf("read %d: %02x %02x %02x\n", rc, buffer[0], buffer[1], buffer[2]);
    rc = sqlite3_blob_read(b, buffer, 8, 0);
    printf("read past the end %d\n", rc);
    rc = sqlite3_blob_write(b, "\xaa\xbb", 2, 5);
    printf("write %d\n", rc);
    rc = sqlite3_blob_reopen(b, 2);
    printf("reopen %d bytes %d\n", rc, sqlite3_blob_bytes(b));
    rc = sqlite3_blob_write(b, "\xff", 1, 0);
    printf("write %d\n", rc);
    rc = sqlite3_blob_reopen(b, 9);
    printf("reopen missing %d %s\n", rc, sqlite3_errmsg(db));
    printf("close %d\n", sqlite3_blob_close(b));
    run(db, "SELECT id, hex(data) FROM bl;");
    b = NULL;
    rc = sqlite3_blob_open(db, "main", "bl", "note", 1, 0, &b);
    printf("open text %d bytes %d\n", rc, sqlite3_blob_bytes(b));
    rc = sqlite3_blob_write(b, "J", 1, 0);
    printf("write read-only %d\n", rc);
    sqlite3_blob_close(b);
    rc = sqlite3_blob_open(db, "main", "bl", "note", 1, 1, &b);
    printf("open indexed for writing %d %s\n", rc, sqlite3_errmsg(db));
    rc = sqlite3_blob_open(db, "main", "bl", "n", 1, 0, &b);
    printf("open integer %d %s\n", rc, sqlite3_errmsg(db));
    rc = sqlite3_blob_open(db, "main", "bl", "note", 2, 0, &b);
    printf("open null %d %s\n", rc, sqlite3_errmsg(db));
    rc = sqlite3_blob_open(db, "main", "nosuch", "x", 1, 0, &b);
    printf("open no table %d %s\n", rc, sqlite3_errmsg(db));
    rc = sqlite3_blob_open(db, "main", "bl", "nosuch", 1, 0, &b);
    printf("open no column %d %s\n", rc, sqlite3_errmsg(db));
}

static int busy_calls;

static int busy(void *context, int count)
{
    (void)context;
    busy_calls++;
    return count < 2;
}

static int wal_hook_calls;

static int wal_hook(void *context, sqlite3 *db, const char *name, int pages)
{
    (void)context;
    (void)db;
    (void)name;
    (void)pages;
    wal_hook_calls++;
    return SQLITE_OK;
}

static void locking(void)
{
    printf("== two connections\n");
    remove("extend-test.db");
    sqlite3 *a = NULL;
    sqlite3 *b = NULL;
    sqlite3_open("extend-test.db", &a);
    sqlite3_open("extend-test.db", &b);
    run(a, "CREATE TABLE l(x); INSERT INTO l VALUES (1);");
    run(a, "BEGIN IMMEDIATE; INSERT INTO l VALUES (2);");
    sqlite3_busy_handler(b, busy, NULL);
    run(b, "INSERT INTO l VALUES (3);");
    printf("busy handler calls %d\n", busy_calls);
    run(b, "SELECT count(*) FROM l;");
    sqlite3_busy_timeout(b, 50);
    run(b, "BEGIN IMMEDIATE;");
    run(a, "COMMIT;");
    run(b, "INSERT INTO l VALUES (4); SELECT count(*) FROM l;");
    printf("autocommit a %d b %d\n", sqlite3_get_autocommit(a), sqlite3_get_autocommit(b));
    sqlite3_stmt *s = NULL;
    sqlite3_prepare_v2(a, "SELECT x FROM l", -1, &s, NULL);
    sqlite3_step(s);
    run(b, "INSERT INTO l VALUES (5);");
    sqlite3_finalize(s);
    run(b, "INSERT INTO l VALUES (6); SELECT count(*) FROM l;");
    printf("== statements and WAL\n");
    sqlite3_stmt *first = NULL;
    sqlite3_stmt *second = NULL;
    sqlite3_prepare_v2(a, "SELECT 1", -1, &first, NULL);
    sqlite3_prepare_v2(a, "SELECT 2", -1, &second, NULL);
    int count = 0;
    for (sqlite3_stmt *t = sqlite3_next_stmt(a, NULL); t; t = sqlite3_next_stmt(a, t))
        count++;
    printf("statements %d\n", count);
    sqlite3_finalize(first);
    sqlite3_finalize(second);
    printf("next after finalize %s\n", sqlite3_next_stmt(a, NULL) ? "set" : "NULL");
    sqlite3_wal_hook(a, wal_hook, NULL);
    int log = -1;
    int done = -1;
    printf("checkpoint (not WAL) %d log %d done %d\n", sqlite3_wal_checkpoint_v2(a, NULL, SQLITE_CHECKPOINT_TRUNCATE, &log, &done), log, done);
    sqlite3_close(b);
    sqlite3_close(a);
    remove("extend-test.db");
}

int main(void)
{
    sqlite3 *db = open_memory();
    functions(db);
    collations(db);
    authorizing(db);
    blobs(db);
    printf("close %d destroyed %d\n", sqlite3_close(db), destroyed);
    locking();
    return 0;
}
