/* mlx-sqlite3's SQL: expressions and their NULLs, types, affinities and
   comparisons, collations, LIKE and GLOB, CASE and CAST, the scalar,
   date and aggregate functions, GROUP BY and HAVING, ORDER BY with
   collations and NULLs, LIMIT and OFFSET, DISTINCT, joins, subqueries,
   compound SELECTs and their errors. It prints what SQLite 3.45 prints
   for the same program. */
#include "common.h"

static const char *expressions =
    "SELECT 1 + 2, 7 / 2, 7.0 / 2, 7 % 3, -7 % 3, 7 % -3, 2 * 3.5, 1 - 0.1;"
    "SELECT 9223372036854775807 + 1, -9223372036854775808 - 1, 9223372036854775807 * 2;"
    "SELECT 5 / 0, 5 % 0, 5.0 / 0, 0 / 0.0;"
    "SELECT 1 << 3, 256 >> 4, 6 & 3, 6 | 3, ~5, -(-3), +'12';"
    "SELECT 'a' || 'b' || 1 || 2.5, 'x' || NULL;"
    "SELECT NULL + 1, NULL = NULL, NULL IS NULL, 1 IS NOT NULL, NULL AND 0, NULL OR 1, NULL AND 1, NULL OR 0, NOT NULL;"
    "SELECT 1 = 1.0, '1' = 1, 1 < 'a', x'00' > 'z', 2 BETWEEN 1 AND 3, 5 NOT BETWEEN 1 AND 3;"
    "SELECT 3 IN (1, 2, 3), 4 IN (1, 2, 3), 4 NOT IN (1, 2, NULL), 1 IN (1, NULL), NULL IN (), 1 NOT IN ();"
    "SELECT 1 IS 1, NULL IS NOT 1, 1 IS DISTINCT FROM NULL, NULL IS NOT DISTINCT FROM NULL;"
    "SELECT CASE 2 WHEN 1 THEN 'one' WHEN 2 THEN 'two' ELSE 'many' END, CASE WHEN NULL THEN 1 WHEN 0 THEN 2 END, CASE 5 WHEN 1 THEN 1 END;"
    "SELECT CAST('12abc' AS INTEGER), CAST('3.7' AS INTEGER), CAST(3.7 AS INTEGER), CAST(-3.7 AS INTEGER), CAST('1e3' AS REAL), CAST(12 AS TEXT), CAST(x'414243' AS TEXT), CAST('abc' AS BLOB), CAST(NULL AS TEXT), CAST('  42  ' AS NUMERIC), CAST('4.0' AS NUMERIC);"
    "SELECT typeof(1), typeof(1.5), typeof('a'), typeof(x'01'), typeof(NULL), typeof(1e400), typeof(9223372036854775808);"
    "SELECT 1e15, 1e16, 123456789012345678, 0.1 + 0.2, 1.0, -0.0, 3.0e-7, 1.5e300 * 1e10, 2.5e-310, 100.0 / 3;"
    "SELECT 0x10, 0xffffffffffffffff, 0x7fffffffffffffff;"
    "SELECT 'it''s', \"no such column\";";

static const char *strings =
    "SELECT length('héllo'), length(x'0001'), length(NULL), length(12345), octet_length('héllo');"
    "SELECT upper('abcé'), lower('ABCÉ'), substr('abcdef', 2, 3), substr('abcdef', -2), substr('abcdef', 0, 2), substr('abcdef', 3), substr('héllo', 2, 2), substring('abc', 2);"
    "SELECT replace('aaa', 'a', 'bc'), replace('abc', '', 'x'), trim('  x  '), ltrim('xxyx', 'x'), rtrim('xyxx', 'x'), trim('--a--', '-');"
    "SELECT instr('hello', 'l'), instr('hello', 'z'), instr(x'010203', x'03'), hex('abc'), hex(x'00ff'), hex(12), unhex('414243'), quote('a''b'), quote(1.5), quote(x'0a'), quote(NULL);"
    "SELECT char(72, 105), unicode('é'), zeroblob(3), length(zeroblob(5)), printf('%d|%5s|%-5s|%.2f|%x|%q|%Q|%%|%c', 42, 'ab', 'cd', 3.14159, 255, 'it''s', NULL, 65);"
    "SELECT format('%05d %+d %e %g %g', 42, 5, 12345.678, 0.0001, 1e20), printf('%s', 1.5), printf('%.3s', 'abcdef'), printf('%10.4f|', 3.14159265);"
    "SELECT 'abc' LIKE 'ABC', 'abc' LIKE 'a%', 'abc' LIKE 'a_c', 'a%c' LIKE 'a\\%c' ESCAPE '\\', 'abc' NOT LIKE 'b%', 'ÉA' LIKE 'éa';"
    "SELECT 'abc' GLOB 'a*', 'abc' GLOB 'A*', 'abc' GLOB 'a?c', 'abc' GLOB '[a-c]bc', 'xbc' GLOB '[^a]bc', like('a%', 'ABC'), glob('*c', 'abc');"
    "SELECT coalesce(NULL, NULL, 3), ifnull(NULL, 'x'), nullif(1, 1), nullif(1, 2), iif(1, 'y', 'n'), min(3, 1, 2), max('a', 'b'), min(1, NULL), abs(-5), abs(-5.5), abs(NULL), sign(-3), sign(0.5);"
    "SELECT round(2.5), round(-2.5), round(1.2345, 2), round(1.005, 2), round(12345.6789, -2), round(0.5), round(NULL);"
    "SELECT likely(1), unlikely(0), likelihood(1, 0.5), soundex('Robert'), sqlite_version() > '3';";

