/* SoupSession against a small HTTP/1.1 server the test runs in a child
 * process (plain POSIX sockets, so the same server answers both builds):
 * bodies by Content-Length, chunked and to the connection's end, gzip and
 * deflate, redirects, Basic and Digest authentication, request bodies,
 * HEAD, the headers a request carries, connection reuse, content
 * sniffing, synchronous and asynchronous sends and the message's signals
 * in the order they come. Each line printed is compared with what the
 * system's libsoup prints. */
#include <libsoup/soup.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

static const char gzip_body[] = "\x1f\x8b\x08\x00\x00\x00\x00\x00\x02\xff\xf3\x48\xcd\xc9\xc9\xd7\x51\x48\xce\xcf\x2d\x28\x4a\x2d\x2e\x4e\x4d\x51\x28\xcf\x2f\xca\x49\x51\x54\xf0\x18\x5a\x12\x00\x6e\x83\xac\xb3\xc8\x00\x00\x00";
static const char zlib_body[] = "\x78\x9c\xf3\x48\xcd\xc9\xc9\xd7\x51\x48\xce\xcf\x2d\x28\x4a\x2d\x2e\x4e\x4d\x51\x28\xcf\x2f\xca\x49\x51\x54\xf0\x18\x5a\x12\x00\x55\xac\x47\xf1";
static const char raw_body[] = "\xf3\x48\xcd\xc9\xc9\xd7\x51\x48\xce\xcf\x2d\x28\x4a\x2d\x2e\x4e\x4d\x51\x28\xcf\x2f\xca\x49\x51\x54\xf0\x18\x5a\x12\x00";

/* ------------------------------------------------------------------ */
/* The server */

static int connections;
static int port;
static GString *trace;

static void put (int fd, const char *data, size_t length)
{
    while (length > 0) {
        ssize_t n = write (fd, data, length);
        if (n <= 0) return;
        data += n;
        length -= (size_t) n;
    }
}

static void puts_fd (int fd, const char *text) { put (fd, text, strlen (text)); }

static void respond (int fd, const char *status, const char *extra, const char *body, size_t length, int keep)
{
    char head[1024];
    snprintf (head, sizeof head, "HTTP/1.1 %s\r\n%sContent-Length: %zu\r\n%s\r\n", status, extra, length, keep ? "" : "Connection: close\r\n");
    puts_fd (fd, head);
    put (fd, body, length);
}

/* The request's head (up to its blank line) and body, read off `fd`
 * with what was read past the last request in `buf`. */
static char buf[65536];
static size_t have;

static int read_more (int fd)
{
    ssize_t n;
    if (have == sizeof buf) return 0;
    n = read (fd, buf + have, sizeof buf - have);
    if (n <= 0) return 0;
    have += (size_t) n;
    return 1;
}

static char *find_head_end (void)
{
    size_t i;
    for (i = 0; i + 3 < have; i++)
        if (memcmp (buf + i, "\r\n\r\n", 4) == 0) return buf + i + 4;
    return NULL;
}

static const char *header_value (const char *head, const char *name, char *out, size_t size)
{
    const char *line = strstr (head, "\r\n");
    size_t name_len = strlen (name);
    while (line && line[2] != '\r') {
        line += 2;
        if (strncasecmp (line, name, name_len) == 0 && line[name_len] == ':') {
            const char *v = line + name_len + 1;
            const char *end = strstr (v, "\r\n");
            size_t n;
            while (*v == ' ') v++;
            n = (size_t) (end - v);
            if (n >= size) n = size - 1;
            memcpy (out, v, n);
            out[n] = 0;
            return out;
        }
        line = strstr (line, "\r\n");
    }
    return NULL;
}

