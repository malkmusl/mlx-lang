/* mlx-glib's TLS connections against OpenSSL's s_server on this machine:
   a GTlsClientConnection over a GSocketConnection to 127.0.0.1, its
   database a GTlsFileDatabase of the test's CA, handshakes synchronous
   and asynchronous (in a main loop, the streams read without blocking
   as libsoup reads them), the peer's certificate and its fields, the
   negotiated protocol, version and suite, "accept-certificate" with a
   name that is not the certificate's (refused, then accepted), and a
   TLS 1.2 server.

   tools/check_glib.sh runs it with s_server answering on PORT (TLS 1.3)
   and PORT12 (TLS 1.2 only), with the certificates of
   tools/tls-test/make_certs.sh in CERTS, and compares what it prints with
   tls_handshake.expected, which is what it prints on GLib 2.80 with
   glib-networking's GnuTLS backend. The certificates are made anew each
   run, so their dates and serial numbers are not printed.

     tls_handshake PORT PORT12 CERTS */
#include <gio/gio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int port, port12;
static const char *certs;
static GTlsDatabase *database;

static void error(const char *what, GError **e)
{
    if (*e) {
        printf("%s: %s %d %s\n", what, g_quark_to_string((*e)->domain), (*e)->code, (*e)->message);
        g_clear_error(e);
    } else {
        printf("%s: no error\n", what);
    }
}

static const char *version_name(GTlsProtocolVersion v)
{
    switch (v) {
    case G_TLS_PROTOCOL_VERSION_TLS_1_2: return "TLS 1.2";
    case G_TLS_PROTOCOL_VERSION_TLS_1_3: return "TLS 1.3";
    case G_TLS_PROTOCOL_VERSION_UNKNOWN: return "unknown";
    default: return "other";
    }
}

static void describe(GTlsConnection *tls)
{
    GTlsCertificate *peer = g_tls_connection_get_peer_certificate(tls);
    printf("version %s, suite %s\n", version_name(g_tls_connection_get_protocol_version(tls)), g_tls_connection_get_ciphersuite_name(tls));
    printf("negotiated protocol %s\n", g_tls_connection_get_negotiated_protocol(tls) ? g_tls_connection_get_negotiated_protocol(tls) : "(none)");
    printf("peer errors %d\n", g_tls_connection_get_peer_certificate_errors(tls));
    if (!peer) {
        printf("no peer certificate\n");
        return;
    }
    gchar *subject = NULL, *issuer = NULL;
    GPtrArray *names = NULL, *addresses = NULL;
    GDateTime *before = NULL, *after = NULL;
    g_object_get(peer, "subject-name", &subject, "issuer-name", &issuer, "dns-names", &names, "ip-addresses", &addresses,
                 "not-valid-before", &before, "not-valid-after", &after, NULL);
    printf("subject %s\nissuer %s\n", subject, issuer);
    printf("dns names");
    for (guint i = 0; names && i < names->len; i++) {
        GBytes *name = g_ptr_array_index(names, i);
        gsize length;
        const char *data = g_bytes_get_data(name, &length);
        printf(" %.*s", (int)length, data);
    }
    printf("\naddresses");
    for (guint i = 0; addresses && i < addresses->len; i++) {
        gchar *text = g_inet_address_to_string(g_ptr_array_index(addresses, i));
        printf(" %s", text);
        g_free(text);
    }
    printf("\nvalid for %d days\n", before && after ? (int)(g_date_time_difference(after, before) / G_TIME_SPAN_DAY) : -1);
    GTlsCertificate *issuer_cert = g_tls_certificate_get_issuer(peer);
    printf("issuer certificate %d\n", issuer_cert != NULL);
    printf("verified with the database %d\n", g_tls_database_verify_chain(database, peer, G_TLS_DATABASE_PURPOSE_AUTHENTICATE_SERVER, NULL, NULL, G_TLS_DATABASE_VERIFY_NONE, NULL, NULL));
    g_free(subject);
    g_free(issuer);
    if (names) g_ptr_array_unref(names);
    if (addresses) g_ptr_array_unref(addresses);
    if (before) g_date_time_unref(before);
    if (after) g_date_time_unref(after);
}

