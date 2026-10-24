/* WebSockets: the client's handshake request and its check of responses,
 * then connections through soup_session_websocket_connect_async to a
 * small server in this test (a process of its own) that echoes what it
 * is sent: text and binary messages of each length encoding, fragments,
 * pings and pongs, frames sent with the handshake's response, the closing
 * handshake from either side, errors (too big, bad UTF-8, a refused or
 * wrong handshake) and permessage-deflate (the server sends back the
 * compressed payloads it gets). Each line printed is compared with what
 * the system's libsoup prints. */
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

static int port;

/* ------------------------------------------------------------------ */
/* The server */

static char inbuf[262144];
static size_t have;

static int fill (int fd)
{
    ssize_t n = read (fd, inbuf + have, sizeof inbuf - have);
    if (n <= 0) return 0;
    have += (size_t) n;
    return 1;
}

static void put (int fd, const void *data, size_t length)
{
    const char *p = data;
    while (length > 0) {
        ssize_t n = write (fd, p, length);
        if (n <= 0) _exit (1);
        p += n;
        length -= (size_t) n;
    }
}

static char *accept_key (const char *key)
{
    GChecksum *checksum = g_checksum_new (G_CHECKSUM_SHA1);
    guint8 digest[20];
    gsize length = sizeof digest;
    g_checksum_update (checksum, (const guchar *) key, -1);
    g_checksum_update (checksum, (const guchar *) "258EAFA5-E914-47DA-95CA-C5AB0DC85B11", -1);
    g_checksum_get_digest (checksum, digest, &length);
    g_checksum_free (checksum);
    return g_base64_encode (digest, length);
}

/* A frame from the server (never masked). */
static void send_frame (int fd, int first, const void *data, size_t length)
{
    unsigned char header[10];
    size_t at = 2;
    header[0] = (unsigned char) first;
    if (length < 126) {
        header[1] = (unsigned char) length;
    } else if (length < 65536) {
        header[1] = 126;
        header[2] = (unsigned char) (length >> 8);
        header[3] = (unsigned char) length;
        at = 4;
    } else {
        header[1] = 127;
        for (int i = 0; i < 8; i++) header[2 + i] = (unsigned char) ((guint64) length >> (56 - 8 * i));
        at = 10;
    }
    put (fd, header, at);
    put (fd, data, length);
}

/* A frame from the client: its first byte, its payload unmasked. */
static int read_frame (int fd, int *first, unsigned char **payload, size_t *length)
{
    size_t at, len;
    unsigned char mask[4];
    while (have < 2) if (!fill (fd)) return 0;
    len = inbuf[1] & 0x7f;
    at = 2;
    if (len == 126) {
        while (have < 4) if (!fill (fd)) return 0;
        len = ((size_t) (unsigned char) inbuf[2] << 8) | (unsigned char) inbuf[3];
        at = 4;
    } else if (len == 127) {
        while (have < 10) if (!fill (fd)) return 0;
        len = 0;
        for (int i = 0; i < 8; i++) len = (len << 8) | (unsigned char) inbuf[2 + i];
        at = 10;
    }
    while (have < at + 4 + len) if (!fill (fd)) return 0;
    memcpy (mask, inbuf + at, 4);
    *first = (unsigned char) inbuf[0];
    *payload = g_malloc (len + 1);
    for (size_t i = 0; i < len; i++) (*payload)[i] = (unsigned char) inbuf[at + 4 + i] ^ mask[i & 3];
    (*payload)[len] = 0;
    *length = len;
    memmove (inbuf, inbuf + at + 4 + len, have - at - 4 - len);
    have -= at + 4 + len;
    return 1;
}

static const char *header_value (const char *head, const char *name, char *out, size_t size)
{
    const char *p = head;
    size_t name_len = strlen (name);
    while ((p = strstr (p, "\r\n"))) {
        p += 2;
        if (g_ascii_strncasecmp (p, name, name_len) == 0 && p[name_len] == ':') {
            const char *v = p + name_len + 1, *e;
            while (*v == ' ') v++;
            e = strstr (v, "\r\n");
            snprintf (out, size, "%.*s", (int) (e - v), v);
            return out;
        }
    }
    return NULL;
}

