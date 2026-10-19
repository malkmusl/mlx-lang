/* SoupServer: handlers by path (early ones, a paused one writing a chunked
 * body as it goes, redirects, queries, request bodies plain and chunked,
 * "Expect: 100-continue"), the responses as bytes on the wire to a client
 * process speaking HTTP/1.1 and 1.0 by hand (keep-alive, bad requests),
 * auth domains (Basic and Digest) and a WebSocket handler as a SoupSession
 * in the same process sees them, and the server's signals. Each line
 * printed is compared with what the system's libsoup prints. */
#define _GNU_SOURCE
#include <libsoup/soup.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

static int port;
static GString *events;
static gboolean quiet;

static void event (const char *format, const char *a, const char *b)
{
    char line[512];
    snprintf (line, sizeof line, format, a, b);
    if (quiet) {
        g_string_append (events, line);
        g_string_append_c (events, '\n');
    } else {
        printf ("%s\n", line);
    }
}

static const char *path_of (SoupServerMessage *msg)
{
    GUri *uri = soup_server_message_get_uri (msg);
    return uri ? g_uri_get_path (uri) : "(none)";
}

static void on_started (SoupServer *server, SoupServerMessage *msg, gpointer data) { event ("  server: request-started%s%s", "", ""); }
static void on_read (SoupServer *server, SoupServerMessage *msg, gpointer data) { event ("  server: request-read %s %s", soup_server_message_get_method (msg), path_of (msg)); }
static void on_finished (SoupServer *server, SoupServerMessage *msg, gpointer data)
{
    char status[16];
    snprintf (status, sizeof status, "%u", soup_server_message_get_status (msg));
    event ("  server: request-finished %s %s", path_of (msg), status);
}
static void on_aborted (SoupServer *server, SoupServerMessage *msg, gpointer data) { event ("  server: request-aborted %s%s", soup_server_message_get_method (msg) ? soup_server_message_get_method (msg) : "(none)", ""); }

static void root_handler (SoupServer *server, SoupServerMessage *msg, const char *path, GHashTable *query, gpointer data)
{
    soup_server_message_set_status (msg, SOUP_STATUS_OK, NULL);
    soup_server_message_set_response (msg, "text/plain", SOUP_MEMORY_STATIC, "root page\n", 10);
}

static void echo_handler (SoupServer *server, SoupServerMessage *msg, const char *path, GHashTable *query, gpointer data)
{
    SoupMessageBody *body = soup_server_message_get_request_body (msg);
    GString *out = g_string_new (NULL);
    const char *type = soup_message_headers_get_content_type (soup_server_message_get_request_headers (msg), NULL);
    g_string_append_printf (out, "%s %s HTTP/1.%d, path %s, type %s, body %d: %.*s", soup_server_message_get_method (msg),
                            g_uri_get_path (soup_server_message_get_uri (msg)), soup_server_message_get_http_version (msg), path,
                            type ? type : "(none)", (int) body->length, (int) body->length, body->data ? body->data : "");
    if (query) g_string_append_printf (out, ", query a=%s", (char *) g_hash_table_lookup (query, "a"));
    soup_server_message_set_status (msg, SOUP_STATUS_OK, NULL);
    soup_server_message_set_response (msg, "text/plain", SOUP_MEMORY_COPY, out->str, out->len);
    g_string_free (out, TRUE);
}

static void early_handler (SoupServer *server, SoupServerMessage *msg, const char *path, GHashTable *query, gpointer data)
{
    event ("  server: early handler for %s%s", path, "");
    if (strstr (path, "forbidden")) soup_server_message_set_status (msg, SOUP_STATUS_FORBIDDEN, NULL);
}

static void redirect_handler (SoupServer *server, SoupServerMessage *msg, const char *path, GHashTable *query, gpointer data)
{
    soup_server_message_set_redirect (msg, SOUP_STATUS_FOUND, "/echo?a=redirected");
}

/* A chunked body written a piece at a time: the message is paused until
 * each piece is there. */
typedef struct {
    SoupServerMessage *msg;
    int piece;
} Chunks;