static gboolean accept_answer;

static gboolean on_accept(GTlsConnection *tls, GTlsCertificate *peer, GTlsCertificateFlags errors, gpointer data)
{
    printf("accept-certificate: errors %d, peer %d, answer %d\n", errors, G_IS_TLS_CERTIFICATE(peer), accept_answer);
    return accept_answer;
}

static GIOStream *connect_to(int which, const char *address, GError **e)
{
    GSocketClient *client = g_socket_client_new();
    gchar *host = g_strdup_printf("127.0.0.1:%d", which);
    GSocketConnection *connection = g_socket_client_connect_to_host(client, host, 0, NULL, e);
    g_free(host);
    g_object_unref(client);
    if (!connection)
        return NULL;
    GSocketConnectable *identity = G_SOCKET_CONNECTABLE(g_inet_socket_address_new_from_string(address, which));
    GIOStream *tls = g_tls_client_connection_new(G_IO_STREAM(connection), identity, e);
    g_object_unref(identity);
    g_object_unref(connection);
    if (!tls)
        return NULL;
    g_tls_connection_set_database(G_TLS_CONNECTION(tls), database);
    const gchar *protocols[] = { "h2", "http/1.1", NULL };
    g_tls_connection_set_advertised_protocols(G_TLS_CONNECTION(tls), protocols);
    g_signal_connect(tls, "accept-certificate", G_CALLBACK(on_accept), NULL);
    return tls;
}

/* The request, and the answer's first line. */
static void request(GIOStream *tls)
{
    GError *e = NULL;
    const char *text = "GET / HTTP/1.0\r\n\r\n";
    gsize written = 0;
    gboolean ok = g_output_stream_write_all(g_io_stream_get_output_stream(tls), text, strlen(text), &written, NULL, &e);
    printf("request written %d, %d bytes\n", ok, (int)written);
    if (e) error("write", &e);
    char buffer[65536];
    gsize total = 0;
    for (;;) {
        gssize count = g_input_stream_read(g_io_stream_get_input_stream(tls), buffer + total, sizeof buffer - 1 - total, NULL, &e);
        if (count <= 0)
            break;
        total += count;
    }
    if (e) error("read", &e);
    buffer[total] = 0;
    char *end = strstr(buffer, "\r\n");
    printf("answer %.*s, %s\n", end ? (int)(end - buffer) : 0, buffer, total > 1000 ? "more than 1000 bytes" : "short");
}

static void synchronous(void)
{
    GError *e = NULL;
    GIOStream *tls = connect_to(port, "127.0.0.1", &e);
    error("connect", &e);
    if (!tls)
        return;
    gboolean done = g_tls_connection_handshake(G_TLS_CONNECTION(tls), NULL, &e);
    printf("handshake %d\n", done);
    error("handshake", &e);
    describe(G_TLS_CONNECTION(tls));
    request(tls);
    printf("close %d\n", g_io_stream_close(tls, NULL, &e));
    error("close", &e);
    g_object_unref(tls);
}

/* Asynchronously, and read as libsoup reads: without blocking, waiting
   on the pollable stream's source. */
typedef struct {
    GMainLoop *loop;
    GIOStream *tls;
    char buffer[65536];
    gsize total;
    int waits;
} Async;

static gboolean readable(GObject *stream, gpointer data)
{
    Async *a = data;
    GError *e = NULL;
    for (;;) {
        gssize count = g_pollable_input_stream_read_nonblocking(G_POLLABLE_INPUT_STREAM(stream), a->buffer + a->total, sizeof a->buffer - 1 - a->total, NULL, &e);
        if (count < 0 && g_error_matches(e, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK)) {
            g_clear_error(&e);
            a->waits++;
            return G_SOURCE_CONTINUE;
        }
        if (count <= 0) {
            if (e) error("read", &e);
            a->buffer[a->total] = 0;
            char *end = strstr(a->buffer, "\r\n");
            printf("answer %.*s, %s\n", end ? (int)(end - a->buffer) : 0, a->buffer, a->total > 1000 ? "more than 1000 bytes" : "short");
            g_main_loop_quit(a->loop);
            return G_SOURCE_REMOVE;
        }
        a->total += count;
    }
}

