/* What the tests of mlx-sqlite3 share: running SQL text statement by
   statement (as sqlite3_exec does, through sqlite3_prepare_v2 and its
   tail) and printing each statement, the rows it returns with their
   types, and the error it ends with. The output is the same whichever
   library a test is built against: values are printed by type (a REAL
   as SQLite's own text for it). */
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

static void show_value(sqlite3_stmt *s, int i)
{
    int type = sqlite3_column_type(s, i);
    if (type == SQLITE_INTEGER) {
        printf("%lld", (long long)sqlite3_column_int64(s, i));
    } else if (type == SQLITE_FLOAT) {
        printf("r:%s", (const char *)sqlite3_column_text(s, i));
    } else if (type == SQLITE_TEXT) {
        printf("'%s'", (const char *)sqlite3_column_text(s, i));
    } else if (type == SQLITE_BLOB) {
        const unsigned char *data = sqlite3_column_blob(s, i);
        int n = sqlite3_column_bytes(s, i);
        printf("x'");
        for (int k = 0; k < n; k++)
            printf("%02x", data[k]);
        printf("'");
    } else {
        printf("NULL");
    }
}

static void show_row(sqlite3_stmt *s)
{
    int n = sqlite3_column_count(s);
    printf("  ");
    for (int i = 0; i < n; i++) {
        if (i > 0)
            printf("|");
        show_value(s, i);
    }
    printf("\n");
}

static void show_names(sqlite3_stmt *s)
{
    int n = sqlite3_column_count(s);
    if (n == 0)
        return;
    printf("  [");
    for (int i = 0; i < n; i++) {
        if (i > 0)
            printf("|");
        printf("%s", sqlite3_column_name(s, i));
    }
    printf("]\n");
}

/* Runs every statement of `sql`: the statement, its column names (when
   `names`), its rows, and how it ended. The result code of the last
   statement (or of the first that failed). */
static int run_with(sqlite3 *db, const char *sql, int names)
{
    const char *tail = sql;
    int rc = SQLITE_OK;
    while (tail && *tail) {
        sqlite3_stmt *s = NULL;
        const char *start = tail;
        rc = sqlite3_prepare_v2(db, start, -1, &s, &tail);
        if (rc != SQLITE_OK) {
            printf("> %s\n  prepare error %d: %s\n", start, rc, sqlite3_errmsg(db));
            return rc;
        }
        if (!s)
            break;
        printf("> %s\n", sqlite3_sql(s));
        if (names)
            show_names(s);
        for (;;) {
            rc = sqlite3_step(s);
            if (rc != SQLITE_ROW)
                break;
            show_row(s);
        }
        if (rc != SQLITE_DONE) {
            int extended = sqlite3_extended_errcode(db);
            printf("  error %d (%d): %s\n", rc, extended, sqlite3_errmsg(db));
        }
        int finalized = sqlite3_finalize(s);
        if (rc != SQLITE_DONE)
            return finalized;
        rc = SQLITE_OK;
    }
    return rc;
}

static int run(sqlite3 *db, const char *sql) { return run_with(db, sql, 0); }

static sqlite3 *open_memory(void)
{
    sqlite3 *db = NULL;
    int rc = sqlite3_open(":memory:", &db);
    if (rc != SQLITE_OK)
        printf("open: %d\n", rc);
    return db;
}
