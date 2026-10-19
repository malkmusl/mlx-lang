/* mlx-sqlite3's C API around statements and connections: the version,
   opening (sqlite3_open, _v2 with its flags, a file that is not a
   database), prepare with tails and sqlite3_sql, the binds of every type
   by number and by name (and their errors), the column accessors and
   their conversions, names and declared types, reset and
   clear_bindings, step after DONE, expanded_sql, stmt_readonly and
   _busy, changes, total_changes and last_insert_rowid, errcode,
   extended_errcode, errmsg and errstr, sqlite3_exec with its callback
   and error message, sqlite3_get_table, sqlite3_complete, the printf
   family, stricmp and strglob, and the UTF-16 entry points. It prints
   what SQLite 3.45 prints for the same program. */
#include "common.h"
#include <stdlib.h>

static int rows_seen;

static int exec_callback(void *argument, int columns, char **values, char **names)
{
    printf("  callback %s:", (const char *)argument);
    for (int i = 0; i < columns; i++)
        printf(" %s=%s", names[i], values[i] ? values[i] : "(null)");
    printf("\n");
    rows_seen++;
    return rows_seen >= 3 ? 1 : 0;
}

/* The destructor of a bound text: SQLite calls it when it is done with
   the text (when is its own business: the count is printed after the
   statement is finalized). */
static int destructor_calls;

static void freed(void *data)
{
    destructor_calls++;
    free(data);
}

static void basics(void)
{
    printf("== version\n");
    printf("%s %d %d\n", sqlite3_libversion(), sqlite3_libversion_number(), SQLITE_VERSION_NUMBER);
    printf("threadsafe %d\n", sqlite3_threadsafe() != 0);
    printf("complete: %d %d %d %d %d\n", sqlite3_complete("SELECT 1;"), sqlite3_complete("SELECT 1"), sqlite3_complete("SELECT 'a;"), sqlite3_complete("CREATE TRIGGER t AFTER INSERT ON x BEGIN SELECT 1; END;"), sqlite3_complete("-- c\n;"));
    printf("stricmp: %d %d %d\n", sqlite3_stricmp("abc", "ABC") == 0, sqlite3_stricmp("a", "b") < 0, sqlite3_strnicmp("abcd", "ABCE", 3) == 0);
    printf("strglob: %d %d strlike: %d %d\n", sqlite3_strglob("a*c", "abbc"), sqlite3_strglob("a?c", "abbc") != 0, sqlite3_strlike("A%", "abc", 0), sqlite3_strlike("a\\%", "a%", '\\'));
    printf("errstr: %s | %s | %s | %s\n", sqlite3_errstr(SQLITE_OK), sqlite3_errstr(SQLITE_CONSTRAINT), sqlite3_errstr(SQLITE_BUSY), sqlite3_errstr(SQLITE_RANGE));
    char *text = sqlite3_mprintf("%d %s %q %Q %Q %.3f %5.1f|%-4d|%04x|%lld|%c%%", 42, "str", "it's", "q'q", (char *)0, 3.14159, 2.25, 7, 255, (long long)1 << 40, 'z');
    printf("mprintf: %s\n", text);
    sqlite3_free(text);
    char buffer[16];
    sqlite3_snprintf(sizeof buffer, buffer, "%s-%d-%s", "abcdefgh", 12345, "tail");
    printf("snprintf: %s\n", buffer);
    text = sqlite3_mprintf("%w", "a\"b");
    printf("%%w: %s\n", text);
    sqlite3_free(text);
}