static const char *dates =
    "SELECT date('2024-02-29'), date('2024-02-29', '+1 year'), date('2024-01-31', '+1 month'), date('2024-03-31', '-1 month', 'start of month');"
    "SELECT time('12:34:56'), datetime('2024-05-06 07:08:09', '+90 minutes'), datetime(0, 'unixepoch'), datetime(1700000000, 'unixepoch');"
    "SELECT julianday('2000-01-01'), julianday('2000-01-01 12:00'), strftime('%Y-%m-%d %H:%M:%S %j %w %s', '2024-12-31 23:59:59'), strftime('%f', '2024-01-01 00:00:01.25');"
    "SELECT date('2024-06-15', 'weekday 0'), date('2024-06-15', 'start of year', '+45 days'), datetime('2024-01-01', '-1 second'), unixepoch('2024-01-01'), date('junk'), date(NULL);"
    "SELECT datetime(2460000.5), date('2024-01-01T10:00:00Z'), datetime('2024-01-01 10:00:00+02:00'), time('2024-01-01 25:00:00');";

static const char *tables =
    "CREATE TABLE t(a INTEGER, b TEXT, c REAL, d BLOB, e);"
    "INSERT INTO t VALUES (1, 'x', 1.5, x'01', NULL), ('2', 2, '2.5', 'y', 'z'), (3.0, 3.5, 3, 3, 3), ('a', NULL, 'b', NULL, 4.0), (' 5 ', '0005', '1e2', NULL, x'ff');"
    "SELECT a, typeof(a), b, typeof(b), c, typeof(c), d, typeof(d), e, typeof(e) FROM t;"
    "SELECT a FROM t WHERE a = '2';"
    "SELECT b FROM t WHERE b = 2;"
    "SELECT b FROM t WHERE b > 1 ORDER BY b;"
    "SELECT c FROM t WHERE c < '3' ORDER BY c;"
    "SELECT e FROM t WHERE e = 3;"
    "SELECT e FROM t WHERE e = '3';"
    "SELECT rowid, * FROM t WHERE rowid IN (2, 4) ORDER BY rowid DESC;"
    "CREATE TABLE n(k TEXT COLLATE NOCASE, v TEXT);"
    "INSERT INTO n VALUES ('b', 'B'), ('A', 'a'), ('a', 'b'), ('C', 'c'), (NULL, NULL), ('b ', 'x');"
    "SELECT k FROM n ORDER BY k;"
    "SELECT k FROM n ORDER BY k COLLATE BINARY;"
    "SELECT v FROM n ORDER BY v COLLATE NOCASE DESC, k;"
    "SELECT k FROM n WHERE k = 'B';"
    "SELECT v FROM n WHERE v = 'b' COLLATE NOCASE ORDER BY 1;"
    "SELECT k FROM n WHERE k = 'b' COLLATE RTRIM ORDER BY 1;"
    "SELECT DISTINCT k FROM n ORDER BY 1;"
    "SELECT k, v FROM n ORDER BY k NULLS LAST, v DESC NULLS FIRST;"
    "SELECT count(*), count(k), count(DISTINCT k), min(k), max(k), group_concat(v), group_concat(v, ';') FROM n;"
    "SELECT sum(a), total(a), avg(c), min(c), max(c), sum(NULL), total(NULL), avg(NULL) FROM t;"
    "SELECT sum(9223372036854775807), sum(x) FROM (SELECT 9223372036854775807 AS x UNION ALL SELECT 1);"
    "SELECT count(*) FROM t WHERE 0;"
    "SELECT max(a), b FROM t;";

