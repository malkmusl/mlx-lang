/* HTTP Strict Transport Security: policies made and read from response
 * headers, an enforcer's policies for domains and their subdomains, the
 * "changed" signal, messages to known hosts made secure (and the
 * "hsts-enforced" signal), a database enforcer written and read again.
 * Each line printed is compared with what the system's libsoup prints. */
#include <libsoup/soup.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void show (const char *what, SoupHSTSPolicy *policy)
{
    printf ("%s: ", what);
    if (!policy) {
        printf ("(none)\n");
        return;
    }
    printf ("%s max-age %lu subdomains %d session %d expired %d expires %d\n", soup_hsts_policy_get_domain (policy),
            soup_hsts_policy_get_max_age (policy), soup_hsts_policy_includes_subdomains (policy),
            soup_hsts_policy_is_session_policy (policy), soup_hsts_policy_is_expired (policy),
            soup_hsts_policy_get_expires (policy) != NULL);
}

static void from_header (const char *uri, const char *value)
{
    SoupMessage *msg = soup_message_new ("GET", uri);
    SoupHSTSPolicy *policy;
    char what[256];

    if (value) soup_message_headers_append (soup_message_get_response_headers (msg), "Strict-Transport-Security", value);
    policy = soup_hsts_policy_new_from_response (msg);
    snprintf (what, sizeof what, "from %s '%s'", uri, value ? value : "");
    show (what, policy);
    if (policy) soup_hsts_policy_free (policy);
    g_object_unref (msg);
}

static void on_changed (SoupHSTSEnforcer *enforcer, SoupHSTSPolicy *old, SoupHSTSPolicy *new, gpointer data)
{
    printf ("  changed: %s -> %s\n", old ? soup_hsts_policy_get_domain (old) : "(none)", new ? soup_hsts_policy_get_domain (new) : "(none)");
}

static void valid (SoupHSTSEnforcer *enforcer, const char *domain)
{
    printf ("valid %s: %d\n", domain, soup_hsts_enforcer_has_valid_policy (enforcer, domain));
}

static int compare_text (gconstpointer a, gconstpointer b)
{
    return strcmp (a, b);
}

static int compare_policies (gconstpointer a, gconstpointer b)
{
    return strcmp (soup_hsts_policy_get_domain ((SoupHSTSPolicy *) a), soup_hsts_policy_get_domain ((SoupHSTSPolicy *) b));
}

static void list (SoupHSTSEnforcer *enforcer)
{
    GList *domains = g_list_sort (soup_hsts_enforcer_get_domains (enforcer, TRUE), compare_text), *l;
    GList *policies = g_list_sort (soup_hsts_enforcer_get_policies (enforcer, FALSE), compare_policies);

    printf ("domains:");
    for (l = domains; l; l = l->next) printf (" %s", (char *) l->data);
    printf ("\n");
    g_list_free_full (domains, g_free);
    for (l = policies; l; l = l->next) show ("  permanent", l->data);
    g_list_free_full (policies, (GDestroyNotify) soup_hsts_policy_free);
}

static void on_enforced (SoupMessage *msg, gpointer data)
{
    GUri *uri = soup_message_get_uri (msg);
    printf ("  hsts-enforced: %s://%s\n", g_uri_get_scheme (uri), g_uri_get_host (uri));
}

/* A message to a port nothing listens on: its URI after it is sent, and its
 * error. */
static void send_to (SoupSession *session, const char *host, int port)
{
    char uri[256];
    char *now;
    GError *error = NULL;
    SoupMessage *msg;
    GInputStream *stream;

    snprintf (uri, sizeof uri, "http://%s:%d/", host, port);
    msg = soup_message_new ("GET", uri);
    g_signal_connect (msg, "hsts-enforced", G_CALLBACK (on_enforced), NULL);
    stream = soup_session_send (session, msg, NULL, &error);
    now = g_strdup_printf ("%s://%s", g_uri_get_scheme (soup_message_get_uri (msg)), g_uri_get_host (soup_message_get_uri (msg)));
    printf ("send %s: now %s, %s", host, now, stream ? "stream" : "error");
    if (error) printf (" %s %d", g_quark_to_string (error->domain), error->code);
    printf ("\n");
    g_free (now);
    g_clear_error (&error);
    if (stream) g_object_unref (stream);
    g_object_unref (msg);
}

