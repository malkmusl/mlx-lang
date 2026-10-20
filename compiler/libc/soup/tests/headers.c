/* Message heads and header values: requests, responses and status lines
   parsed (good, bad, folded, with NULs and junk lines), lists, lists by
   quality, parameter lists (quoted, RFC 5987, duplicated, strict),
   parameters written back; SoupMessageHeaders: appending, replacing,
   removing, one value and the list of values, the order they are walked
   in, Content-Length and Transfer-Encoding, Expect, ranges, Content-Range,
   Content-Type and Content-Disposition. */
#include <libsoup/soup.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_headers(const char *what, SoupMessageHeaders *hdrs)
{
    SoupMessageHeadersIter iter;
    const char *name, *value;
    printf("%s:\n", what);
    soup_message_headers_iter_init(&iter, hdrs);
    while (soup_message_headers_iter_next(&iter, &name, &value))
        printf("  [%s] [%s]\n", name, value);
}

static void print_each(const char *name, const char *value, gpointer data)
{
    printf("  foreach [%s] [%s]\n", name, value);
}

static void print_list(const char *what, GSList *list)
{
    printf("%s:", what);
    for (GSList *l = list; l; l = l->next)
        printf(" [%s]", (char *)l->data);
    printf("\n");
}

/* A table's entries in name order (a hash table's own order is its own). */
static int compare_strings(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

static void print_params(const char *what, GHashTable *params)
{
    if (!params) {
        printf("%s: NULL\n", what);
        return;
    }
    guint count = g_hash_table_size(params);
    const char **keys = g_new0(const char *, count + 1);
    GHashTableIter iter;
    gpointer key, value;
    guint n = 0;
    g_hash_table_iter_init(&iter, params);
    while (g_hash_table_iter_next(&iter, &key, &value))
        keys[n++] = key;
    qsort(keys, n, sizeof(char *), compare_strings);
    printf("%s (%u):", what, count);
    for (guint i = 0; i < n; i++) {
        const char *v = g_hash_table_lookup(params, keys[i]);
        printf(" [%s]=%s%s%s", keys[i], v ? "[" : "", v ? v : "NULL", v ? "]" : "");
    }
    printf("\n");
    g_free(keys);
}

static void parse_response(const char *text)
{
    SoupMessageHeaders *hdrs = soup_message_headers_new(SOUP_MESSAGE_HEADERS_RESPONSE);
    SoupHTTPVersion version = 9;
    guint status = 0;
    char *reason = NULL;
    gboolean ok = soup_headers_parse_response(text, strlen(text), hdrs, &version, &status, &reason);
    printf("response %d: version %d status %u reason [%s]\n", ok, version, status, reason ? reason : "(null)");
    if (ok)
        print_headers("  headers", hdrs);
    g_free(reason);
    soup_message_headers_unref(hdrs);
}

static void parse_request(const char *text, int length)
{
    SoupMessageHeaders *hdrs = soup_message_headers_new(SOUP_MESSAGE_HEADERS_REQUEST);
    SoupHTTPVersion version = 9;
    char *method = NULL, *path = NULL;
    guint status = soup_headers_parse_request(text, length < 0 ? (int)strlen(text) : length, hdrs, &method, &path, &version);
    printf("request %u: [%s] [%s] version %d\n", status, method ? method : "(null)", path ? path : "(null)", version);
    if (status == SOUP_STATUS_OK)
        print_headers("  headers", hdrs);
    g_free(method);
    g_free(path);
    soup_message_headers_unref(hdrs);
}

static void status_line(const char *line)
{
    SoupHTTPVersion version = 9;
    guint status = 0;
    char *reason = NULL;
    gboolean ok = soup_headers_parse_status_line(line, &version, &status, &reason);
    printf("status line %d: %d %u [%s]\n", ok, version, status, reason ? reason : "(null)");
    g_free(reason);
}

int main(void)
{
    /* Message heads. */
    parse_response("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nX-Thing: a\r\n  continued\r\n\tand more  \r\nx-thing: b\r\nContent-Length: 12\r\n");
    parse_response("\r\n\nHTTP/1.0 404 Not Found  \nServer: test\nConnection: X-Gone\nX-Gone: 1\nKeep-Alive: no\n");
    parse_response("HTTP/1.1 301 Moved\r\nbad line\r\n: empty name\r\nna me: spaced\r\nLocation: /there\r\n");
    parse_response("ICY 200 OK\r\nicy-name: radio\r\n");
    parse_response("HTTP/2.0 200 OK\r\n");
    parse_response("HTTP/1.1 20 OK\r\n");
    parse_response("garbage\r\n");
    parse_response("HTTP/1.1 200 OK");
    parse_response("HTTP/1.1 200 OK\r\nA: 1\r\nB: 2");
    parse_response("HTTP/1.1 204 \r\nEmpty:\r\nCR: a\rb\r\n");
    {
        static const char nul[] = "HTTP/1.1 200 OK\r\nA: x\0y\r\nB: z\r\n";
        SoupMessageHeaders *hdrs = soup_message_headers_new(SOUP_MESSAGE_HEADERS_RESPONSE);
        guint status = 0;
        gboolean ok = soup_headers_parse_response(nul, sizeof nul - 1, hdrs, NULL, &status, NULL);
        printf("response with a NUL %d %u\n", ok, status);
        print_headers("  headers", hdrs);
        soup_message_headers_unref(hdrs);
    }
    parse_request("GET /index.html HTTP/1.1\r\nHost: example.com\r\nAccept: */*\r\n", -1);
    parse_request("\r\nPOST   /form   HTTP/1.0\r\nConnection: Keep-Alive, X-Hop\r\nX-Hop: 1\r\nContent-Length: 3\r\n", -1);
    parse_request("GET /x HTTP/1.2\r\n", -1);
    parse_request("GET /x HTTP/2.0\r\n", -1);
    parse_request("GET /x\r\n", -1);
    parse_request("GET /x HTTP/1.1 junk\r\n", -1);
    parse_request("PUT /x HTTP/1.1\r\nExpect: 100-continue\r\n", -1);
    parse_request("PUT /x HTTP/1.1\r\nExpect: something-else\r\n", -1);
    parse_request("", 0);
    status_line("HTTP/1.1 200 OK");
    status_line("HTTP/1.0 500 Internal Server Error\r\nmore");
    status_line("HTTP/1.1  302\tFound \r\n");
    status_line("HTTP/1.1 099 Low");
    status_line("HTTP/1.1 2000 Long");
    status_line("HTTP/1. 200 OK");
    status_line("ICY 200 OK");
    status_line("FTP 200 OK");

    /* Header values. */
    GSList *list = soup_header_parse_list(" ,a, b ,\"c, d\", , e;q=1 ,,");
    print_list("list", list);
    soup_header_free_list(list);
    GSList *unacceptable = NULL;
    list = soup_header_parse_quality_list("text/html;level=1, text/*;q=0.5, image/png;q=0.8, */*;q=0.001, gzip;q=0, x;q=0.80, y;q = 0.5;z=1, w;q=1.0", &unacceptable);
    print_list("quality list", list);
    print_list("unacceptable", unacceptable);
    soup_header_free_list(list);
    soup_header_free_list(unacceptable);
    printf("contains: %d %d %d %d\n", soup_header_contains("close, Upgrade", "upgrade"), soup_header_contains("close,Upgrade", "close"), soup_header_contains("\"close\", other", "close"), soup_header_contains("closed", "close"));

    GHashTable *params = soup_header_parse_param_list("realm=\"testrealm@host.com\", qop=\"auth,auth-int\", nonce=dcd98b, opaque, =bad, Qop=again");
    print_params("param list", params);
    soup_header_free_param_list(params);
    params = soup_header_parse_semi_param_list("attachment; filename=\"a\\\"b.txt\"; filename*=UTF-8''%E2%82%AC%20rates; size = 12 ; title*=iso-8859-1'en'%A3x; bad*=koi8''x; t*=UTF-8''%zz");
    print_params("semi param list", params);
    soup_header_free_param_list(params);
    params = soup_header_parse_param_list_strict("a=1, b=2, A=3");
    print_params("strict with a repeat", params);
    params = soup_header_parse_semi_param_list_strict("a=1; b=\"2\"");
    print_params("strict", params);
    soup_header_free_param_list(params);

    GString *s = g_string_new("x");
    soup_header_g_string_append_param(s, "token", "abc-123");
    g_string_append(s, "; ");
    soup_header_g_string_append_param(s, "spaces", "a b");
    g_string_append(s, "; ");
    soup_header_g_string_append_param(s, "quotes", "say \"hi\" \\ back");
    g_string_append(s, "; ");
    soup_header_g_string_append_param(s, "utf8", "caf\xc3\xa9 & bar");
    g_string_append(s, "; ");
    soup_header_g_string_append_param(s, "latin1", "caf\xe9");
    g_string_append(s, "; ");
    soup_header_g_string_append_param(s, "bare", NULL);
    g_string_append(s, "; ");
    soup_header_g_string_append_param_quoted(s, "q", "abc");
    g_string_append(s, "; ");
    soup_header_g_string_append_param(s, "empty", "");
    printf("params written: %s\n", s->str);
    g_string_free(s, TRUE);

    /* SoupMessageHeaders. */
    SoupMessageHeaders *hdrs = soup_message_headers_new(SOUP_MESSAGE_HEADERS_REQUEST);
    soup_message_headers_append(hdrs, "X-Custom", "1");
    soup_message_headers_append(hdrs, "accept", "text/html");
    soup_message_headers_append(hdrs, "x-custom", "2");
    soup_message_headers_append(hdrs, "ACCEPT", "image/png");
    soup_message_headers_append(hdrs, "Cache-Control", "no-cache");
    soup_message_headers_append(hdrs, "Other", "o");
    print_headers("headers", hdrs);
    soup_message_headers_foreach(hdrs, print_each, NULL);
    printf("get_one: [%s] [%s] [%s]\n", soup_message_headers_get_one(hdrs, "x-custom"), soup_message_headers_get_one(hdrs, "Accept"), soup_message_headers_get_one(hdrs, "missing") ? "?" : "NULL");
    printf("get_list: [%s] [%s] [%s]\n", soup_message_headers_get_list(hdrs, "X-CUSTOM"), soup_message_headers_get_list(hdrs, "accept"), soup_message_headers_get_list(hdrs, "other"));
    printf("contains: %d %d equals: %d %d\n", soup_message_headers_header_contains(hdrs, "Accept", "IMAGE/PNG"), soup_message_headers_header_contains(hdrs, "Accept", "image"), soup_message_headers_header_equals(hdrs, "other", "O"), soup_message_headers_header_equals(hdrs, "accept", "text/html"));
    soup_message_headers_replace(hdrs, "Accept", "*/*");
    soup_message_headers_remove(hdrs, "X-CUSTOM");
    soup_message_headers_append(hdrs, "X-Custom", "3");
    printf("after replace and remove: list [%s] [%s]\n", soup_message_headers_get_list(hdrs, "accept"), soup_message_headers_get_list(hdrs, "x-custom"));
    print_headers("headers", hdrs);
    printf("type %d encoding %d length %ld expectations %d\n", soup_message_headers_get_headers_type(hdrs), soup_message_headers_get_encoding(hdrs), (long)soup_message_headers_get_content_length(hdrs), soup_message_headers_get_expectations(hdrs));
    soup_message_headers_set_content_length(hdrs, 1234);
    printf("content length: %ld encoding %d [%s]\n", (long)soup_message_headers_get_content_length(hdrs), soup_message_headers_get_encoding(hdrs), soup_message_headers_get_one(hdrs, "Content-Length"));
    soup_message_headers_set_encoding(hdrs, SOUP_ENCODING_CHUNKED);
    printf("chunked: encoding %d length %ld [%s] [%s]\n", soup_message_headers_get_encoding(hdrs), (long)soup_message_headers_get_content_length(hdrs), soup_message_headers_get_one(hdrs, "Transfer-Encoding"), soup_message_headers_get_one(hdrs, "Content-Length") ? "?" : "NULL");
    soup_message_headers_append(hdrs, "Content-Length", "99");
    printf("content length while chunked: encoding %d\n", soup_message_headers_get_encoding(hdrs));
    soup_message_headers_set_encoding(hdrs, SOUP_ENCODING_NONE);
    printf("none: encoding %d\n", soup_message_headers_get_encoding(hdrs));
    soup_message_headers_replace(hdrs, "Content-Length", "12abc");
    printf("bad length: encoding %d length %ld\n", soup_message_headers_get_encoding(hdrs), (long)soup_message_headers_get_content_length(hdrs));
    soup_message_headers_replace(hdrs, "Transfer-Encoding", "gzip");
    printf("unknown transfer encoding: %d\n", soup_message_headers_get_encoding(hdrs));
    soup_message_headers_set_expectations(hdrs, SOUP_EXPECTATION_CONTINUE);
    printf("expect: %d [%s]\n", soup_message_headers_get_expectations(hdrs), soup_message_headers_get_one(hdrs, "Expect"));
    soup_message_headers_clear(hdrs);
    printf("cleared: encoding %d expectations %d\n", soup_message_headers_get_encoding(hdrs), soup_message_headers_get_expectations(hdrs));
    soup_message_headers_unref(hdrs);

    hdrs = soup_message_headers_new(SOUP_MESSAGE_HEADERS_RESPONSE);
    printf("response encoding: %d\n", soup_message_headers_get_encoding(hdrs));
    soup_message_headers_replace(hdrs, "Transfer-Encoding", "chunked");
    printf("response chunked: %d\n", soup_message_headers_get_encoding(hdrs));
    soup_message_headers_unref(hdrs);

    /* Ranges. */
    hdrs = soup_message_headers_new(SOUP_MESSAGE_HEADERS_REQUEST);
    const char *ranges[] = { "bytes=0-99", "bytes=500-", "bytes=-100", "bytes = 0-9, 20-29,5-24", "bytes=9-0", "bytes=a-b", "items=0-1", "bytes=0-9,2000-3000", "bytes=1000-2000", NULL };
    for (int i = 0; ranges[i]; i++) {
        SoupRange *r = NULL;
        int n = -1;
        soup_message_headers_replace(hdrs, "Range", ranges[i]);
        gboolean ok = soup_message_headers_get_ranges(hdrs, 1000, &r, &n);
        printf("ranges [%s]: %d", ranges[i], ok);
        if (ok) {
            for (int j = 0; j < n; j++)
                printf(" %ld-%ld", (long)r[j].start, (long)r[j].end);
            soup_message_headers_free_ranges(hdrs, r);
        }
        printf("\n");
    }
    SoupRange set[3] = { { 0, 10 }, { 20, -1 }, { -30, -1 } };
    soup_message_headers_set_ranges(hdrs, set, 3);
    printf("set ranges: [%s]\n", soup_message_headers_get_one(hdrs, "Range"));
    soup_message_headers_set_range(hdrs, 5, 6);
    printf("set range: [%s]\n", soup_message_headers_get_one(hdrs, "Range"));
    soup_message_headers_set_content_range(hdrs, 0, 99, 1000);
    goffset start = 0, end = 0, total = 0;
    gboolean ok = soup_message_headers_get_content_range(hdrs, &start, &end, &total);
    printf("content range [%s]: %d %ld %ld %ld\n", soup_message_headers_get_one(hdrs, "Content-Range"), ok, (long)start, (long)end, (long)total);
    soup_message_headers_set_content_range(hdrs, 10, 20, -1);
    ok = soup_message_headers_get_content_range(hdrs, &start, &end, &total);
    printf("content range [%s]: %d %ld %ld %ld\n", soup_message_headers_get_one(hdrs, "Content-Range"), ok, (long)start, (long)end, (long)total);
    soup_message_headers_replace(hdrs, "Content-Range", "bytes 1-2/3x");
    printf("bad content range: %d\n", soup_message_headers_get_content_range(hdrs, &start, &end, &total));

    /* Content-Type and Content-Disposition. */
    const char *types[] = { "text/html; charset=UTF-8", "text/html ; charset=\"utf-8\" ; boundary=x", "texthtml", "text/ html", "text/html/x", "application/json", NULL };
    for (int i = 0; types[i]; i++) {
        GHashTable *p = NULL;
        soup_message_headers_replace(hdrs, "Content-Type", types[i]);
        const char *type = soup_message_headers_get_content_type(hdrs, &p);
        printf("content type [%s]: [%s] ", types[i], type ? type : "NULL");
        print_params("params", p);
        if (p)
            g_hash_table_destroy(p);
    }
    GHashTable *ct = g_hash_table_new(g_str_hash, g_str_equal);
    g_hash_table_insert(ct, "charset", "utf-8");
    soup_message_headers_set_content_type(hdrs, "text/plain", ct);
    printf("set content type: [%s] [%s]\n", soup_message_headers_get_one(hdrs, "Content-Type"), soup_message_headers_get_content_type(hdrs, NULL));
    g_hash_table_destroy(ct);
    const char *dispositions[] = { "attachment; filename=\"/etc/passwd\"", "inline", "filename=only.txt; size=3", "form-data; name=\"f\"; filename=\"dir/x.png\"", NULL };
    for (int i = 0; dispositions[i]; i++) {
        GHashTable *p = NULL;
        char *disposition = NULL;
        soup_message_headers_replace(hdrs, "Content-Disposition", dispositions[i]);
        gboolean got = soup_message_headers_get_content_disposition(hdrs, &disposition, &p);
        printf("disposition [%s]: %d [%s] ", dispositions[i], got, disposition ? disposition : "NULL");
        print_params("params", p);
        g_free(disposition);
        if (p)
            g_hash_table_destroy(p);
    }
    GHashTable *dp = g_hash_table_new(g_str_hash, g_str_equal);
    g_hash_table_insert(dp, "filename", "na\"me.txt");
    soup_message_headers_set_content_disposition(hdrs, "attachment", dp);
    printf("set disposition: [%s]\n", soup_message_headers_get_one(hdrs, "Content-Disposition"));
    g_hash_table_destroy(dp);
    soup_message_headers_unref(hdrs);
    return 0;
}
