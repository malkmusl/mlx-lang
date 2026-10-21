/* Multipart bodies: parsed whole (parts, headers, bodies, the preamble
 * and epilogue dropped, bodies that are not multipart), put together
 * (form fields and files), and read part by part from a stream, at once
 * and asynchronously, from memory and from a server that sends the parts
 * a piece at a time. Each line printed is compared with what the system's
 * libsoup prints. */
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

static const char body_text[] =
    "This is the preamble.\r\n"
    "--xyz\r\n"
    "Content-Type: text/plain\r\n"
    "X-Part: one\r\n"
    "\r\n"
    "first part\r\n"
    "--xyz\r\n"
    "\r\n"
    "second part, no headers\r\n"
    "--xyz\r\n"
    "Content-Type: text/html\r\n"
    "Content-Length: 18\r\n"
    "\r\n"
    "<b>third part</b>\n"
    "\r\n"
    "--xyz--\r\n"
    "The epilogue.\r\n";

static void print_headers (const char *indent, SoupMessageHeaders *headers)
{
    SoupMessageHeadersIter iter;
    const char *name, *value;
    if (!headers) {
        printf ("%s(no headers)\n", indent);
        return;
    }
    soup_message_headers_iter_init (&iter, headers);
    while (soup_message_headers_iter_next (&iter, &name, &value)) printf ("%s%s: %s\n", indent, name, value);
}

static void print_bytes (const char *what, const char *data, gsize size)
{
    printf ("%s (%zu): '", what, size);
    for (gsize i = 0; i < size; i++) {
        if (data[i] == '\r') printf ("\\r");
        else if (data[i] == '\n') printf ("\\n");
        else putchar (data[i]);
    }
    printf ("'\n");
}

static void parse (const char *content_type, const char *text)
{
    SoupMessageHeaders *headers = soup_message_headers_new (SOUP_MESSAGE_HEADERS_RESPONSE);
    GBytes *body = g_bytes_new (text, strlen (text));
    SoupMultipart *multipart;

    soup_message_headers_replace (headers, "Content-Type", content_type);
    multipart = soup_multipart_new_from_message (headers, body);
    printf ("parse %s: ", content_type);
    if (!multipart) {
        printf ("(none)\n");
    } else {
        int count = soup_multipart_get_length (multipart);
        printf ("%d parts\n", count);
        for (int i = 0; i <= count; i++) {
            SoupMessageHeaders *part_headers;
            GBytes *part_body;
            if (!soup_multipart_get_part (multipart, i, &part_headers, &part_body)) {
                printf ("  part %d: none\n", i);
                continue;
            }
            printf ("  part %d:\n", i);
            print_headers ("    ", part_headers);
            print_bytes ("    body", g_bytes_get_data (part_body, NULL), g_bytes_get_size (part_body));
        }
        soup_multipart_free (multipart);
    }
    g_bytes_unref (body);
    soup_message_headers_unref (headers);
}

static void build (void)
{
    SoupMessageHeaders *headers = soup_message_headers_new (SOUP_MESSAGE_HEADERS_RESPONSE);
    SoupMessageHeaders *dest = soup_message_headers_new (SOUP_MESSAGE_HEADERS_REQUEST);
    GBytes *body = g_bytes_new (body_text, strlen (body_text)), *out = NULL, *file;
    SoupMultipart *multipart, *copy, *fresh;
    const char *boundary;

    soup_message_headers_replace (headers, "Content-Type", "multipart/mixed; boundary=xyz");
    multipart = soup_multipart_new_from_message (headers, body);
    soup_multipart_append_form_string (multipart, "field", "value");
    soup_multipart_append_form_string (multipart, "quote\"d", "a \"quoted\" value");
    file = g_bytes_new_static ("file contents", 13);
    soup_multipart_append_form_file (multipart, "upload", "name.txt", "text/plain", file);
    soup_multipart_append_form_file (multipart, "anonymous", NULL, NULL, file);
    copy = g_boxed_copy (SOUP_TYPE_MULTIPART, multipart);
    soup_multipart_free (multipart);
    printf ("copy has %d parts\n", soup_multipart_get_length (copy));
    soup_multipart_to_message (copy, dest, &out);
    printf ("content type: %s\n", soup_message_headers_get_one (dest, "Content-Type"));
    print_bytes ("message", g_bytes_get_data (out, NULL), g_bytes_get_size (out));
    g_bytes_unref (out);
    soup_multipart_free (copy);

    /* A new one has a boundary of its own. */
    fresh = soup_multipart_new ("multipart/form-data");
    soup_multipart_append_form_string (fresh, "a", "b");
    soup_message_headers_clear (dest);
    soup_multipart_to_message (fresh, dest, &out);
    {
        GHashTable *params;
        const char *type = soup_message_headers_get_content_type (dest, &params);
        boundary = g_hash_table_lookup (params, "boundary");
        printf ("fresh: %s, boundary length %zu\n", type, strlen (boundary));
        g_hash_table_destroy (params);
    }
    g_bytes_unref (out);
    soup_multipart_free (fresh);
    g_bytes_unref (file);
    g_bytes_unref (body);
    soup_message_headers_unref (headers);
    soup_message_headers_unref (dest);
}

