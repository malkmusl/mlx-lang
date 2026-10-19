/* HTTPS: requests to two openssl s_servers on 127.0.0.1 (TLS 1.3 and TLS
 * 1.2, the certificates of tools/tls-test/make_certs.sh), the server's
 * chain checked against a database of the test's root, by name and by
 * address; a root the session does not know refused, or accepted by
 * "accept-certificate"; a name the certificate is not for; synchronous and
 * asynchronous sends. Arguments: the two ports and the certificates'
 * directory. Each line printed is compared with what the system's libsoup
 * (with glib-networking) prints. */
#include <libsoup/soup.h>
#include <stdio.h>
#include <string.h>

static int accept_count;

static gboolean on_accept (SoupMessage *msg, GTlsCertificate *certificate, GTlsCertificateFlags errors, gpointer data)
{
    accept_count++;
    printf ("  accept-certificate: errors 0x%x, certificate %d\n", errors, certificate != NULL);
    return GPOINTER_TO_INT (data);
}

static void show (const char *what, SoupMessage *msg, GBytes *body, GError *error)
{
    printf ("%s: ", what);
    if (error) {
        printf ("error %s %d: %s\n", g_quark_to_string (error->domain), error->code, error->message);
        return;
    }
    printf ("%u %s, errors 0x%x, version %d, peer certificate %d, body %.6s\n", soup_message_get_status (msg),
            soup_message_get_reason_phrase (msg), soup_message_get_tls_peer_certificate_errors (msg),
            soup_message_get_tls_protocol_version (msg), soup_message_get_tls_peer_certificate (msg) != NULL,
            body ? (const char *) g_bytes_get_data (body, NULL) : "(none)");
}

static void get (SoupSession *session, const char *what, const char *uri, int accept)
{
    SoupMessage *msg = soup_message_new ("GET", uri);
    GError *error = NULL;
    GBytes *body;
    if (accept >= 0) g_signal_connect (msg, "accept-certificate", G_CALLBACK (on_accept), GINT_TO_POINTER (accept));
    body = soup_session_send_and_read (session, msg, NULL, &error);
    show (what, msg, body, error);
    g_clear_error (&error);
    if (body) g_bytes_unref (body);
    g_object_unref (msg);
}

typedef struct {
    GMainLoop *loop;
    SoupMessage *msg;
} Async;

static void read_done (GObject *source, GAsyncResult *result, gpointer data)
{
    Async *async = data;
    GError *error = NULL;
    GBytes *body = soup_session_send_and_read_finish (SOUP_SESSION (source), result, &error);
    show ("async", async->msg, body, error);
    g_clear_error (&error);
    if (body) g_bytes_unref (body);
    g_main_loop_quit (async->loop);
}

int main (int argc, char **argv)
{
    char uri[256], path[512];
    SoupSession *session, *plain;
    GTlsDatabase *database;
    GError *error = NULL;
    Async async;

    setvbuf (stdout, NULL, _IOLBF, 0);
    if (argc < 4) return 2;
    snprintf (path, sizeof path, "%s/ca.pem", argv[3]);
    database = g_tls_file_database_new (path, &error);
    if (!database) {
        printf ("database: %s\n", error->message);
        return 1;
    }
    session = soup_session_new ();
    soup_session_set_tls_database (session, database);

    snprintf (uri, sizeof uri, "https://localhost:%s/", argv[1]);
    get (session, "TLS 1.3 by name", uri, -1);
    snprintf (uri, sizeof uri, "https://127.0.0.1:%s/", argv[1]);
    get (session, "TLS 1.3 by address", uri, -1);
    snprintf (uri, sizeof uri, "https://localhost:%s/", argv[2]);
    get (session, "TLS 1.2", uri, -1);
    snprintf (uri, sizeof uri, "https://127.0.0.2:%s/", argv[1]);
    get (session, "another address, refused", uri, 0);
    get (session, "another address, accepted", uri, 1);

    async.loop = g_main_loop_new (NULL, FALSE);
    snprintf (uri, sizeof uri, "https://localhost:%s/", argv[1]);
    async.msg = soup_message_new ("GET", uri);
    soup_session_send_and_read_async (session, async.msg, G_PRIORITY_DEFAULT, NULL, read_done, &async);
    g_main_loop_run (async.loop);
    g_object_unref (async.msg);
    g_main_loop_unref (async.loop);

    /* The system's roots do not have the test's. */
    plain = soup_session_new ();
    snprintf (uri, sizeof uri, "https://localhost:%s/", argv[1]);
    get (plain, "system roots", uri, -1);
    get (plain, "system roots, accepted", uri, 1);
    printf ("accept-certificate emitted %d times\n", accept_count);

    g_object_unref (plain);
    g_object_unref (session);
    g_object_unref (database);
    return 0;
}
