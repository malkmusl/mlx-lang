/* Reason phrases; HTTP dates read in every form servers send (and
   refused when they are not dates) and written as HTTP and cookies write
   them; forms encoded and decoded; data: URIs decoded; URIs compared and
   copied with components replaced; the version. */
#include <libsoup/soup.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void show_date(const char *text)
{
    GDateTime *d = soup_date_time_new_from_http_string(text);
    if (!d) {
        printf("date [%s]: NULL\n", text);
        return;
    }
    char *http = soup_date_time_to_string(d, SOUP_DATE_HTTP);
    char *cookie = soup_date_time_to_string(d, SOUP_DATE_COOKIE);
    printf("date [%s]: %ld offset %ld [%s] [%s]\n", text, (long)g_date_time_to_unix(d), (long)(g_date_time_get_utc_offset(d) / G_USEC_PER_SEC), http, cookie);
    g_free(http);
    g_free(cookie);
    g_date_time_unref(d);
}

static int compare_strings(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

static void show_table(const char *what, GHashTable *table)
{
    guint count = g_hash_table_size(table);
    const char **keys = g_new0(const char *, count + 1);
    GHashTableIter iter;
    gpointer key, value;
    guint n = 0;
    g_hash_table_iter_init(&iter, table);
    while (g_hash_table_iter_next(&iter, &key, &value))
        keys[n++] = key;
    qsort(keys, n, sizeof(char *), compare_strings);
    printf("%s (%u):", what, count);
    for (guint i = 0; i < n; i++)
        printf(" [%s]=[%s]", keys[i], (const char *)g_hash_table_lookup(table, keys[i]));
    printf("\n");
    g_free(keys);
}

static void show_data_uri(const char *uri)
{
    char *type = NULL;
    GBytes *bytes = soup_uri_decode_data_uri(uri, &type);
    if (!bytes) {
        printf("data [%s]: NULL type [%s]\n", uri, type ? type : "NULL");
        g_free(type);
        return;
    }
    gsize size = 0;
    const char *data = g_bytes_get_data(bytes, &size);
    printf("data [%s]: [%s] %lu [%.*s]\n", uri, type ? type : "NULL", (unsigned long)size, (int)size, data ? data : "");
    g_free(type);
    g_bytes_unref(bytes);
}

static void show_uri(const char *what, GUri *uri)
{
    char *text = g_uri_to_string(uri);
    printf("%s: [%s] port %d\n", what, text, g_uri_get_port(uri));
    g_free(text);
}

int main(void)
{
    guint codes[] = { 100, 200, 204, 206, 301, 304, 307, 308, 401, 404, 407, 416, 418, 421, 500, 505, 510, 999, 0 };
    for (int i = 0; codes[i]; i++)
        printf("phrase %u: %s\n", codes[i], soup_status_get_phrase(codes[i]));
    printf("version %u.%u.%u %d %d %d\n", soup_get_major_version(), soup_get_minor_version(), soup_get_micro_version(), soup_check_version(3, 4, 4), soup_check_version(3, 5, 0), SOUP_CHECK_VERSION(3, 0, 0));

    const char *dates[] = {
        "Sun, 06 Nov 1994 08:49:37 GMT", "Sunday, 06-Nov-94 08:49:37 GMT", "Sun Nov  6 08:49:37 1994",
        "Sun, 06-Nov-1994 08:49:37 GMT", "06 Nov 1994 08:49:37", "Sun, 06 Nov 1994 08:49:37 +0130",
        "Sun, 06 Nov 1994 08:49:37 -01:00", "Sun, 06 Nov 1994 08:49:37 -0000", "Sun, 06 Nov 1994 08:49:37 PST",
        "Sun, 06 Nov 1994 08:49:37 EDT", "Sun, 06 Nov 1994 08:49:37 CST", "Sun, 06 Nov 1994 08:49:37 Z",
        "Sun, 06 Nov 1994 08:49:37 UTC", "Sun, 06 Nov 1994 08:49:37 XYZ", "  Wed, 31 Dec 1969 23:59:59 GMT",
        "Thu, 01 Jan 70 00:00:00 GMT", "Fri, 31 Dec 2069 12:00:00 GMT", "Sat, 01 Jan 69 00:00:00 GMT",
        "1994-11-06T08:49:37Z", "784111777", "31 Apr 2020 10:00:00 GMT", "29 Feb 2023 10:00:00 GMT",
        "29 Feb 2024 10:00:00 GMT", "Sun, 06 Nov 1994 25:49:37 GMT", "Sun, 06 Nov 1994 08:60:37 GMT",
        "Sun, 32 Nov 1994 08:49:37 GMT", "Sun, 06 Foo 1994 08:49:37 GMT", "Sun, 06 Nov 99999 08:49:37 GMT",
        "Sun, 06 Nov 1994", "Sun, 06 Nov 1994 08:49", "Sun, 06 Nov 1994 08:49:37 +99999",
        "Sun, 06 Nov 1994 08:49:37 +100:00", "06-nov-1994 08:49:37 gmt", "Sun, 6 Nov 1994 8:9:7 GMT",
        "Tue, 15-Nov-2044 08:12:31 GMT", "", "garbage", NULL };
    for (int i = 0; dates[i]; i++)
        show_date(dates[i]);
    GDateTime *epoch = g_date_time_new_from_unix_utc(1700000000);
    char *s = soup_date_time_to_string(epoch, SOUP_DATE_HTTP);
    printf("written: %s\n", s);
    g_free(s);
    g_date_time_unref(epoch);
    GTimeZone *tz = g_time_zone_new_offset(-5 * 3600);
    GDateTime *local = g_date_time_new(tz, 2001, 2, 3, 23, 4, 5);
    s = soup_date_time_to_string(local, SOUP_DATE_COOKIE);
    printf("written from -05: %s\n", s);
    g_free(s);
    g_date_time_unref(local);
    g_time_zone_unref(tz);

    /* Forms. */
    char *encoded = soup_form_encode("name", "John Doe", "email", "j@x.org", "sym", "a+b&c=d/é~", "empty", "", NULL);
    printf("form: %s\n", encoded);
    GHashTable *decoded = soup_form_decode(encoded);
    show_table("decoded", decoded);
    g_hash_table_destroy(decoded);
    g_free(encoded);
    decoded = soup_form_decode("a=1&b=%zz&c&=nameless&d=x%2By+z&a=2&e=%41%4a%4A%");
    show_table("decoded odd form", decoded);
    g_hash_table_destroy(decoded);
    encoded = soup_form_encode("only", NULL);
    printf("form with no value: [%s]\n", encoded);
    g_free(encoded);
    GHashTable *single = g_hash_table_new(g_str_hash, g_str_equal);
    g_hash_table_insert(single, "k e y", "v&l");
    encoded = soup_form_encode_hash(single);
    printf("form of a table: %s\n", encoded);
    g_free(encoded);
    g_hash_table_destroy(single);

    /* data: URIs. */
    show_data_uri("data:,Hello%2C%20World!");
    show_data_uri("data:text/plain;base64,SGVsbG8sIFdvcmxkIQ==");
    show_data_uri("data:text/html;charset=utf-8,%3Ch1%3Ehi%3C%2Fh1%3E");
    show_data_uri("data:;base64,QQ==");
    show_data_uri("data:image/png;base64,");
    show_data_uri("data:text/plain;BASE64,eA");
    show_data_uri("data:");
    show_data_uri("http://example.com/");
    show_data_uri("data://host/x");

    /* URIs. */
    GUri *a = g_uri_parse("http://example.com/path?q=1", SOUP_HTTP_URI_FLAGS, NULL);
    GUri *b = g_uri_parse("HTTP://EXAMPLE.com:80/path?q=1", SOUP_HTTP_URI_FLAGS, NULL);
    GUri *c = g_uri_parse("http://example.com", SOUP_HTTP_URI_FLAGS, NULL);
    GUri *d = g_uri_parse("http://example.com/", SOUP_HTTP_URI_FLAGS, NULL);
    GUri *e = g_uri_parse("http://example.com/path?q=2", SOUP_HTTP_URI_FLAGS, NULL);
    GUri *f = g_uri_parse("http://example.com/path?q=1", G_URI_FLAGS_NONE, NULL);
    printf("equal: %d %d %d %d\n", soup_uri_equal(a, b), soup_uri_equal(c, d), soup_uri_equal(a, e), soup_uri_equal(a, f));
    GUri *copy = soup_uri_copy(a, SOUP_URI_PATH, "/other", SOUP_URI_QUERY, NULL, SOUP_URI_PORT, 8080, SOUP_URI_NONE);
    show_uri("copy", copy);
    g_uri_unref(copy);
    copy = soup_uri_copy(a, SOUP_URI_SCHEME, "https", SOUP_URI_USER, "me", SOUP_URI_PASSWORD, "pw", SOUP_URI_NONE);
    show_uri("copy", copy);
    g_uri_unref(copy);
    copy = soup_uri_copy(b, SOUP_URI_NONE);
    show_uri("copy keeping all", copy);
    g_uri_unref(copy);
    g_uri_unref(a);
    g_uri_unref(b);
    g_uri_unref(c);
    g_uri_unref(d);
    g_uri_unref(e);
    g_uri_unref(f);
    return 0;
}