static const char *queries =
    "CREATE TABLE p(id INTEGER PRIMARY KEY, name TEXT, dept INTEGER);"
    "CREATE TABLE d(id INTEGER PRIMARY KEY, title TEXT);"
    "INSERT INTO d VALUES (1, 'eng'), (2, 'ops'), (3, 'empty');"
    "INSERT INTO p(name, dept) VALUES ('ann', 1), ('bob', 1), ('cy', 2), ('dee', NULL), ('eve', 2), ('fay', 1);"
    "SELECT p.name, d.title FROM p JOIN d ON p.dept = d.id ORDER BY p.name;"
    "SELECT p.name, d.title FROM p LEFT JOIN d ON p.dept = d.id ORDER BY p.id DESC;"
    "SELECT d.title, count(p.id) FROM d LEFT JOIN p ON p.dept = d.id GROUP BY d.id ORDER BY 2 DESC, 1;"
    "SELECT dept, count(*) AS n, group_concat(name, ',') FROM p GROUP BY dept HAVING n > 1 ORDER BY dept;"
    "SELECT dept, max(name) FROM p WHERE dept IS NOT NULL GROUP BY 1 ORDER BY 1 DESC;"
    "SELECT name FROM p ORDER BY name LIMIT 2 OFFSET 1;"
    "SELECT name FROM p ORDER BY name LIMIT 2, 3;"
    "SELECT name FROM p ORDER BY name DESC LIMIT -1 OFFSET 4;"
    "SELECT name FROM p WHERE dept IN (SELECT id FROM d WHERE title = 'eng') ORDER BY 1;"
    "SELECT title FROM d WHERE EXISTS (SELECT 1 FROM p WHERE p.dept = d.id) ORDER BY 1;"
    "SELECT title FROM d WHERE NOT EXISTS (SELECT 1 FROM p WHERE p.dept = d.id);"
    "SELECT name, (SELECT title FROM d WHERE d.id = p.dept) FROM p ORDER BY id;"
    "SELECT (SELECT count(*) FROM p), (SELECT max(id) FROM d), (SELECT name FROM p WHERE 0);"
    "SELECT name FROM p WHERE dept = 1 UNION SELECT title FROM d ORDER BY 1;"
    "SELECT dept FROM p UNION ALL SELECT id FROM d ORDER BY 1 DESC LIMIT 4;"
    "SELECT dept FROM p INTERSECT SELECT id FROM d;"
    "SELECT id FROM d EXCEPT SELECT dept FROM p;"
    "SELECT a.name, b.name FROM p a, p b WHERE a.dept = b.dept AND a.id < b.id ORDER BY 1, 2;"
    "SELECT x.n FROM (SELECT name AS n, dept FROM p WHERE dept = 2) AS x ORDER BY x.n DESC;"
    "SELECT * FROM d CROSS JOIN (SELECT 1 AS one) ORDER BY id LIMIT 2;"
    "SELECT p.*, d.title FROM p, d WHERE p.dept = d.id AND d.title LIKE 'o%' ORDER BY p.id;"
    "SELECT count(*) FROM p, d;"
    "SELECT name FROM p WHERE name > 'b' AND name < 'e' ORDER BY name DESC;"
    "SELECT DISTINCT dept FROM p ORDER BY dept;"
    "SELECT id, name FROM p WHERE id = 3;"
    "SELECT id FROM p WHERE id > 2 AND id <= 5;"
    "SELECT id FROM p WHERE id IN (6, 1, 99) ORDER BY id;"
    "SELECT 1 FROM p WHERE id = 'abc';"
    "SELECT name FROM p WHERE rowid = 2.0;"
    "SELECT nosuch FROM p;"
    "SELECT * FROM nosuch;"
    "SELECT name FROM p, d WHERE id = 1;"
    "SELECT count(*) FROM p GROUP BY;"
    "SELECT 1 UNION SELECT 1, 2;"
    "SELECT max(count(*)) FROM p;"
    "SELECT name FROM p WHERE count(*) > 1;";

int main(void)
{
    sqlite3 *db = open_memory();
    run_with(db, expressions, 1);
    run(db, strings);
    run(db, dates);
    run(db, tables);
    run(db, queries);
    /* After a failed statement run stops: the rest one by one. */
    run(db, "SELECT * FROM nosuch;");
    run(db, "SELECT name FROM p, d WHERE id = 1;");
    run(db, "SELECT count(*) FROM p GROUP BY;");
    run(db, "SELECT 1 UNION SELECT 1, 2;");
    run(db, "SELECT max(count(*)) FROM p;");
    run(db, "SELECT name FROM p WHERE count(*) > 1;");
    run(db, "SELECT abs(-9223372036854775808);");
    run(db, "SELECT nosuchfunction(1);");
    run(db, "SELECT substr();");
    run(db, "SELECT 1 +;");
    run(db, "SELEC 1;");
    run(db, "SELECT 1_000;");
    run(db, "SELECT 'unterminated;");
    sqlite3_close(db);
    return 0;
}