static void serve (int fd)
{
    char *end, head[8192], path[512], key[256], value[1024], response[2048];
    char *accept;
    int first;
    unsigned char *payload;
    size_t length;

    have = 0;
    while (!(end = g_strstr_len (inbuf, have, "\r\n\r\n"))) if (!fill (fd)) return;
    end += 4;
    memcpy (head, inbuf, (size_t) (end - inbuf));
    head[end - inbuf] = 0;
    memmove (inbuf, end, have - (size_t) (end - inbuf));
    have -= (size_t) (end - inbuf);
    sscanf (head, "GET %511s", path);

    if (strcmp (path, "/notws") == 0) {
        const char *text = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nConnection: close\r\n\r\nplain";
        put (fd, text, strlen (text));
        return;
    }
    if (!header_value (head, "Sec-WebSocket-Key", key, sizeof key)) return;
    accept = strcmp (path, "/badkey") == 0 ? g_strdup ("AAAAAAAAAAAAAAAAAAAAAAAAAAA=") : accept_key (key);
    snprintf (response, sizeof response, "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n%s%s\r\n",
              accept, header_value (head, "Sec-WebSocket-Protocol", value, sizeof value) ? "Sec-WebSocket-Protocol: chat\r\n" : "",
              strcmp (path, "/deflate") == 0 ? "Sec-WebSocket-Extensions: permessage-deflate\r\n" : "");
    g_free (accept);
    if (strcmp (path, "/early") == 0) {
        /* The response and frames in one write: a fragmented text. */
        GString *all = g_string_new (response);
        g_string_append_len (all, "\x01\x04" "frag", 6);
        g_string_append_len (all, "\x00\x05" "mente", 7);
        g_string_append_len (all, "\x80\x01" "d", 3);
        put (fd, all->str, all->len);
        g_string_free (all, TRUE);
    } else {
        put (fd, response, strlen (response));
    }
    if (strcmp (path, "/badkey") == 0) return;

    if (strcmp (path, "/ping") == 0) send_frame (fd, 0x89, "are you there", 13);
    if (strcmp (path, "/goaway") == 0) {
        send_frame (fd, 0x88, "\x03\xe9" "going", 7);
    }
    if (strcmp (path, "/big") == 0) {
        char *big = g_malloc0 (2000);
        send_frame (fd, 0x82, big, 2000);
        g_free (big);
    }
    if (strcmp (path, "/badutf8") == 0) send_frame (fd, 0x81, "\xc3\x28", 2);
    if (strcmp (path, "/masked") == 0) put (fd, "\x81\x81\x01\x02\x03\x04\x60", 7);
    if (strcmp (path, "/extensions") == 0) {
        const char *asked = header_value (head, "Sec-WebSocket-Extensions", value, sizeof value);
        send_frame (fd, 0x81, asked ? asked : "(none)", strlen (asked ? asked : "(none)"));
    }

    while (read_frame (fd, &first, &payload, &length)) {
        int opcode = first & 0x0f;
        if (opcode == 8) {
            /* The close echoed. */
            send_frame (fd, 0x88, payload, length);
            g_free (payload);
            break;
        }
        if (opcode == 9) {
            send_frame (fd, 0x8a, payload, length);
        } else if (opcode == 10) {
            char text[256];
            snprintf (text, sizeof text, "pong '%.*s'", (int) length, payload);
            send_frame (fd, 0x81, text, strlen (text));
        } else {
            /* Echoed, compressed or not, as it came. */
            send_frame (fd, first, payload, length);
        }
        g_free (payload);
    }
    /* The client closes its side after the close. */
    while (fill (fd)) have = 0;
}