static SoupMessage *multipart_message (const char *content_type)
{
    SoupMessage *msg = soup_message_new ("GET", "http://example.com/");
    soup_message_headers_replace (soup_message_get_response_headers (msg), "Content-Type", content_type);
    return msg;
}

static void read_part (GInputStream *part)
{
    char buffer[4096];
    GString *all = g_string_new (NULL);
    gssize n;
    int reads = 0;
    while ((n = g_input_stream_read (part, buffer, sizeof buffer, NULL, NULL)) > 0) {
        g_string_append_len (all, buffer, n);
        reads++;
    }
    print_bytes ("    body", all->str, all->len);
    g_string_free (all, TRUE);
}

static void sync_parts (const char *content_type, const char *text)
{
    SoupMessage *msg = multipart_message (content_type);
    GInputStream *base = g_memory_input_stream_new_from_data (text, strlen (text), NULL);
    SoupMultipartInputStream *stream = soup_multipart_input_stream_new (msg, base);
    GInputStream *part;
    GError *error = NULL;
    int index = 0;

    printf ("stream %s:\n", content_type);
    while ((part = soup_multipart_input_stream_next_part (stream, NULL, &error))) {
        printf ("  part %d:\n", index++);
        print_headers ("    ", soup_multipart_input_stream_get_headers (stream));
        read_part (part);
        g_object_unref (part);
        if (index > 6) break;
    }
    printf ("  end%s%s\n", error ? ": " : "", error ? error->message : "");
    g_clear_error (&error);
    g_object_unref (stream);
    g_object_unref (base);
    g_object_unref (msg);
}

/* Asynchronously: each part's headers, then its body. (Reads are of a
 * buffer larger than the bodies: libsoup's reads of a part write a byte
 * past a smaller buffer when the stream holds more than fits.) */
typedef struct {
    SoupMultipartInputStream *stream;
    GInputStream *part;
    GMainLoop *loop;
    GString *body;
    char buffer[4096];
    int index;
} AsyncState;

static void next_part (AsyncState *state);

static void part_read (GObject *source, GAsyncResult *result, gpointer data)
{
    AsyncState *state = data;
    GError *error = NULL;
    gssize n = g_input_stream_read_finish (G_INPUT_STREAM (source), result, &error);
    if (n > 0) {
        g_string_append_len (state->body, state->buffer, n);
        g_input_stream_read_async (state->part, state->buffer, sizeof state->buffer, G_PRIORITY_DEFAULT, NULL, part_read, state);
        return;
    }
    if (error) printf ("    read error: %s\n", error->message);
    g_clear_error (&error);
    print_bytes ("    body", state->body->str, state->body->len);
    g_string_truncate (state->body, 0);
    g_object_unref (state->part);
    state->part = NULL;
    next_part (state);
}

static void got_part (GObject *source, GAsyncResult *result, gpointer data)
{
    AsyncState *state = data;
    GError *error = NULL;
    state->part = soup_multipart_input_stream_next_part_finish (state->stream, result, &error);
    if (!state->part) {
        printf ("  end%s%s\n", error ? ": " : "", error ? error->message : "");
        g_clear_error (&error);
        g_main_loop_quit (state->loop);
        return;
    }
    printf ("  part %d:\n", state->index++);
    print_headers ("    ", soup_multipart_input_stream_get_headers (state->stream));
    g_input_stream_read_async (state->part, state->buffer, sizeof state->buffer, G_PRIORITY_DEFAULT, NULL, part_read, state);
}