static void opening(void)
{
    printf("== open\n");
    sqlite3 *db = NULL;
    int rc = sqlite3_open_v2("missing-dir/x.db", &db, SQLITE_OPEN_READWRITE, NULL);
    printf("open missing: %d %s\n", rc, db ? sqlite3_errmsg(db) : "(no db)");
    sqlite3_close(db);
    db = NULL;
    rc = sqlite3_open_v2("api-test.db", &db, SQLITE_OPEN_READONLY, NULL);
    printf("open read-only, not there: %d\n", rc);
    sqlite3_close(db);
    FILE *junk = fopen("junk.db", "w");
    fputs("this is not a database file at all, it is just text that goes on and on for a while to be long enough", junk);
    fclose(junk);
    db = NULL;
    rc = sqlite3_open("junk.db", &db);
    printf("open junk: %d\n", rc);
    rc = run(db, "SELECT * FROM sqlite_master;");
    printf("read junk: %d %d %s\n", rc, sqlite3_errcode(db), sqlite3_errmsg(db));
    sqlite3_close(db);
    db = NULL;
    rc = sqlite3_open("api-test.db", &db);
    printf("open file: %d autocommit %d readonly %d\n", rc, sqlite3_get_autocommit(db), sqlite3_db_readonly(db, "main"));
    run(db, "CREATE TABLE f(x); INSERT INTO f VALUES (1), (2);");
    sqlite3_close(db);
    db = NULL;
    rc = sqlite3_open_v2("api-test.db", &db, SQLITE_OPEN_READONLY, NULL);
    printf("open read-only: %d readonly %d\n", rc, sqlite3_db_readonly(db, "main"));
    run(db, "SELECT x FROM f;");
    run(db, "INSERT INTO f VALUES (3);");
    sqlite3_close(db);
    printf("close NULL: %d\n", sqlite3_close(NULL));
}