static gboolean next_piece (gpointer data)
{
    Chunks *chunks = data;
    SoupMessageBody *body = soup_server_message_get_response_body (chunks->msg);
    if (chunks->piece < 3) {
        char text[32];
        snprintf (text, sizeof text, "piece %d\n", chunks->piece++);
        soup_message_body_append (body, SOUP_MEMORY_COPY, text, strlen (text));
        soup_server_message_unpause (chunks->msg);
        return G_SOURCE_CONTINUE;
    }
    soup_message_body_complete (body);
    soup_server_message_unpause (chunks->msg);
    g_object_unref (chunks->msg);
    g_free (chunks);
    return G_SOURCE_REMOVE;
}

static void chunked_handler (SoupServer *server, SoupServerMessage *msg, const char *path, GHashTable *query, gpointer data)
{
    Chunks *chunks = g_new0 (Chunks, 1);
    soup_server_message_set_status (msg, SOUP_STATUS_OK, NULL);
    soup_message_headers_set_encoding (soup_server_message_get_response_headers (msg), SOUP_ENCODING_CHUNKED);
    soup_message_headers_set_content_type (soup_server_message_get_response_headers (msg), "text/plain", NULL);
    chunks->msg = g_object_ref (msg);
    soup_server_message_pause (msg);
    g_timeout_add (20, next_piece, chunks);
}

static void auth_handler (SoupServer *server, SoupServerMessage *msg, const char *path, GHashTable *query, gpointer data)
{
    soup_server_message_set_status (msg, SOUP_STATUS_OK, NULL);
    soup_server_message_set_response (msg, "text/plain", SOUP_MEMORY_STATIC, "secret", 6);
}

static gboolean basic_check (SoupAuthDomain *domain, SoupServerMessage *msg, const char *user, const char *password, gpointer data)
{
    event ("  server: basic check %s %s", user, password);
    return strcmp (user, "user") == 0 && strcmp (password, "pass") == 0;
}

static char *digest_lookup (SoupAuthDomain *domain, SoupServerMessage *msg, const char *user, gpointer data)
{
    event ("  server: digest lookup %s%s", user, "");
    if (strcmp (user, "user") != 0) return NULL;
    return soup_auth_domain_digest_encode_password (user, "digest realm", "pass");
}

static void ws_message (SoupWebsocketConnection *ws, int type, GBytes *message, gpointer data)
{
    gsize size;
    const char *text = g_bytes_get_data (message, &size);
    char *reply = g_strdup_printf ("server got %.*s", (int) size, text);
    soup_websocket_connection_send_text (ws, reply);
    g_free (reply);
}

static void ws_closed (SoupWebsocketConnection *ws, gpointer data)
{
    event ("  server: websocket closed, code %s%s", soup_websocket_connection_get_close_code (ws) == 1000 ? "1000" : "other", "");
    g_object_unref (ws);
}

static void ws_handler (SoupServer *server, SoupServerMessage *msg, const char *path, SoupWebsocketConnection *ws, gpointer data)
{
    event ("  server: websocket at %s, protocol %s", path, soup_websocket_connection_get_protocol (ws) ? soup_websocket_connection_get_protocol (ws) : "(none)");
    g_object_ref (ws);
    g_signal_connect (ws, "message", G_CALLBACK (ws_message), NULL);
    g_signal_connect (ws, "closed", G_CALLBACK (ws_closed), NULL);
}

/* ------------------------------------------------------------------ */
/* The client by hand, in a process of its own. */