static void next_part (AsyncState *state)
{
    soup_multipart_input_stream_next_part_async (state->stream, G_PRIORITY_DEFAULT, NULL, got_part, state);
}

static void async_parts (SoupMessage *msg, GInputStream *base)
{
    AsyncState state = { 0 };
    state.stream = soup_multipart_input_stream_new (msg, base);
    state.loop = g_main_loop_new (NULL, FALSE);
    state.body = g_string_new (NULL);
    next_part (&state);
    g_main_loop_run (state.loop);
    g_main_loop_unref (state.loop);
    g_string_free (state.body, TRUE);
    g_object_unref (state.stream);
}

/* A server sending a multipart/x-mixed-replace response a piece at a
 * time. */
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
    listen (fd, 4);
    getsockname (fd, (struct sockaddr *) &address, &length);
    *child = fork ();
    if (*child == 0) {
        static const char *pieces[] = {
            "HTTP/1.1 200 OK\r\nContent-Type: multipart/x-mixed-replace; boundary=frame\r\nConnection: close\r\n\r\n",
            "--frame\r\nContent-Type: text/plain\r\n\r\nfirst ",
            "frame\r\n--fr",
            "ame\r\nContent-Type: text/plain\r\nContent-Length: 12\r\n\r\nsecond frame",
            "\r\n--frame\r\n",
            "Content-Type: text/x-third\r\n\r\nthird\r\n--frame--\r\n",
            NULL
        };
        char request[4096];
        int client;
        alarm (60);
        client = accept (fd, NULL, NULL);
        if (read (client, request, sizeof request) <= 0) _exit (1);
        for (int i = 0; pieces[i]; i++) {
            if (write (client, pieces[i], strlen (pieces[i])) < 0) _exit (1);
            usleep (30000);
        }
        close (client);
        _exit (0);
    }
    close (fd);
    return ntohs (address.sin_port);
}

int main (void)
{
    pid_t child;
    int port;
    char *uri;
    SoupSession *session;
    SoupMessage *msg;
    GInputStream *base;
    GError *error = NULL;

    setvbuf (stdout, NULL, _IOLBF, 0);
    printf ("type %s\n", g_type_name (SOUP_TYPE_MULTIPART));
    parse ("multipart/mixed; boundary=xyz", body_text);
    parse ("multipart/mixed; boundary=\"xyz\"", "--xyz\r\nA: b\r\n\r\nonly\r\n--xyz--");
    parse ("multipart/mixed; boundary=xyz", "--xyz\r\nA: b\r\n\r\nunterminated");
    parse ("multipart/mixed; boundary=xyz", "no boundary at all");
    parse ("multipart/mixed; boundary=xyz", "--xyz\r\nno blank line\r\n--xyz--\r\n");
    parse ("multipart/mixed", body_text);
    parse ("text/plain; boundary=xyz", body_text);
    parse ("multipart/mixed; boundary=xyz", "x--xyz\r\n\r\nnot at line start\r\n--xyz\r\n\r\nreal\r\n--xyz--\r\n");
    build ();

    sync_parts ("multipart/mixed; boundary=xyz", body_text);
    sync_parts ("multipart/mixed; boundary=--xyz", body_text);
    sync_parts ("multipart/mixed; boundary=b", "--b\nA: 1\n\none\n--b\n\ntwo\n--b--\n");
    sync_parts ("multipart/mixed; boundary=xyz", "--xyz\r\n\r\nunterminated part");

    printf ("async from memory:\n");
    msg = multipart_message ("multipart/mixed; boundary=xyz");
    base = g_memory_input_stream_new_from_data (body_text, strlen (body_text), NULL);
    async_parts (msg, base);
    g_object_unref (base);
    g_object_unref (msg);

    port = start_server (&child);
    uri = g_strdup_printf ("http://127.0.0.1:%d/", port);
    session = soup_session_new ();
    msg = soup_message_new ("GET", uri);
    base = soup_session_send (session, msg, NULL, &error);
    printf ("async from a server: %u %s\n", soup_message_get_status (msg), soup_message_headers_get_content_type (soup_message_get_response_headers (msg), NULL));
    if (base) {
        async_parts (msg, base);
        g_object_unref (base);
    } else {
        printf ("  error: %s\n", error->message);
        g_clear_error (&error);
    }
    g_object_unref (msg);
    g_object_unref (session);
    g_free (uri);
    waitpid (child, NULL, 0);
    return 0;
}