static int start_server (pid_t *child)
{
    struct sockaddr_in address;
    socklen_t length = sizeof address;
    int fd = socket (AF_INET, SOCK_STREAM, 0), one = 1;
    setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    memset (&address, 0, sizeof address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
    bind (fd, (struct sockaddr *) &address, sizeof address);
    listen (fd, 16);
    getsockname (fd, (struct sockaddr *) &address, &length);
    *child = fork ();
    if (*child == 0) {
        alarm (120);
        signal (SIGCHLD, SIG_IGN);
        for (;;) {
            int client = accept (fd, NULL, NULL);
            if (client < 0) continue;
            if (fork () == 0) {
                close (fd);
                serve (client);
                shutdown (client, SHUT_WR);
                close (client);
                _exit (0);
            }
            close (client);
        }
    }
    close (fd);
    return ntohs (address.sin_port);
}

/* ------------------------------------------------------------------ */
/* The client */

typedef struct {
    GMainLoop *loop;
    SoupWebsocketConnection *ws;
    GError *error;
    int messages;
    int expect;
    char *last_error;
} State;

static void print_message (int type, GBytes *message)
{
    gsize size;
    const char *data = g_bytes_get_data (message, &size);
    printf ("  message %d (%zu)", type, size);
    if (type == SOUP_WEBSOCKET_DATA_TEXT) printf (": '%.*s'", (int) (size > 60 ? 60 : size), data);
    else {
        guint sum = 0;
        for (gsize i = 0; i < size; i++) sum = sum * 31 + (guchar) data[i];
        printf (", sum %u", sum);
    }
    printf (", nul %d\n", data[size] == 0);
}

static void on_message (SoupWebsocketConnection *ws, int type, GBytes *message, State *state)
{
    print_message (type, message);
    if (++state->messages == state->expect) g_main_loop_quit (state->loop);
}

/* (An error is printed once: a bad frame stays in the buffer and is met
 * again on each later read, how many times depending on how the peer's
 * bytes arrive.) */
static void on_error (SoupWebsocketConnection *ws, GError *error, State *state)
{
    char *text = g_strdup_printf ("  error: %s %d %s\n", g_quark_to_string (error->domain), error->code, error->message);
    if (g_strcmp0 (text, state->last_error) != 0) printf ("%s", text);
    g_free (state->last_error);
    state->last_error = text;
}

static void on_closing (SoupWebsocketConnection *ws, State *state)
{
    printf ("  closing (state %d)\n", soup_websocket_connection_get_state (ws));
}

static void on_closed (SoupWebsocketConnection *ws, State *state)
{
    printf ("  closed: code %u, data '%s', state %d\n", soup_websocket_connection_get_close_code (ws),
            soup_websocket_connection_get_close_data (ws) ? soup_websocket_connection_get_close_data (ws) : "(null)",
            soup_websocket_connection_get_state (ws));
    g_main_loop_quit (state->loop);
}

static void on_pong (SoupWebsocketConnection *ws, GBytes *message, State *state)
{
    printf ("  pong (%zu)\n", g_bytes_get_size (message));
}

static void connected (GObject *source, GAsyncResult *result, gpointer data)
{
    State *state = data;
    state->ws = soup_session_websocket_connect_finish (SOUP_SESSION (source), result, &state->error);
    g_main_loop_quit (state->loop);
}

static gboolean timed_out (gpointer data)
{
    printf ("  (timed out)\n");
    g_main_loop_quit (data);
    return G_SOURCE_REMOVE;
}

static void run (State *state)
{
    guint timeout = g_timeout_add_seconds (20, timed_out, state->loop);
    g_main_loop_run (state->loop);
    g_source_remove (timeout);
}

static SoupWebsocketConnection *connect_to (SoupSession *session, const char *path, State *state, char **protocols)
{
    char *uri = g_strdup_printf ("ws://127.0.0.1:%d%s", port, path);
    SoupMessage *msg = soup_message_new ("GET", uri);
    GList *l;
    memset (state, 0, sizeof *state);
    state->loop = g_main_loop_new (NULL, FALSE);
    soup_session_websocket_connect_async (session, msg, "http://example.com", protocols, G_PRIORITY_DEFAULT, NULL, connected, state);
    run (state);
    printf ("%s: status %u", path, soup_message_get_status (msg));
    if (!state->ws) {
        printf (", error %s %d: %s\n", g_quark_to_string (state->error->domain), state->error->code, state->error->message);
        g_clear_error (&state->error);
    } else {
        GUri *u = soup_websocket_connection_get_uri (state->ws);
        printf (", connected: type %d, state %d, protocol %s, origin %s, uri %s://%s%s, max %" G_GUINT64_FORMAT ", keepalive %u, extensions",
                soup_websocket_connection_get_connection_type (state->ws), soup_websocket_connection_get_state (state->ws),
                soup_websocket_connection_get_protocol (state->ws) ? soup_websocket_connection_get_protocol (state->ws) : "(null)",
                soup_websocket_connection_get_origin (state->ws) ? soup_websocket_connection_get_origin (state->ws) : "(null)",
                g_uri_get_scheme (u), g_uri_get_host (u), g_uri_get_path (u),
                soup_websocket_connection_get_max_incoming_payload_size (state->ws), soup_websocket_connection_get_keepalive_interval (state->ws));
        for (l = soup_websocket_connection_get_extensions (state->ws); l; l = l->next) printf (" %s", G_OBJECT_TYPE_NAME (l->data));
        printf ("\n");
        g_signal_connect (state->ws, "message", G_CALLBACK (on_message), state);
        g_signal_connect (state->ws, "error", G_CALLBACK (on_error), state);
        g_signal_connect (state->ws, "closing", G_CALLBACK (on_closing), state);
        g_signal_connect (state->ws, "closed", G_CALLBACK (on_closed), state);
        g_signal_connect (state->ws, "pong", G_CALLBACK (on_pong), state);
    }
    g_object_unref (msg);
    g_free (uri);
    return state->ws;
}

static void expect_messages (State *state, int count)
{
    state->messages = 0;
    state->expect = count;
    run (state);
}

static void finish (State *state)
{
    if (state->ws) {
        if (soup_websocket_connection_get_state (state->ws) != SOUP_WEBSOCKET_STATE_CLOSED) run (state);
        g_object_unref (state->ws);
    }
    g_clear_pointer (&state->last_error, g_free);
    g_main_loop_unref (state->loop);
}

static void close_and_finish (State *state, gushort code, const char *data)
{
    soup_websocket_connection_close (state->ws, code, data);
    printf ("  close sent: state %d\n", soup_websocket_connection_get_state (state->ws));
    finish (state);
}

static void handshake_checks (void)
{
    SoupMessage *msg = soup_message_new ("GET", "ws://example.com/socket");
    char *protocols[] = { "one", "two", NULL };
    GError *error = NULL;
    const char *key;
    char *accept;
    GChecksum *checksum;
    guint8 digest[20];
    gsize length = sizeof digest;
    SoupMessageHeadersIter iter;
    const char *name, *value;

    soup_websocket_client_prepare_handshake (msg, "http://example.com", protocols, NULL);
    soup_message_headers_iter_init (&iter, soup_message_get_request_headers (msg));
    while (soup_message_headers_iter_next (&iter, &name, &value))
        printf ("request %s: %s\n", name, strcmp (name, "Sec-WebSocket-Key") == 0 ? (strlen (value) == 24 ? "(24 characters)" : value) : value);

    printf ("verify 200: %d", soup_websocket_client_verify_handshake (msg, NULL, NULL, &error));
    printf (" %s\n", error ? error->message : "");
    g_clear_error (&error);

    key = soup_message_headers_get_one (soup_message_get_request_headers (msg), "Sec-WebSocket-Key");
    checksum = g_checksum_new (G_CHECKSUM_SHA1);
    g_checksum_update (checksum, (const guchar *) key, -1);
    g_checksum_update (checksum, (const guchar *) "258EAFA5-E914-47DA-95CA-C5AB0DC85B11", -1);
    g_checksum_get_digest (checksum, digest, &length);
    g_checksum_free (checksum);
    accept = g_base64_encode (digest, length);
    printf ("error quark %s\n", g_quark_to_string (SOUP_WEBSOCKET_ERROR));
    g_object_unref (msg);
    g_free (accept);
}

int main (void)
{
    pid_t child;
    SoupSession *session;
    State state;
    char *protocols[] = { "chat", "superchat", NULL };
    char *big;
    GBytes *bytes;

    setvbuf (stdout, NULL, _IOLBF, 0);
    handshake_checks ();
    port = start_server (&child);
    session = soup_session_new ();

    if (connect_to (session, "/echo", &state, protocols)) {
        soup_websocket_connection_send_text (state.ws, "hello");
        soup_websocket_connection_send_binary (state.ws, "\x01\x02\x03", 3);
        big = g_malloc (70000);
        for (int i = 0; i < 70000; i++) big[i] = (char) (i * 7);
        soup_websocket_connection_send_binary (state.ws, big, 70000);
        memset (big, 'x', 300);
        big[300] = 0;
        soup_websocket_connection_send_text (state.ws, big);
        g_free (big);
        bytes = g_bytes_new_static ("as bytes", 8);
        soup_websocket_connection_send_message (state.ws, SOUP_WEBSOCKET_DATA_TEXT, bytes);
        g_bytes_unref (bytes);
        soup_websocket_connection_send_text (state.ws, "");
        expect_messages (&state, 6);
        close_and_finish (&state, SOUP_WEBSOCKET_CLOSE_NORMAL, "bye");
    }

    /* The server's ping answered: the server says what the pong had. */
    if (connect_to (session, "/ping", &state, NULL)) {
        expect_messages (&state, 1);
        close_and_finish (&state, SOUP_WEBSOCKET_CLOSE_GOING_AWAY, NULL);
    }

    if (connect_to (session, "/early", &state, NULL)) {
        expect_messages (&state, 1);
        close_and_finish (&state, SOUP_WEBSOCKET_CLOSE_NORMAL, NULL);
    }

    if (connect_to (session, "/goaway", &state, NULL)) finish (&state);

    if (connect_to (session, "/big", &state, NULL)) {
        soup_websocket_connection_set_max_incoming_payload_size (state.ws, 1000);
        finish (&state);
    }

    if (connect_to (session, "/badutf8", &state, NULL)) finish (&state);
    if (connect_to (session, "/masked", &state, NULL)) finish (&state);

    connect_to (session, "/notws", &state, NULL);
    g_main_loop_unref (state.loop);
    connect_to (session, "/badkey", &state, NULL);
    g_main_loop_unref (state.loop);

    if (connect_to (session, "/extensions", &state, NULL)) {
        expect_messages (&state, 1);
        close_and_finish (&state, SOUP_WEBSOCKET_CLOSE_NORMAL, NULL);
    }

    soup_session_add_feature_by_type (session, SOUP_TYPE_WEBSOCKET_EXTENSION_MANAGER);
    printf ("manager %d, deflate %d\n", soup_session_has_feature (session, SOUP_TYPE_WEBSOCKET_EXTENSION_MANAGER),
            soup_session_has_feature (session, SOUP_TYPE_WEBSOCKET_EXTENSION_DEFLATE));
    if (connect_to (session, "/extensions", &state, NULL)) {
        expect_messages (&state, 1);
        close_and_finish (&state, SOUP_WEBSOCKET_CLOSE_NORMAL, NULL);
    }
    if (connect_to (session, "/deflate", &state, NULL)) {
        soup_websocket_connection_send_text (state.ws, "compressed hello, compressed hello, compressed hello");
        soup_websocket_connection_send_text (state.ws, "compressed hello again");
        big = g_malloc (20000);
        for (int i = 0; i < 20000; i++) big[i] = (char) ("abcdefgh"[i % 8] + (i / 1000));
        soup_websocket_connection_send_binary (state.ws, big, 20000);
        g_free (big);
        soup_websocket_connection_send_text (state.ws, "");
        expect_messages (&state, 4);
        close_and_finish (&state, SOUP_WEBSOCKET_CLOSE_NORMAL, "done");
    }

    g_object_unref (session);
    kill (child, SIGTERM);
    waitpid (child, NULL, 0);
    return 0;
}