static void statements(sqlite3 *db)
{
    printf("== prepare\n");
    const char *sql = "  SELECT 1; SELECT 2 ;  -- comment\n  ";
    const char *tail = NULL;
    sqlite3_stmt *s = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &s, &tail);
    printf("rc %d sql [%s] tail [%s]\n", rc, sqlite3_sql(s), tail);
    sqlite3_finalize(s);
    rc = sqlite3_prepare_v2(db, tail, -1, &s, &tail);
    printf("rc %d sql [%s] tail [%s]\n", rc, sqlite3_sql(s), tail);
    sqlite3_finalize(s);
    rc = sqlite3_prepare_v2(db, tail, -1, &s, &tail);
    printf("rc %d stmt %s tail [%s]\n", rc, s ? "set" : "NULL", tail);
    rc = sqlite3_prepare_v2(db, "SELECT 12345", 9, &s, &tail);
    printf("rc %d sql [%s] tail [%s]\n", rc, sqlite3_sql(s), tail);
    sqlite3_step(s);
    printf("value %d\n", sqlite3_column_int(s, 0));
    sqlite3_finalize(s);
    rc = sqlite3_prepare_v2(db, "SELECT * FROM nowhere", -1, &s, NULL);
    printf("rc %d stmt %s errcode %d msg %s\n", rc, s ? "set" : "NULL", sqlite3_errcode(db), sqlite3_errmsg(db));
    rc = sqlite3_prepare_v2(db, "SELECT 1 FROM", -1, &s, NULL);
    printf("rc %d msg %s\n", rc, sqlite3_errmsg(db));

    printf("== bind\n");
    run(db, "CREATE TABLE b(i INTEGER, r REAL, t TEXT, x BLOB, n NUMERIC, a ANY_TYPE);");
    rc = sqlite3_prepare_v2(db, "INSERT INTO b VALUES (?, ?2, :text, @blob, $num, ?6)", -1, &s, NULL);
    printf("count %d names %s %s %s %s %s %s\n", sqlite3_bind_parameter_count(s), sqlite3_bind_parameter_name(s, 1) ? "?" : "(null)", sqlite3_bind_parameter_name(s, 2), sqlite3_bind_parameter_name(s, 3), sqlite3_bind_parameter_name(s, 4), sqlite3_bind_parameter_name(s, 5), sqlite3_bind_parameter_name(s, 6));
    printf("index %d %d %d %d\n", sqlite3_bind_parameter_index(s, ":text"), sqlite3_bind_parameter_index(s, "@blob"), sqlite3_bind_parameter_index(s, "$num"), sqlite3_bind_parameter_index(s, "text"));
    sqlite3_bind_int(s, 1, 10);
    sqlite3_bind_double(s, 2, 2.5);
    sqlite3_bind_text(s, 3, "hello", -1, SQLITE_STATIC);
    sqlite3_bind_blob(s, 4, "\x01\x02\x03", 3, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 5, "42", -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(s, 6, (sqlite3_int64)1 << 50);
    printf("step %d changes %d rowid %lld\n", sqlite3_step(s), sqlite3_changes(db), (long long)sqlite3_last_insert_rowid(db));
    printf("bind while done: %d\n", sqlite3_bind_int(s, 1, 1));
    sqlite3_reset(s);
    printf("range: %d %d errcode %d %s\n", sqlite3_bind_int(s, 0, 1), sqlite3_bind_int(s, 7, 1), sqlite3_errcode(db), sqlite3_errmsg(db));
    sqlite3_bind_text(s, 1, "11", -1, SQLITE_STATIC);
    sqlite3_bind_text(s, 2, "3", -1, SQLITE_STATIC);
    sqlite3_bind_int(s, 3, 7);
    sqlite3_bind_zeroblob(s, 4, 4);
    sqlite3_bind_double(s, 5, 3.0);
    sqlite3_bind_text(s, 6, "abc\0def", 7, SQLITE_STATIC);
    printf("step %d\n", sqlite3_step(s));
    sqlite3_reset(s);
    sqlite3_bind_null(s, 1);
    sqlite3_bind_text(s, 2, "not a number", -1, SQLITE_STATIC);
    sqlite3_bind_blob(s, 3, NULL, 0, SQLITE_STATIC);
    sqlite3_bind_text(s, 4, "text into blob", -1, SQLITE_STATIC);
    sqlite3_bind_text(s, 5, "1e3", -1, SQLITE_STATIC);
    char *owned = strdup("owned");
    sqlite3_bind_text(s, 6, owned, -1, freed);
    printf("step %d\n", sqlite3_step(s));
    sqlite3_reset(s);
    sqlite3_clear_bindings(s);
    printf("step %d\n", sqlite3_step(s));
    sqlite3_reset(s);
    sqlite3_bind_text(s, 1, "  12  ", -1, SQLITE_STATIC);
    sqlite3_bind_text(s, 2, "0x10", -1, SQLITE_STATIC);
    sqlite3_bind_text(s, 5, "12.0", -1, SQLITE_STATIC);
    sqlite3_bind_text(s, 6, "9223372036854775808", -1, SQLITE_STATIC);
    printf("step %d\n", sqlite3_step(s));
    printf("finalize %d destructor calls %d\n", sqlite3_finalize(s), destructor_calls);
    run(db, "SELECT rowid, i, typeof(i), r, typeof(r), t, typeof(t), x, typeof(x), n, typeof(n), a, typeof(a), length(a) FROM b;");

    printf("== columns\n");
    rc = sqlite3_prepare_v2(db, "SELECT i, r AS real_value, t, x, n, i + 1, b.a, NULL, rowid FROM b ORDER BY rowid", -1, &s, NULL);
    int n = sqlite3_column_count(s);
    printf("columns %d data_count before step %d\n", n, sqlite3_data_count(s));
    for (int i = 0; i < n; i++) {
        const char *type = sqlite3_column_decltype(s, i);
        printf("  %d name %s decltype %s\n", i, sqlite3_column_name(s, i), type ? type : "(null)");
    }
    printf("readonly %d busy %d\n", sqlite3_stmt_readonly(s), sqlite3_stmt_busy(s));
    rc = sqlite3_step(s);
    printf("step %d busy %d data_count %d\n", rc, sqlite3_stmt_busy(s), sqlite3_data_count(s));
    for (int i = 0; i < n; i++) {
        int type = sqlite3_column_type(s, i);
        const unsigned char *text = sqlite3_column_text(s, i);
        int bytes = sqlite3_column_bytes(s, i);
        printf("  %d type %d int %d int64 %lld double %g text %s bytes %d type after %d\n", i, type, sqlite3_column_int(s, i), (long long)sqlite3_column_int64(s, i), sqlite3_column_double(s, i), text ? (const char *)text : "(null)", bytes, sqlite3_column_type(s, i));
    }
    printf("out of range: type %d text %s\n", sqlite3_column_type(s, 20), sqlite3_column_text(s, 20) ? "set" : "(null)");
    rc = sqlite3_step(s);
    const void *blob = sqlite3_column_blob(s, 3);
    printf("step %d blob bytes %d first %d\n", rc, sqlite3_column_bytes(s, 3), blob ? ((const unsigned char *)blob)[0] : -1);
    printf("blob of '': %s\n", sqlite3_column_blob(s, 2) ? "set" : "(null)");
    for (;;) {
        rc = sqlite3_step(s);
        if (rc != SQLITE_ROW)
            break;
    }
    printf("end %d busy %d data_count %d\n", rc, sqlite3_stmt_busy(s), sqlite3_data_count(s));
    rc = sqlite3_step(s);
    printf("step after done %d\n", rc);
    sqlite3_finalize(s);

    printf("== expanded\n");
    rc = sqlite3_prepare_v2(db, "SELECT ?1, :a, ?, 'lit?', ?5", -1, &s, NULL);
    sqlite3_bind_int(s, 1, 5);
    sqlite3_bind_text(s, 2, "it's", -1, SQLITE_STATIC);
    sqlite3_bind_double(s, 3, 1.5);
    sqlite3_bind_blob(s, 5, "\xab\xcd", 2, SQLITE_STATIC);
    char *expanded = sqlite3_expanded_sql(s);
    printf("%s\n", expanded);
    sqlite3_free(expanded);
    printf("count %d\n", sqlite3_bind_parameter_count(s));
    sqlite3_finalize(s);

    printf("== readonly\n");
    const char *kinds[] = { "SELECT 1", "INSERT INTO b(i) VALUES (1)", "CREATE TABLE z(a)", "BEGIN", "COMMIT", "PRAGMA user_version", "PRAGMA user_version = 3", "UPDATE b SET i = 1 WHERE 0", "DELETE FROM b WHERE 0", "SELECT * FROM b" };
    for (int i = 0; i < 10; i++) {
        rc = sqlite3_prepare_v2(db, kinds[i], -1, &s, NULL);
        printf("  %s: %d %d\n", kinds[i], rc, rc == SQLITE_OK ? sqlite3_stmt_readonly(s) : -1);
        sqlite3_finalize(s);
    }
}

