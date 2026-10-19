/* WebKit's own SQL (webkit_corpus.h: every statement WebCore's and
   WebKit's databases prepare), run the way their stores run it: a
   database per source file with foreign keys on and IndexedDB's IDBKEY
   collation, its CREATE statements first, its INSERTs twice with
   different bound values, then its queries, updates, deletes and
   schema changes. Each statement is prepared, its parameters bound
   (integers, texts, reals and blobs by position), stepped to its end
   and printed with its rows or its error. It prints what SQLite 3.45
   prints for the same program. */
#include "common.h"
#include "webkit_corpus.h"

/* IndexedDB's keys compare by IDBKEY; here as BINARY does (WebKit's
   compares serialized keys). */
static int idbkey(void *context, int lengthA, const void *a, int lengthB, const void *b)
{
    (void)context;
    int n = lengthA < lengthB ? lengthA : lengthB;
    int c = memcmp(a, b, n);
    if (c != 0)
        return c;
    return lengthA - lengthB;
}

static void bind_all(sqlite3_stmt *s, int round)
{
    int n = sqlite3_bind_parameter_count(s);
    for (int k = 1; k <= n; k++) {
        int kind = (k + round) % 4;
        if (kind == 0) {
            sqlite3_bind_int64(s, k, k * 10 + round);
        } else if (kind == 1) {
            char text[32];
            snprintf(text, sizeof text, "site%d.example", (k + round) % 3);
            sqlite3_bind_text(s, k, text, -1, SQLITE_TRANSIENT);
        } else if (kind == 2) {
            sqlite3_bind_double(s, k, k + round / 4.0);
        } else {
            unsigned char blob[2] = { (unsigned char)k, (unsigned char)round };
            sqlite3_bind_blob(s, k, blob, 2, SQLITE_TRANSIENT);
        }
    }
}

static void run_statement(sqlite3 *db, const struct corpus_statement *c, int round)
{
    sqlite3_stmt *s = NULL;
    printf("[%s:%d] %.60s\n", c->file, c->line, c->sql);
    int rc = sqlite3_prepare_v2(db, c->sql, -1, &s, NULL);
    if (rc != SQLITE_OK) {
        printf("  prepare error %d: %s\n", rc, sqlite3_errmsg(db));
        return;
    }
    if (!s)
        return;
    bind_all(s, round);
    int rows = 0;
    for (;;) {
        rc = sqlite3_step(s);
        if (rc != SQLITE_ROW)
            break;
        if (rows < 6)
            show_row(s);
        rows++;
    }
    if (rc != SQLITE_DONE)
        printf("  error %d (%d): %s\n", rc, sqlite3_extended_errcode(db), sqlite3_errmsg(db));
    else
        printf("  done: %d rows, %d changes\n", rows, sqlite3_stmt_readonly(s) ? 0 : sqlite3_changes(db));
    sqlite3_finalize(s);
}

static int starts(const char *sql, const char *word)
{
    return sqlite3_strnicmp(sql, word, (int)strlen(word)) == 0;
}

/* The phase a statement runs in. */
static int phase_of(const char *sql)
{
    if (starts(sql, "CREATE"))
        return 0;
    if (starts(sql, "INSERT") || starts(sql, "REPLACE"))
        return 1;
    if (starts(sql, "DELETE"))
        return 3;
    if (starts(sql, "ALTER") || starts(sql, "DROP"))
        return 4;
    return 2;
}

static void run_file(int first, int end)
{
    sqlite3 *db = open_memory();
    sqlite3_create_collation(db, "IDBKEY", SQLITE_UTF8, NULL, idbkey);
    run(db, "PRAGMA foreign_keys = ON;");
    printf("=== %s (%d statements)\n", corpus[first].file, end - first);
    for (int phase = 0; phase <= 4; phase++) {
        int rounds = phase == 1 ? 2 : 1;
        for (int round = 0; round < rounds; round++) {
            for (int i = first; i < end; i++) {
                if (phase_of(corpus[i].sql) == phase)
                    run_statement(db, &corpus[i], round);
            }
        }
    }
    run(db, "PRAGMA integrity_check;");
    run(db, "SELECT type, name, tbl_name FROM sqlite_master ORDER BY name;");
    sqlite3_close(db);
}

int main(void)
{
    int first = 0;
    int i = 0;
    for (;;) {
        if (!corpus[i].file || strcmp(corpus[i].file, corpus[first].file) != 0) {
            run_file(first, i);
            first = i;
        }
        if (!corpus[i].file)
            break;
        i++;
    }
    return 0;
}