static void serve (int fd)
{
    int keep = 1;
    have = 0;
    while (keep) {
        char *end, head[8192], method[16], path[512], value[1024], body[8192];
        size_t head_len, body_len = 0;
        while (!(end = find_head_end ()))
            if (!read_more (fd)) { close (fd); return; }
        head_len = (size_t) (end - buf);
        memcpy (head, buf, head_len);
        head[head_len] = 0;
        memmove (buf, end, have - head_len);
        have -= head_len;
        sscanf (head, "%15s %511s", method, path);
        if (header_value (head, "Content-Length", value, sizeof value)) {
            body_len = (size_t) atol (value);
            while (have < body_len) if (!read_more (fd)) { close (fd); return; }
            memcpy (body, buf, body_len);
            memmove (buf, buf + body_len, have - body_len);
            have -= body_len;
        } else if (header_value (head, "Transfer-Encoding", value, sizeof value)) {
            /* Chunked: sizes, data, the last chunk, the empty trailer. */
            for (;;) {
                char *lf;
                size_t size;
                while (!(lf = memchr (buf, '\n', have))) if (!read_more (fd)) { close (fd); return; }
                size = strtoul (buf, NULL, 16);
                memmove (buf, lf + 1, have - (size_t) (lf + 1 - buf));
                have -= (size_t) (lf + 1 - buf);
                if (size == 0) {
                    while (have < 2) if (!read_more (fd)) { close (fd); return; }
                    memmove (buf, buf + 2, have - 2);
                    have -= 2;
                    break;
                }
                while (have < size + 2) if (!read_more (fd)) { close (fd); return; }
                memcpy (body + body_len, buf, size);
                body_len += size;
                memmove (buf, buf + size + 2, have - size - 2);
                have -= size + 2;
            }
        }
        body[body_len] = 0;
        if (header_value (head, "Connection", value, sizeof value) && strcasecmp (value, "close") == 0) keep = 0;

        if (strcmp (path, "/plain") == 0) {
            if (strcmp (method, "HEAD") == 0) {
                puts_fd (fd, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 12\r\n\r\n");
            } else {
                respond (fd, "200 OK", "Content-Type: text/plain\r\n", "Hello, soup\n", 12, keep);
            }
        } else if (strcmp (path, "/chunked") == 0) {
            puts_fd (fd, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHello\r\n7;ext=1\r\n, chunk\r\n2\r\ns\n\r\n0\r\nX-Trailer: yes\r\n\r\n");
        } else if (strcmp (path, "/eof") == 0) {
            puts_fd (fd, "HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n\r\nto the end of the connection\n");
            keep = 0;
        } else if (strcmp (path, "/gzip") == 0) {
            respond (fd, "200 OK", "Content-Type: text/plain\r\nContent-Encoding: gzip\r\n", gzip_body, sizeof gzip_body - 1, keep);
        } else if (strcmp (path, "/deflate") == 0) {
            respond (fd, "200 OK", "Content-Type: text/plain\r\nContent-Encoding: deflate\r\n", zlib_body, sizeof zlib_body - 1, keep);
        } else if (strcmp (path, "/rawdeflate") == 0) {
            respond (fd, "200 OK", "Content-Type: text/plain\r\nContent-Encoding: deflate\r\n", raw_body, sizeof raw_body - 1, keep);
        } else if (strcmp (path, "/notgzip") == 0) {
            respond (fd, "200 OK", "Content-Type: text/plain\r\nContent-Encoding: gzip\r\n", "not compressed at all", 21, keep);
        } else if (strcmp (path, "/redirect") == 0) {
            respond (fd, "302 Found", "Location: /plain\r\n", "moved", 5, keep);
        } else if (strcmp (path, "/redirect-post") == 0) {
            respond (fd, "303 See Other", "Location: /echo\r\n", "", 0, keep);
        } else if (strcmp (path, "/redirect-loop") == 0) {
            respond (fd, "302 Found", "Location: /redirect-loop\r\n", "", 0, keep);
        } else if (strcmp (path, "/redirect-none") == 0) {
            respond (fd, "302 Found", "", "no location", 11, keep);
        } else if (strcmp (path, "/basic") == 0) {
            if (header_value (head, "Authorization", value, sizeof value) && strcmp (value, "Basic dXNlcjpwYXNzd29yZA==") == 0)
                respond (fd, "200 OK", "Content-Type: text/plain\r\n", "authorized\n", 11, keep);
            else
                respond (fd, "401 Unauthorized", "WWW-Authenticate: Basic realm=\"test realm\"\r\n", "who are you", 11, keep);
        } else if (strcmp (path, "/digest") == 0) {
            if (header_value (head, "Authorization", value, sizeof value) && strncmp (value, "Digest ", 7) == 0)
                respond (fd, "200 OK", "Content-Type: text/plain\r\n", value, strlen (value), keep);
            else
                respond (fd, "401 Unauthorized", "WWW-Authenticate: Digest realm=\"digest realm\", nonce=\"abc123\", qop=\"auth\", opaque=\"xyz\"\r\n", "", 0, keep);
        } else if (strcmp (path, "/echo") == 0) {
            char out[16384];
            int n = snprintf (out, sizeof out, "%s %zu:%s", method, body_len, body);
            respond (fd, "200 OK", "Content-Type: text/plain\r\n", out, (size_t) n, keep);
        } else if (strncmp (path, "/headers", 8) == 0) {
            respond (fd, "200 OK", "Content-Type: text/plain\r\n", head, head_len, keep);
        } else if (strcmp (path, "/sniff") == 0) {
            respond (fd, "200 OK", "", "<html><body>sniff me</body></html>", 34, keep);
        } else if (strcmp (path, "/stats") == 0) {
            char out[64];
            int n = snprintf (out, sizeof out, "%d", connections);
            respond (fd, "200 OK", "", out, (size_t) n, keep);
        } else if (strcmp (path, "/continue") == 0) {
            respond (fd, "200 OK", "", body, body_len, keep);
        } else if (strcmp (path, "/quit") == 0) {
            respond (fd, "200 OK", "", "bye", 3, 0);
            close (fd);
            kill (getppid (), SIGTERM);
            _exit (0);
        } else {
            respond (fd, "404 Not Found", "Content-Type: text/plain\r\n", "nothing here", 12, keep);
        }
    }
    close (fd);
}

static int start_server (pid_t *child)
{
    struct sockaddr_in addr;
    socklen_t length = sizeof addr;
    int fd = socket (AF_INET, SOCK_STREAM, 0), one = 1;
    setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    memset (&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
    bind (fd, (struct sockaddr *) &addr, sizeof addr);
    listen (fd, 16);
    getsockname (fd, (struct sockaddr *) &addr, &length);
    *child = fork ();
    if (*child == 0) {
        /* A process for each connection; the one asked to quit ends the
         * server. */
        signal (SIGCHLD, SIG_IGN);
        alarm (120);
        for (;;) {
            int c = accept (fd, NULL, NULL);
            if (c < 0) _exit (1);
            connections++;
            if (fork () == 0) {
                close (fd);
                serve (c);
                _exit (0);
            }
            close (c);
        }
    }
    close (fd);
    return ntohs (addr.sin_port);
}

/* ------------------------------------------------------------------ */
/* The client */


static char *url (const char *path) { return g_strdup_printf ("http://127.0.0.1:%d%s", port, path); }

static void on_signal (SoupMessage *msg, gpointer name) { g_string_append_printf (trace, " %s", (const char *) name); }
static void on_data (SoupMessage *msg, guint count, gpointer name) { g_string_append_printf (trace, " %s", (const char *) name); }

static void on_sniffed (SoupMessage *msg, const char *type, GHashTable *params, gpointer data)
{
    g_string_append_printf (trace, " content-sniffed(%s)", type);
}

static void watch (SoupMessage *msg)
{
    g_signal_connect (msg, "starting", G_CALLBACK (on_signal), "starting");
    g_signal_connect (msg, "wrote-headers", G_CALLBACK (on_signal), "wrote-headers");
    g_signal_connect (msg, "wrote-body", G_CALLBACK (on_signal), "wrote-body");
    g_signal_connect (msg, "got-informational", G_CALLBACK (on_signal), "got-informational");
    g_signal_connect (msg, "got-headers", G_CALLBACK (on_signal), "got-headers");
    g_signal_connect (msg, "got-body", G_CALLBACK (on_signal), "got-body");
    g_signal_connect (msg, "restarted", G_CALLBACK (on_signal), "restarted");
    g_signal_connect (msg, "finished", G_CALLBACK (on_signal), "finished");
    g_signal_connect (msg, "content-sniffed", G_CALLBACK (on_sniffed), NULL);
}

static void show_response (const char *what, SoupMessage *msg, GBytes *body, GError *error)
{
    printf ("%s: ", what);
    if (error) {
        printf ("error %s %d: %s\n", g_quark_to_string (error->domain), error->code, error->message);
        return;
    }
    printf ("%u %s (connection %lu)", soup_message_get_status (msg), soup_message_get_reason_phrase (msg), (unsigned long) soup_message_get_connection_id (msg));
    if (body) {
        gsize size;
        const char *data = g_bytes_get_data (body, &size);
        char *text = g_strndup (data ? data : "", size), *at, needle[16];
        /* The server's port varies from run to run. */
        snprintf (needle, sizeof needle, ":%d", port);
        while ((at = strstr (text, needle))) {
            memcpy (at, ":PORT", 5);
            memmove (at + 5, at + strlen (needle), strlen (at + strlen (needle)) + 1);
        }
        printf (" [%s]", text);
        g_free (text);
    }
    printf ("\n");
}

static void sync_get (SoupSession *session, const char *path)
{
    char *u = url (path);
    SoupMessage *msg = soup_message_new ("GET", u);
    GError *error = NULL;
    GBytes *body;
    g_string_truncate (trace, 0);
    watch (msg);
    body = soup_session_send_and_read (session, msg, NULL, &error);
    show_response (path, msg, body, error);
    printf ("  signals:%s\n", trace->str);
    if (!error) {
        const char *ct = soup_message_headers_get_content_type (soup_message_get_response_headers (msg), NULL);
        char *final = g_uri_to_string_partial (soup_message_get_uri (msg), G_URI_HIDE_NONE);
        printf ("  content-type %s, encoding %d, connection %lu, final %s\n", ct ? ct : "(none)",
                soup_message_headers_get_encoding (soup_message_get_response_headers (msg)), (unsigned long) soup_message_get_connection_id (msg),
                final + strlen (u) - strlen (path));
        g_free (final);
    }
    g_clear_error (&error);
    if (body) g_bytes_unref (body);
    g_object_unref (msg);
    g_free (u);
}

static gboolean on_authenticate (SoupMessage *msg, SoupAuth *auth, gboolean retrying, gpointer data)
{
    printf ("  authenticate %s realm \"%s\" retrying %d proxy %d\n", soup_auth_get_scheme_name (auth), soup_auth_get_realm (auth), retrying, soup_auth_is_for_proxy (auth));
    if (!retrying) soup_auth_authenticate (auth, "user", "password");
    return FALSE;
}

static void auth_get (SoupSession *session, const char *path)
{
    char *u = url (path);
    SoupMessage *msg = soup_message_new ("GET", u);
    GError *error = NULL;
    GBytes *body;
    g_signal_connect (msg, "authenticate", G_CALLBACK (on_authenticate), NULL);
    body = soup_session_send_and_read (session, msg, NULL, &error);
    if (body && strcmp (path, "/digest") == 0) {
        /* The response varies with the client nonce: shown without it. */
        gsize size;
        const char *data = g_bytes_get_data (body, &size);
        char *text = g_strndup (data ? data : "", size), *cnonce = strstr (text, "cnonce=\"");
        if (cnonce) { char *end = strchr (cnonce + 8, '"'); memmove (cnonce + 8, end, strlen (end) + 1); }
        cnonce = strstr (text, "response=\"");
        if (cnonce) { char *end = strchr (cnonce + 10, '"'); memmove (cnonce + 10, end, strlen (end) + 1); }
        g_bytes_unref (body);
        body = g_bytes_new_take (text, strlen (text));
    }
    show_response (path, msg, body, error);
    g_clear_error (&error);
    if (body) g_bytes_unref (body);
    g_object_unref (msg);
    g_free (u);
}

static void post (SoupSession *session, const char *path, const char *data, gboolean chunked)
{
    char *u = url (path);
    SoupMessage *msg = soup_message_new ("POST", u);
    GError *error = NULL;
    GBytes *body;
    if (chunked) {
        GInputStream *stream = g_memory_input_stream_new_from_data (g_strdup (data), (gssize) strlen (data), g_free);
        soup_message_set_request_body (msg, "text/plain", stream, -1);
        g_object_unref (stream);
    } else {
        GBytes *bytes = g_bytes_new (data, strlen (data));
        soup_message_set_request_body_from_bytes (msg, "text/plain", bytes);
        g_bytes_unref (bytes);
    }
    g_string_truncate (trace, 0);
    watch (msg);
    body = soup_session_send_and_read (session, msg, NULL, &error);
    show_response (path, msg, body, error);
    printf ("  signals:%s, method now %s\n", trace->str, soup_message_get_method (msg));
    g_clear_error (&error);
    if (body) g_bytes_unref (body);
    g_object_unref (msg);
    g_free (u);
}

/* The asynchronous send: the stream read in small pieces as they come. */
typedef struct {
    GMainLoop *loop;
    SoupMessage *msg;
    GInputStream *stream;
    GString *body;
    char piece[7];
} AsyncRead;

static void read_ready (GObject *source, GAsyncResult *result, gpointer data)
{
    AsyncRead *ar = data;
    GError *error = NULL;
    gssize n = g_input_stream_read_finish (G_INPUT_STREAM (source), result, &error);
    if (n > 0) {
        g_string_append_len (ar->body, ar->piece, n);
        g_input_stream_read_async (ar->stream, ar->piece, sizeof ar->piece, G_PRIORITY_DEFAULT, NULL, read_ready, ar);
        return;
    }
    if (error) { printf ("  read error %s\n", error->message); g_error_free (error); }
    g_string_append (trace, " eof");
    g_input_stream_close (ar->stream, NULL, NULL);
    g_main_loop_quit (ar->loop);
}

static void sent (GObject *source, GAsyncResult *result, gpointer data)
{
    AsyncRead *ar = data;
    GError *error = NULL;
    ar->stream = soup_session_send_finish (SOUP_SESSION (source), result, &error);
    g_string_append (trace, " sent");
    if (!ar->stream) {
        printf ("  send error %s %d: %s\n", g_quark_to_string (error->domain), error->code, error->message);
        g_error_free (error);
        g_main_loop_quit (ar->loop);
        return;
    }
    printf ("  result message is the message: %d\n", soup_session_get_async_result_message (SOUP_SESSION (source), result) == ar->msg);
    g_input_stream_read_async (ar->stream, ar->piece, sizeof ar->piece, G_PRIORITY_DEFAULT, NULL, read_ready, ar);
}

static void async_get (SoupSession *session, const char *path)
{
    char *u = url (path);
    AsyncRead ar = { 0 };
    ar.loop = g_main_loop_new (NULL, FALSE);
    ar.msg = soup_message_new ("GET", u);
    ar.body = g_string_new (NULL);
    g_string_truncate (trace, 0);
    watch (ar.msg);
    printf ("async %s\n", path);
    soup_session_send_async (session, ar.msg, G_PRIORITY_DEFAULT, NULL, sent, &ar);
    g_main_loop_run (ar.loop);
    printf ("  %u %s (connection %lu) [%s]\n  signals:%s\n", soup_message_get_status (ar.msg), soup_message_get_reason_phrase (ar.msg), (unsigned long) soup_message_get_connection_id (ar.msg), ar.body->str, trace->str);
    if (ar.stream) g_object_unref (ar.stream);
    g_object_unref (ar.msg);
    g_string_free (ar.body, TRUE);
    g_main_loop_unref (ar.loop);
    g_free (u);
}

static void read_done (GObject *source, GAsyncResult *result, gpointer data)
{
    AsyncRead *ar = data;
    GError *error = NULL;
    GBytes *body = soup_session_send_and_read_finish (SOUP_SESSION (source), result, &error);
    show_response ("send_and_read_async", ar->msg, body, error);
    g_clear_error (&error);
    if (body) g_bytes_unref (body);
    g_main_loop_quit (ar->loop);
}

static void async_read (SoupSession *session, const char *path)
{
    char *u = url (path);
    AsyncRead ar = { 0 };
    ar.loop = g_main_loop_new (NULL, FALSE);
    ar.msg = soup_message_new ("GET", u);
    soup_session_send_and_read_async (session, ar.msg, G_PRIORITY_DEFAULT, NULL, read_done, &ar);
    g_main_loop_run (ar.loop);
    g_object_unref (ar.msg);
    g_main_loop_unref (ar.loop);
    g_free (u);
}

static void reuse (SoupSession *session)
{
    char *u = url ("/plain");
    SoupMessage *a = soup_message_new ("GET", u), *b = soup_message_new ("GET", u);
    GBytes *one = soup_session_send_and_read (session, a, NULL, NULL);
    GBytes *two = soup_session_send_and_read (session, b, NULL, NULL);
    printf ("reuse: same connection %d, ids nonzero %d\n", soup_message_get_connection_id (a) == soup_message_get_connection_id (b),
            soup_message_get_connection_id (a) != 0);
    g_bytes_unref (one);
    g_bytes_unref (two);
    g_object_unref (a);
    g_object_unref (b);
    g_free (u);
}

/* The logger's lines, with the addresses and times that vary left out. */
static void print_log (SoupLogger *logger, SoupLoggerLogLevel level, char direction, const char *data, gpointer user_data)
{
    GString *line = g_string_new (NULL);
    const char *p = data;
    if (g_str_has_prefix (data, "Soup-Debug-Timestamp: ")) {
        printf ("  log %d %c Soup-Debug-Timestamp: T\n", level, direction);
        g_string_free (line, TRUE);
        return;
    }
    if (g_str_has_prefix (data, "Host: 127.0.0.1:")) {
        printf ("  log %d %c Host: 127.0.0.1:PORT\n", level, direction);
        g_string_free (line, TRUE);
        return;
    }
    while (*p) {
        if (p[0] == '0' && p[1] == 'x') {
            g_string_append (line, "P");
            p += 2;
            while (g_ascii_isxdigit (*p)) p++;
        } else {
            g_string_append_c (line, *p++);
        }
    }
    printf ("  log %d %c %s\n", level, direction, line->str);
    g_string_free (line, TRUE);
}

static SoupLoggerLogLevel only_headers (SoupLogger *logger, SoupMessage *msg, gpointer data)
{
    return strstr (g_uri_get_path (soup_message_get_uri (msg)), "chunked") ? SOUP_LOGGER_LOG_HEADERS : SOUP_LOGGER_LOG_BODY;
}

static void logged (SoupSession *session)
{
    SoupLogger *logger = soup_logger_new (SOUP_LOGGER_LOG_BODY);
    int level = -1, size = 0;

    g_object_get (logger, "level", &level, "max-body-size", &size, NULL);
    printf ("logger level %d, max body %d %d\n", level, size, soup_logger_get_max_body_size (logger));
    soup_logger_set_printer (logger, print_log, NULL, NULL);
    soup_session_add_feature (session, SOUP_SESSION_FEATURE (logger));
    sync_get (session, "/plain");
    post (session, "/echo", "a logged body", FALSE);
    post (session, "/echo", "a logged chunked body", TRUE);
    auth_get (session, "/basic");
    async_get (session, "/gzip");
    soup_logger_set_max_body_size (logger, 5);
    sync_get (session, "/plain");
    soup_logger_set_request_filter (logger, only_headers, NULL, NULL);
    soup_logger_set_response_filter (logger, only_headers, NULL, NULL);
    sync_get (session, "/chunked");
    g_object_set (logger, "level", SOUP_LOGGER_LOG_MINIMAL, NULL);
    soup_logger_set_request_filter (logger, NULL, NULL, NULL);
    soup_logger_set_response_filter (logger, NULL, NULL, NULL);
    sync_get (session, "/plain");
    soup_session_remove_feature (session, SOUP_SESSION_FEATURE (logger));
    g_object_unref (logger);
}

int main (void)
{
    pid_t child;
    SoupSession *session;
    GError *error = NULL;
    char *u;
    SoupMessage *msg;
    GInputStream *stream;

    setvbuf (stdout, NULL, _IOLBF, 0);
    port = start_server (&child);
    trace = g_string_new (NULL);
    session = soup_session_new ();
    printf ("timeouts %u %u, max conns %u %u, user agent %s\n", soup_session_get_timeout (session), soup_session_get_idle_timeout (session),
            soup_session_get_max_conns (session), soup_session_get_max_conns_per_host (session), soup_session_get_user_agent (session) ? soup_session_get_user_agent (session) : "(none)");
    printf ("has decoder %d, auth manager %d, basic %d, sniffer %d\n", soup_session_has_feature (session, SOUP_TYPE_CONTENT_DECODER),
            soup_session_has_feature (session, SOUP_TYPE_AUTH_MANAGER), soup_session_has_feature (session, SOUP_TYPE_AUTH_BASIC),
            soup_session_has_feature (session, SOUP_TYPE_CONTENT_SNIFFER));

    sync_get (session, "/plain");
    sync_get (session, "/chunked");
    sync_get (session, "/eof");
    sync_get (session, "/gzip");
    sync_get (session, "/deflate");
    sync_get (session, "/rawdeflate");
    sync_get (session, "/notgzip");
    sync_get (session, "/redirect");
    sync_get (session, "/redirect-loop");
    sync_get (session, "/redirect-none");
    sync_get (session, "/missing");
    sync_get (session, "/headers");
    soup_session_set_user_agent (session, "test-agent ");
    soup_session_set_accept_language (session, "en-gb, en;q=0.8");
    sync_get (session, "/headers?ua");
    reuse (session);

    auth_get (session, "/basic");
    auth_get (session, "/basic");
    auth_get (session, "/digest");

    post (session, "/echo", "a request body", FALSE);
    post (session, "/echo", "a chunked request body", TRUE);
    post (session, "/redirect-post", "dropped on redirect", FALSE);

    /* HEAD: no body read. */
    u = url ("/plain");
    msg = soup_message_new ("HEAD", u);
    stream = soup_session_send (session, msg, NULL, &error);
    printf ("HEAD: %u, length %ld, stream %d\n", soup_message_get_status (msg),
            (long) soup_message_headers_get_content_length (soup_message_get_response_headers (msg)), stream != NULL);
    if (stream) {
        char tmp[16];
        printf ("  read %ld\n", (long) g_input_stream_read (stream, tmp, sizeof tmp, NULL, NULL));
        g_object_unref (stream);
    }
    g_object_unref (msg);
    g_free (u);

    async_get (session, "/plain");
    async_get (session, "/chunked");
    async_get (session, "/gzip");
    async_get (session, "/redirect");
    async_read (session, "/chunked");

    soup_session_add_feature_by_type (session, SOUP_TYPE_CONTENT_SNIFFER);
    async_get (session, "/sniff");
    sync_get (session, "/sniff");

    /* A message already queued, and errors. */
    u = url ("/nothing");
    msg = soup_message_new ("GET", "http://127.0.0.1:1/");
    stream = soup_session_send (session, msg, NULL, &error);
    printf ("refused: %d %s %d: %s\n", stream == NULL, error ? g_quark_to_string (error->domain) : "-", error ? error->code : 0, error ? error->message : "-");
    g_clear_error (&error);
    g_object_unref (msg);
    printf ("new message for bad URI: %p\n", (void *) soup_message_new ("GET", "not a uri"));
    printf ("new message for ftp: %d\n", soup_message_new ("GET", "ftp://host/") != NULL);
    g_free (u);

    logged (session);

    sync_get (session, "/stats");
    sync_get (session, "/quit");
    waitpid (child, NULL, 0);
    g_object_unref (session);
    return 0;
}