static void changes(sqlite3 *db)
{
    printf("== changes\n");
    run(db, "CREATE TABLE c(id INTEGER PRIMARY KEY, v);");
    int before = sqlite3_total_changes(db);
    run(db, "INSERT INTO c(v) VALUES (1), (2), (3);");
    printf("changes %d total +%d rowid %lld\n", sqlite3_changes(db), sqlite3_total_changes(db) - before, (long long)sqlite3_last_insert_rowid(db));
    run(db, "UPDATE c SET v = v * 10 WHERE id > 1;");
    printf("changes %d total +%d\n", sqlite3_changes(db), sqlite3_total_changes(db) - before);
    run(db, "DELETE FROM c WHERE v = 20;");
    printf("changes %d total +%d\n", sqlite3_changes(db), sqlite3_total_changes(db) - before);
    run(db, "SELECT * FROM c;");
    printf("changes after select %d\n", sqlite3_changes(db));
    run(db, "INSERT INTO c VALUES (100, 'x');");
    printf("rowid %lld\n", (long long)sqlite3_last_insert_rowid(db));
    run(db, "DELETE FROM c;");
    printf("changes %d (delete all) %lld\n", sqlite3_changes(db), (long long)sqlite3_changes64(db));
    run(db, "INSERT INTO c VALUES (NULL, 'y');");
    printf("rowid %lld\n", (long long)sqlite3_last_insert_rowid(db));
}

static void exec_and_tables(sqlite3 *db)
{
    printf("== exec\n");
    char *message = NULL;
    rows_seen = 0;
    int rc = sqlite3_exec(db, "CREATE TABLE e(k, v); INSERT INTO e VALUES ('a', 1), ('b', NULL), ('c', 3.5), ('d', x'00'); SELECT * FROM e; SELECT 'never';", exec_callback, "first", &message);
    printf("exec %d message %s\n", rc, message ? message : "(none)");
    sqlite3_free(message);
    message = NULL;
    rc = sqlite3_exec(db, "SELECT k FROM e WHERE v IS NULL; SELECT nosuch FROM e; SELECT 2;", exec_callback, "second", &message);
    printf("exec %d message %s errmsg %s\n", rc, message ? message : "(none)", sqlite3_errmsg(db));
    sqlite3_free(message);
    rc = sqlite3_exec(db, "INSERT INTO e VALUES ('z', 26)", NULL, NULL, NULL);
    printf("exec %d changes %d\n", rc, sqlite3_changes(db));
    rc = sqlite3_exec(db, "", NULL, NULL, NULL);
    printf("exec empty %d\n", rc);

    printf("== get_table\n");
    char **result = NULL;
    int rows = 0;
    int columns = 0;
    rc = sqlite3_get_table(db, "SELECT k, v FROM e ORDER BY k", &result, &rows, &columns, &message);
    printf("get_table %d rows %d columns %d\n", rc, rows, columns);
    for (int i = 0; i < (rows + 1) * columns; i++)
        printf("  [%d] %s\n", i, result[i] ? result[i] : "(null)");
    sqlite3_free_table(result);
    rc = sqlite3_get_table(db, "SELECT nothing FROM e", &result, &rows, &columns, &message);
    printf("get_table %d message %s\n", rc, message ? message : "(none)");
    sqlite3_free(message);
}