static int dial (void)
{
    struct sockaddr_in address;
    int fd = socket (AF_INET, SOCK_STREAM, 0);
    memset (&address, 0, sizeof address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
    address.sin_port = htons (port);
    if (connect (fd, (struct sockaddr *) &address, sizeof address) != 0) return -1;
    return fd;
}

/* A header's value in a response's head (case not minded), or NULL. */
static const char *find_header (const char *head, const char *end, const char *name)
{
    size_t length = strlen (name);
    for (const char *p = head; p < end; p++) {
        if ((p == head || p[-1] == '\n') && g_ascii_strncasecmp (p, name, length) == 0 && p[length] == ':') return p + length + 1;
    }
    return NULL;
}

/* What the server sends for the request: one response (its headers and
 * the body they say: none for HEAD or an informational status), or all
 * until it closes; printed with the Date header's value left out. */
static void exchange (int fd, const char *what, const char *request, int expect_close)
{
    char buffer[16384];
    size_t have = 0;
    ssize_t n = 1;
    struct timeval tv = { 3, 0 };
    int head = strncmp (request, "HEAD ", 5) == 0;
    printf ("%s:\n", what);
    if (write (fd, request, strlen (request)) < 0) return;
    setsockopt (fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    for (;;) {
        n = read (fd, buffer + have, sizeof buffer - 1 - have);
        if (n <= 0) break;
        have += (size_t) n;
        buffer[have] = 0;
        if (!expect_close) {
            char *end = strstr (buffer, "\r\n\r\n");
            const char *length, *coding;
            if (!end) continue;
            length = find_header (buffer, end, "Content-Length");
            coding = find_header (buffer, end, "Transfer-Encoding");
            if (head || strncmp (buffer + 9, "1", 1) == 0) break;
            if (length) {
                if (have >= (size_t) (end + 4 - buffer) + (size_t) atol (length)) break;
            } else if (coding) {
                if (strstr (end, "\r\n0\r\n\r\n")) break;
            } else {
                break;
            }
        }
    }
    buffer[have] = 0;
    for (char *line = strtok (buffer, "\n"); line; line = strtok (NULL, "\n")) {
        size_t len = strlen (line);
        if (len && line[len - 1] == '\r') line[len - 1] = 0;
        if (strncmp (line, "Date: ", 6) == 0) printf ("  Date: (now)\n");
        else printf ("  %s\n", line);
    }
    if (expect_close) printf ("  (closed %d)\n", n == 0);
}

static void by_hand (void)
{
    int fd;

    fd = dial ();
    exchange (fd, "GET / keep-alive", "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n", 0);
    exchange (fd, "GET /missing on the same connection", "GET /missing HTTP/1.1\r\nHost: localhost\r\n\r\n", 0);
    exchange (fd, "HEAD /", "HEAD / HTTP/1.1\r\nHost: localhost\r\n\r\n", 0);
    exchange (fd, "POST /echo", "POST /echo?a=1 HTTP/1.1\r\nHost: localhost\r\nContent-Type: text/plain\r\nContent-Length: 11\r\n\r\nhello there", 0);
    exchange (fd, "POST /echo chunked", "POST /echo HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n6\r\n there\r\n0\r\n\r\n", 0);
    exchange (fd, "GET /chunked", "GET /chunked HTTP/1.1\r\nHost: localhost\r\n\r\n", 0);
    exchange (fd, "GET /early/forbidden", "GET /early/forbidden HTTP/1.1\r\nHost: localhost\r\n\r\n", 0);
    exchange (fd, "GET /early/fine", "GET /early/fine HTTP/1.1\r\nHost: localhost\r\n\r\n", 0);
    exchange (fd, "GET /redirect", "GET /redirect HTTP/1.1\r\nHost: localhost\r\n\r\n", 0);
    exchange (fd, "GET %2e%2e", "GET /echo/%2e%2e/x HTTP/1.1\r\nHost: localhost\r\n\r\n", 0);
    exchange (fd, "OPTIONS *", "OPTIONS * HTTP/1.1\r\nHost: localhost\r\n\r\n", 0);
    exchange (fd, "Connection: close", "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n", 1);
    close (fd);

    fd = dial ();
    exchange (fd, "HTTP/1.0 without Host", "GET /echo HTTP/1.0\r\n\r\n", 1);
    close (fd);
    fd = dial ();
    exchange (fd, "Expect: 100-continue, header part", "POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 4\r\nExpect: 100-continue\r\n\r\n", 0);
    exchange (fd, "Expect: 100-continue, body", "body", 0);
    close (fd);
    fd = dial ();
    exchange (fd, "a bad request line", "NONSENSE\r\n\r\n", 1);
    close (fd);
    fd = dial ();
    exchange (fd, "unknown transfer coding", "POST /echo HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: gzip\r\n\r\n", 1);
    close (fd);
    fd = dial ();
    exchange (fd, "Host with a slash", "GET / HTTP/1.1\r\nHost: a/b\r\n\r\n", 1);
    close (fd);
}

static gboolean tick_cb (gpointer data)
{
    return G_SOURCE_CONTINUE;
}

/* ------------------------------------------------------------------ */
/* A SoupSession in this process. */

static GMainLoop *loop;

static gboolean on_authenticate (SoupMessage *msg, SoupAuth *auth, gboolean retrying, gpointer data)
{
    printf ("  client: authenticate %s realm \"%s\" retrying %d\n", soup_auth_get_scheme_name (auth), soup_auth_get_realm (auth), retrying);
    if (!retrying) soup_auth_authenticate (auth, "user", data ? data : "pass");
    return FALSE;
}

static void got (GObject *source, GAsyncResult *result, gpointer data)
{
    SoupMessage *msg = data;
    GError *error = NULL;
    GBytes *body = soup_session_send_and_read_finish (SOUP_SESSION (source), result, &error);
    if (error) printf ("  client: error %s\n", error->message);
    else printf ("  client: %u %s: %.*s\n", soup_message_get_status (msg), soup_message_get_reason_phrase (msg), (int) g_bytes_get_size (body), (const char *) g_bytes_get_data (body, NULL));
    g_clear_error (&error);
    if (body) g_bytes_unref (body);
    g_main_loop_quit (loop);
}

static void fetch (SoupSession *session, const char *what, const char *path, const char *password)
{
    char *uri = g_strdup_printf ("http://127.0.0.1:%d%s", port, path);
    SoupMessage *msg = soup_message_new ("GET", uri);
    printf ("%s:\n", what);
    g_signal_connect (msg, "authenticate", G_CALLBACK (on_authenticate), (gpointer) password);
    soup_session_send_and_read_async (session, msg, G_PRIORITY_DEFAULT, NULL, got, msg);
    g_main_loop_run (loop);
    g_object_unref (msg);
    g_free (uri);
}

typedef struct {
    SoupWebsocketConnection *ws;
    int messages;
} WsState;

static void ws_connected (GObject *source, GAsyncResult *result, gpointer data)
{
    WsState *state = data;
    GError *error = NULL;
    state->ws = soup_session_websocket_connect_finish (SOUP_SESSION (source), result, &error);
    if (error) printf ("  client: websocket error %s\n", error->message);
    g_clear_error (&error);
    g_main_loop_quit (loop);
}

static void ws_client_message (SoupWebsocketConnection *ws, int type, GBytes *message, gpointer data)
{
    WsState *state = data;
    printf ("  client: websocket message '%.*s'\n", (int) g_bytes_get_size (message), (const char *) g_bytes_get_data (message, NULL));
    if (++state->messages == 2) g_main_loop_quit (loop);
}

static void ws_client_closed (SoupWebsocketConnection *ws, gpointer data)
{
    printf ("  client: websocket closed, code %u\n", soup_websocket_connection_get_close_code (ws));
    g_main_loop_quit (loop);
}

int main (void)
{
    SoupServer *server;
    SoupAuthDomain *basic, *digest;
    GError *error = NULL;
    GSList *uris, *l;
    pid_t child;
    char *protocols[] = { "chat", NULL };

    setvbuf (stdout, NULL, _IOLBF, 0);
    events = g_string_new (NULL);
    server = soup_server_new ("server-header", "test-server ", NULL);
    {
        char *header = NULL;
        g_object_get (server, "server-header", &header, NULL);
        printf ("server header: %s, https %d\n", header, soup_server_is_https (server));
        g_free (header);
    }
    if (!soup_server_listen_local (server, 0, SOUP_SERVER_LISTEN_IPV4_ONLY, &error)) {
        printf ("listen: %s\n", error->message);
        return 1;
    }
    uris = soup_server_get_uris (server);
    for (l = uris; l; l = l->next) {
        printf ("uri: %s://%s path %s\n", g_uri_get_scheme (l->data), g_uri_get_host (l->data), g_uri_get_path (l->data));
        port = g_uri_get_port (l->data);
    }
    g_slist_free_full (uris, (GDestroyNotify) g_uri_unref);
    l = soup_server_get_listeners (server);
    printf ("listeners: %u\n", g_slist_length (l));
    g_slist_free (l);
    printf ("https listen: %d", soup_server_listen_local (server, 0, SOUP_SERVER_LISTEN_HTTPS | SOUP_SERVER_LISTEN_IPV4_ONLY, &error));
    printf (" %s\n", error ? error->message : "");
    g_clear_error (&error);

    g_signal_connect (server, "request-started", G_CALLBACK (on_started), NULL);
    g_signal_connect (server, "request-read", G_CALLBACK (on_read), NULL);
    g_signal_connect (server, "request-finished", G_CALLBACK (on_finished), NULL);
    g_signal_connect (server, "request-aborted", G_CALLBACK (on_aborted), NULL);
    soup_server_add_handler (server, NULL, root_handler, NULL, NULL);
    soup_server_add_handler (server, "/echo", echo_handler, NULL, NULL);
    soup_server_add_handler (server, "/chunked", chunked_handler, NULL, NULL);
    soup_server_add_early_handler (server, "/early", early_handler, NULL, NULL);
    soup_server_add_handler (server, "/early", root_handler, NULL, NULL);
    soup_server_add_handler (server, "/redirect", redirect_handler, NULL, NULL);
    soup_server_add_handler (server, "/basic", auth_handler, NULL, NULL);
    soup_server_add_handler (server, "/digest", auth_handler, NULL, NULL);
    soup_server_add_websocket_handler (server, "/ws", NULL, protocols, ws_handler, NULL, NULL);

    basic = soup_auth_domain_basic_new ("realm", "basic realm", NULL);
    soup_auth_domain_add_path (basic, "/basic");
    soup_auth_domain_basic_set_auth_callback (basic, basic_check, NULL, NULL);
    soup_server_add_auth_domain (server, basic);
    digest = soup_auth_domain_digest_new ("realm", "digest realm", NULL);
    soup_auth_domain_add_path (digest, "/digest");
    soup_auth_domain_digest_set_auth_callback (digest, digest_lookup, NULL, NULL);
    soup_server_add_auth_domain (server, digest);
    printf ("realms: %s, %s\n", soup_auth_domain_get_realm (basic), soup_auth_domain_get_realm (digest));

    /* The hand-written client, its output first, then what the server saw. */
    quiet = TRUE;
    child = fork ();
    if (child == 0) {
        by_hand ();
        _exit (0);
    }
    loop = g_main_loop_new (NULL, FALSE);
    {
        GMainContext *context = g_main_context_default ();
        /* (A tick, so that the loop sees the child end.) */
        guint tick = g_timeout_add (20, tick_cb, NULL);
        while (waitpid (child, NULL, WNOHANG) == 0) g_main_context_iteration (context, TRUE);
        /* Let what the server has left run. */
        for (int i = 0; i < 20; i++) g_main_context_iteration (context, FALSE);
        g_source_remove (tick);
    }
    printf ("what the server saw:\n%s", events->str);
    quiet = FALSE;

    {
        SoupSession *session = soup_session_new ();
        WsState state = { 0 };
        char *uri;
        SoupMessage *msg;

        fetch (session, "session GET /", "/", NULL);
        fetch (session, "session GET /basic", "/basic", NULL);
        fetch (session, "session GET /basic, wrong password", "/basic", "wrong");
        fetch (session, "session GET /digest", "/digest", NULL);
        fetch (session, "session GET /redirect", "/redirect", NULL);

        printf ("websocket:\n");
        uri = g_strdup_printf ("ws://127.0.0.1:%d/ws", port);
        msg = soup_message_new ("GET", uri);
        soup_session_websocket_connect_async (session, msg, NULL, protocols, G_PRIORITY_DEFAULT, NULL, ws_connected, &state);
        g_main_loop_run (loop);
        if (state.ws) {
            printf ("  client: protocol %s\n", soup_websocket_connection_get_protocol (state.ws));
            g_signal_connect (state.ws, "message", G_CALLBACK (ws_client_message), &state);
            g_signal_connect (state.ws, "closed", G_CALLBACK (ws_client_closed), &state);
            soup_websocket_connection_send_text (state.ws, "one");
            soup_websocket_connection_send_text (state.ws, "two");
            g_main_loop_run (loop);
            soup_websocket_connection_close (state.ws, 1000, NULL);
            g_main_loop_run (loop);
            g_object_unref (state.ws);
        }
        g_object_unref (msg);
        g_free (uri);
        g_object_unref (session);
    }
    for (int i = 0; i < 20; i++) g_main_context_iteration (NULL, FALSE);

    soup_server_disconnect (server);
    l = soup_server_get_listeners (server);
    printf ("listeners after disconnect: %u\n", g_slist_length (l));
    g_slist_free (l);
    g_object_unref (basic);
    g_object_unref (digest);
    g_object_unref (server);
    return 0;
}
