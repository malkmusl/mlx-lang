/* mlx-sqlite3 from several threads: four threads, each with its own
   connection to one file, writing in IMMEDIATE transactions and waiting
   on each other's locks with sqlite3_busy_timeout, and readers counting
   while they write; then four threads sharing one connection (which
   takes their calls one at a time). The counts at the end are what
   SQLite's are. */
#include "common.h"
#include <pthread.h>
#include <stdlib.h>

#define THREADS 4
#define ROUNDS 25
#define PER_ROUND 8

static const char *path = "threads-test.db";
static sqlite3 *shared;

static void *writer(void *argument)
{
    int number = (int)(long)argument;
    sqlite3 *db = NULL;
    if (sqlite3_open(path, &db) != SQLITE_OK)
        return (void *)1;
    sqlite3_busy_timeout(db, 10000);
    sqlite3_stmt *insert = NULL;
    sqlite3_prepare_v2(db, "INSERT INTO t(writer, n, data) VALUES (?, ?, ?)", -1, &insert, NULL);
    long failures = 0;
    for (int round = 0; round < ROUNDS; round++) {
        if (sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK) {
            failures++;
            continue;
        }
        for (int k = 0; k < PER_ROUND; k++) {
            sqlite3_bind_int(insert, 1, number);
            sqlite3_bind_int(insert, 2, round * PER_ROUND + k);
            sqlite3_bind_text(insert, 3, "some data for the row", -1, SQLITE_STATIC);
            if (sqlite3_step(insert) != SQLITE_DONE)
                failures++;
            sqlite3_reset(insert);
        }
        if (sqlite3_exec(db, "COMMIT", NULL, NULL, NULL) != SQLITE_OK)
            failures++;
    }
    sqlite3_finalize(insert);
    sqlite3_close(db);
    return (void *)failures;
}

static void *reader(void *argument)
{
    (void)argument;
    sqlite3 *db = NULL;
    if (sqlite3_open(path, &db) != SQLITE_OK)
        return (void *)1;
    sqlite3_busy_timeout(db, 10000);
    long bad = 0;
    for (int i = 0; i < 40; i++) {
        sqlite3_stmt *s = NULL;
        sqlite3_prepare_v2(db, "SELECT count(*), count(DISTINCT writer) FROM t", -1, &s, NULL);
        int rc = sqlite3_step(s);
        /* A count is always whole transactions' rows. */
        if (rc != SQLITE_ROW || sqlite3_column_int(s, 0) % PER_ROUND != 0)
            bad++;
        sqlite3_finalize(s);
    }
    sqlite3_close(db);
    return (void *)bad;
}

static void *sharer(void *argument)
{
    int number = (int)(long)argument;
    long failures = 0;
    for (int i = 0; i < 50; i++) {
        sqlite3_stmt *s = NULL;
        if (sqlite3_prepare_v2(shared, "INSERT INTO s(thread, i) VALUES (?, ?)", -1, &s, NULL) != SQLITE_OK) {
            failures++;
            continue;
        }
        sqlite3_bind_int(s, 1, number);
        sqlite3_bind_int(s, 2, i);
        if (sqlite3_step(s) != SQLITE_DONE)
            failures++;
        sqlite3_finalize(s);
    }
    return (void *)failures;
}

int main(void)
{
    remove(path);
    remove("threads-test.db-journal");
    sqlite3 *db = NULL;
    sqlite3_open(path, &db);
    run(db, "CREATE TABLE t(id INTEGER PRIMARY KEY, writer INTEGER, n INTEGER, data TEXT);");
    run(db, "CREATE INDEX t_writer ON t(writer, n);");
    pthread_t threads[THREADS + 2];
    for (long i = 0; i < THREADS; i++)
        pthread_create(&threads[i], NULL, writer, (void *)i);
    for (long i = 0; i < 2; i++)
        pthread_create(&threads[THREADS + i], NULL, reader, (void *)i);
    long failures = 0;
    for (int i = 0; i < THREADS + 2; i++) {
        void *result = NULL;
        pthread_join(threads[i], &result);
        failures += (long)result;
    }
    printf("failures %ld\n", failures);
    run(db, "SELECT writer, count(*), min(n), max(n) FROM t GROUP BY writer ORDER BY writer;");
    run(db, "SELECT count(*) FROM t; PRAGMA integrity_check;");

    run(db, "CREATE TABLE s(thread INTEGER, i INTEGER);");
    shared = db;
    for (long i = 0; i < THREADS; i++)
        pthread_create(&threads[i], NULL, sharer, (void *)i);
    failures = 0;
    for (int i = 0; i < THREADS; i++) {
        void *result = NULL;
        pthread_join(threads[i], &result);
        failures += (long)result;
    }
    printf("shared connection failures %ld\n", failures);
    run(db, "SELECT thread, count(*), sum(i) FROM s GROUP BY thread ORDER BY thread; PRAGMA integrity_check;");
    sqlite3_close(db);
    remove(path);
    return 0;
}