int main (void)
{
    SoupHSTSEnforcer *enforcer;
    SoupHSTSPolicy *policy, *copy;
    SoupSession *session;
    struct sockaddr_in address;
    socklen_t length = sizeof address;
    int fd, port;
    char path[256];

    setvbuf (stdout, NULL, _IOLBF, 0);

    policy = soup_hsts_policy_new ("example.com", 3600, TRUE);
    show ("new", policy);
    copy = soup_hsts_policy_copy (policy);
    printf ("copy equal %d\n", soup_hsts_policy_equal (policy, copy));
    soup_hsts_policy_free (copy);
    copy = soup_hsts_policy_new ("example.com", 3600, FALSE);
    printf ("other equal %d\n", soup_hsts_policy_equal (policy, copy));
    soup_hsts_policy_free (copy);
    soup_hsts_policy_free (policy);
    policy = soup_hsts_policy_new_session_policy ("session.example", FALSE);
    show ("session", policy);
    soup_hsts_policy_free (policy);
    policy = soup_hsts_policy_new ("gone.example", 0, FALSE);
    show ("zero", policy);
    soup_hsts_policy_free (policy);
    policy = soup_hsts_policy_new_full ("full.example", 100, g_date_time_new_from_unix_utc (1000), FALSE);
    show ("full", policy);
    soup_hsts_policy_free (policy);

    from_header ("https://www.example.com/", "max-age=31536000");
    from_header ("https://www.example.com/", "max-age=31536000; includeSubDomains");
    from_header ("https://www.example.com/", "max-age=600; includeSubDomains; preload");
    from_header ("https://www.example.com/", "includeSubDomains");
    from_header ("https://www.example.com/", "max-age=abc");
    from_header ("https://www.example.com/", "max-age=10; includeSubDomains=yes");
    from_header ("https://www.example.com/", "max-age=10; max-age=20");
    from_header ("https://www.example.com/", "max-age=\"30\"");
    from_header ("https://www.example.com/", "max-age=0");
    from_header ("https://www.example.com/", NULL);
    from_header ("http://plain.example.com/", "max-age=10");

    enforcer = soup_hsts_enforcer_new ();
    g_signal_connect (enforcer, "changed", G_CALLBACK (on_changed), NULL);
    printf ("persistent %d\n", soup_hsts_enforcer_is_persistent (enforcer));
    policy = soup_hsts_policy_new ("example.com", 3600, TRUE);
    soup_hsts_enforcer_set_policy (enforcer, policy);
    soup_hsts_policy_free (policy);
    policy = soup_hsts_policy_new ("only.org", 3600, FALSE);
    soup_hsts_enforcer_set_policy (enforcer, policy);
    soup_hsts_enforcer_set_policy (enforcer, policy);
    soup_hsts_policy_free (policy);
    soup_hsts_enforcer_set_session_policy (enforcer, "session.net", TRUE);
    valid (enforcer, "example.com");
    valid (enforcer, "www.example.com");
    valid (enforcer, "a.b.example.com");
    valid (enforcer, "only.org");
    valid (enforcer, "sub.only.org");
    valid (enforcer, "session.net");
    valid (enforcer, "x.session.net");
    valid (enforcer, "other.com");
    list (enforcer);
    policy = soup_hsts_policy_new ("example.com", 0, FALSE);
    soup_hsts_enforcer_set_policy (enforcer, policy);
    soup_hsts_policy_free (policy);
    valid (enforcer, "www.example.com");
    list (enforcer);

    /* Messages to known hosts become secure. */
    fd = socket (AF_INET, SOCK_STREAM, 0);
    memset (&address, 0, sizeof address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
    bind (fd, (struct sockaddr *) &address, sizeof address);
    getsockname (fd, (struct sockaddr *) &address, &length);
    port = ntohs (address.sin_port);
    close (fd);
    session = soup_session_new ();
    soup_session_add_feature (session, SOUP_SESSION_FEATURE (enforcer));
    soup_hsts_enforcer_set_session_policy (enforcer, "localhost", FALSE);
    send_to (session, "localhost", port);
    send_to (session, "127.0.0.1", port);
    printf ("feature %d\n", soup_session_has_feature (session, SOUP_TYPE_HSTS_ENFORCER));
    g_object_unref (session);
    g_object_unref (enforcer);

    /* A database, written and read again. */
    snprintf (path, sizeof path, "/tmp/mlx-soup-hsts-%d.db", (int) getpid ());
    unlink (path);
    enforcer = soup_hsts_enforcer_db_new (path);
    printf ("db persistent %d\n", soup_hsts_enforcer_is_persistent (enforcer));
    policy = soup_hsts_policy_new ("kept.example", 3600, TRUE);
    soup_hsts_enforcer_set_policy (enforcer, policy);
    soup_hsts_policy_free (policy);
    policy = soup_hsts_policy_new ("also.example", 7200, FALSE);
    soup_hsts_enforcer_set_policy (enforcer, policy);
    soup_hsts_policy_free (policy);
    soup_hsts_enforcer_set_session_policy (enforcer, "session.example", FALSE);
    g_object_unref (enforcer);
    enforcer = soup_hsts_enforcer_db_new (path);
    valid (enforcer, "x.kept.example");
    valid (enforcer, "also.example");
    valid (enforcer, "session.example");
    list (enforcer);
    {
        gchar *filename = NULL;
        g_object_get (enforcer, "filename", &filename, NULL);
        printf ("filename matches %d\n", filename && strcmp (filename, path) == 0);
        g_free (filename);
    }
    g_object_unref (enforcer);
    unlink (path);
    return 0;
}