static void handshaken(GObject *source, GAsyncResult *result, gpointer data)
{
    Async *a = data;
    GError *e = NULL;
    gboolean done = g_tls_connection_handshake_finish(G_TLS_CONNECTION(source), result, &e);
    printf("async handshake %d\n", done);
    error("async handshake", &e);
    if (!done) {
        g_main_loop_quit(a->loop);
        return;
    }
    describe(G_TLS_CONNECTION(source));
    const char *text = "GET / HTTP/1.0\r\n\r\n";
    gsize written = 0;
    g_output_stream_write_all(g_io_stream_get_output_stream(a->tls), text, strlen(text), &written, NULL, &e);
    error("write", &e);
    GInputStream *input = g_io_stream_get_input_stream(a->tls);
    printf("pollable %d, can poll %d\n", G_IS_POLLABLE_INPUT_STREAM(input), g_pollable_input_stream_can_poll(G_POLLABLE_INPUT_STREAM(input)));
    GSource *wait = g_pollable_input_stream_create_source(G_POLLABLE_INPUT_STREAM(input), NULL);
    g_source_set_callback(wait, (GSourceFunc)readable, a, NULL);
    g_source_attach(wait, NULL);
    g_source_unref(wait);
}

static void asynchronous(int which, const char *address, gboolean answer)
{
    GError *e = NULL;
    Async *a = g_new0(Async, 1);
    a->loop = g_main_loop_new(NULL, FALSE);
    a->tls = connect_to(which, address, &e);
    error("connect", &e);
    if (a->tls) {
        accept_answer = answer;
        g_tls_connection_handshake_async(G_TLS_CONNECTION(a->tls), G_PRIORITY_DEFAULT, NULL, handshaken, a);
        g_main_loop_run(a->loop);
        printf("close %d\n", g_io_stream_close(a->tls, NULL, &e));
        error("close", &e);
        g_object_unref(a->tls);
    }
    g_main_loop_unref(a->loop);
    g_free(a);
}

/* A name that is not the certificate's: refused by accept-certificate. */
static void refused(void)
{
    GError *e = NULL;
    GIOStream *tls = connect_to(port, "127.0.0.2", &e);
    error("connect", &e);
    if (!tls)
        return;
    accept_answer = FALSE;
    gboolean done = g_tls_connection_handshake(G_TLS_CONNECTION(tls), NULL, &e);
    printf("handshake %d\n", done);
    error("handshake", &e);
    printf("peer errors %d\n", g_tls_connection_get_peer_certificate_errors(G_TLS_CONNECTION(tls)));
    g_io_stream_close(tls, NULL, NULL);
    g_object_unref(tls);
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        printf("usage: tls_handshake PORT PORT12 CERTS\n");
        return 2;
    }
    port = atoi(argv[1]);
    port12 = atoi(argv[2]);
    certs = argv[3];
    printf("supports tls %d\n", g_tls_backend_supports_tls(g_tls_backend_get_default()));
    GError *e = NULL;
    gchar *anchors = g_build_filename(certs, "ca.pem", NULL);
    database = g_tls_file_database_new(anchors, &e);
    g_free(anchors);
    error("file database", &e);
    if (!database)
        return 1;
    printf("-- synchronous, TLS 1.3\n");
    synchronous();
    printf("-- asynchronous, TLS 1.3\n");
    asynchronous(port, "127.0.0.1", FALSE);
    printf("-- another name, refused\n");
    refused();
    printf("-- another name, accepted, TLS 1.2\n");
    asynchronous(port12, "127.0.0.2", TRUE);
    g_object_unref(database);
    return 0;
}