static void errors(sqlite3 *db)
{
    printf("== errors\n");
    run(db, "CREATE TABLE u(a UNIQUE, b NOT NULL);");
    run(db, "INSERT INTO u VALUES (1, 1);");
    run(db, "INSERT INTO u VALUES (1, 2);");
    printf("errcode %d extended %d\n", sqlite3_errcode(db), sqlite3_extended_errcode(db));
    run(db, "INSERT INTO u VALUES (2, NULL);");
    printf("errcode %d extended %d\n", sqlite3_errcode(db), sqlite3_extended_errcode(db));
    sqlite3_stmt *s = NULL;
    sqlite3_prepare_v2(db, "INSERT INTO u VALUES (1, 3)", -1, &s, NULL);
    int rc = sqlite3_step(s);
    printf("step %d errcode %d msg %s\n", rc, sqlite3_errcode(db), sqlite3_errmsg(db));
    rc = sqlite3_reset(s);
    printf("reset %d errcode %d\n", rc, sqlite3_errcode(db));
    rc = sqlite3_reset(s);
    printf("reset again %d\n", rc);
    sqlite3_finalize(s);
    sqlite3_extended_result_codes(db, 1);
    sqlite3_prepare_v2(db, "INSERT INTO u VALUES (1, 3)", -1, &s, NULL);
    rc = sqlite3_step(s);
    printf("extended step %d\n", rc);
    rc = sqlite3_finalize(s);
    printf("extended finalize %d\n", rc);
    sqlite3_extended_result_codes(db, 0);
    sqlite3_prepare(db, "INSERT INTO u VALUES (1, 3)", -1, &s, NULL);
    rc = sqlite3_step(s);
    printf("legacy step %d\n", rc);
    rc = sqlite3_reset(s);
    printf("legacy reset %d\n", rc);
    sqlite3_finalize(s);
    printf("finalize NULL %d\n", sqlite3_finalize(NULL));
    run(db, "SELECT 1;");
    printf("after success errcode %d msg %s\n", sqlite3_errcode(db), sqlite3_errmsg(db));
}

static void utf16(sqlite3 *db)
{
    printf("== utf16\n");
    static const unsigned short sql[] = { 'S', 'E', 'L', 'E', 'C', 'T', ' ', '\'', 0xe9, 't', 0xe9, '\'', ',', ' ', '?', ';', ' ', 'S', 'E', 'L', 'E', 'C', 'T', ' ', '2', 0 };
    sqlite3_stmt *s = NULL;
    const void *tail = NULL;
    int rc = sqlite3_prepare16_v2(db, sql, -1, &s, &tail);
    printf("prepare16 %d tail at unit %d\n", rc, (int)((const unsigned short *)tail - sql));
    static const unsigned short word[] = { 'n', 0xf6, 0x20ac, 0 };
    sqlite3_bind_text16(s, 1, word, -1, SQLITE_STATIC);
    rc = sqlite3_step(s);
    const unsigned short *text = sqlite3_column_text16(s, 0);
    printf("step %d bytes16 %d bytes %d units:", rc, sqlite3_column_bytes16(s, 0), sqlite3_column_bytes(s, 0));
    for (int i = 0; text[i]; i++)
        printf(" %04x", text[i]);
    printf("\n  utf8 of bound: %s (%d bytes)\n", sqlite3_column_text(s, 1), sqlite3_column_bytes(s, 1));
    const unsigned short *name = sqlite3_column_name16(s, 1);
    printf("  name16:");
    for (int i = 0; name[i]; i++)
        printf(" %c", name[i]);
    printf("\n");
    sqlite3_finalize(s);
}

int main(void)
{
    basics();
    remove("api-test.db");
    opening();
    sqlite3 *db = open_memory();
    statements(db);
    changes(db);
    exec_and_tables(db);
    errors(db);
    utf16(db);
    printf("close %d\n", sqlite3_close(db));
    remove("api-test.db");
    remove("junk.db");
    return 0;
}
