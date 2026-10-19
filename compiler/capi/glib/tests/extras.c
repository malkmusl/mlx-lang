/* The pieces of GLib and GIO libsoup stands on beyond the rest: keyed
 * data lists, the global random numbers, the user's languages, host
 * names in ASCII, GIO's enumeration types and proxy resolvers. Each line
 * printed is what GLib prints for the same calls. */
#include <gio/gio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int destroyed;

static void destroy_note (gpointer data) { destroyed++; printf ("destroy %s\n", (const char *) data); }

static void each (GQuark key, gpointer data, gpointer user_data)
{
    printf ("  %s = %s\n", g_quark_to_string (key), (const char *) data);
    (*(int *) user_data)++;
}

static void datalists (void)
{
    GData *list;
    int count = 0;
    g_datalist_init (&list);
    printf ("empty %p\n", g_datalist_get_data (&list, "a"));
    g_datalist_id_set_data_full (&list, g_quark_from_static_string ("a"), "one", destroy_note);
    g_datalist_id_set_data_full (&list, g_quark_from_static_string ("b"), "two", NULL);
    g_datalist_id_set_data_full (&list, g_quark_from_static_string ("c"), "three", destroy_note);
    printf ("a %s\n", (const char *) g_datalist_get_data (&list, "a"));
    printf ("c %s\n", (const char *) g_datalist_id_get_data (&list, g_quark_from_static_string ("c")));
    printf ("missing %p\n", g_datalist_get_data (&list, "zzz"));
    g_datalist_id_set_data_full (&list, g_quark_from_static_string ("a"), "uno", NULL);
    printf ("a %s destroyed %d\n", (const char *) g_datalist_get_data (&list, "a"), destroyed);
    printf ("removed %s\n", (const char *) g_datalist_id_remove_no_notify (&list, g_quark_from_static_string ("c")));
    printf ("destroyed %d\n", destroyed);
    g_datalist_foreach (&list, each, &count);
    printf ("count %d\n", count);
    g_datalist_id_set_data_full (&list, g_quark_from_static_string ("d"), "four", destroy_note);
    g_datalist_clear (&list);
    printf ("cleared %p destroyed %d\n", (void *) list, destroyed);
}

static void randoms (void)
{
    int in_range = 1, i;
    double d;
    for (i = 0; i < 1000; i++) {
        gint32 n = g_random_int_range (-5, 7);
        if (n < -5 || n >= 7) in_range = 0;
        d = g_random_double ();
        if (d < 0.0 || d >= 1.0) in_range = 0;
        d = g_random_double_range (2.0, 3.0);
        if (d < 2.0 || d >= 3.0) in_range = 0;
    }
    printf ("random in range %d\n", in_range);
}

static void languages (const char *language, const char *all, const char *lang)
{
    const gchar * const *names;
    if (language) g_setenv ("LANGUAGE", language, TRUE); else g_unsetenv ("LANGUAGE");
    if (all) g_setenv ("LC_ALL", all, TRUE); else g_unsetenv ("LC_ALL");
    g_unsetenv ("LC_MESSAGES");
    if (lang) g_setenv ("LANG", lang, TRUE); else g_unsetenv ("LANG");
    names = g_get_language_names ();
    printf ("languages:");
    for (; *names; names++) printf (" %s", *names);
    printf ("\n");
}

static void hostname (const char *name)
{
    gchar *ascii = g_hostname_to_ascii (name);
    printf ("ascii %s -> %s\n", name, ascii ? ascii : "(null)");
    g_free (ascii);
}

static void enum_type (GType type, int value)
{
    GEnumClass *class = g_type_class_ref (type);
    GEnumValue *v = g_enum_get_value (class, value);
    printf ("%s %d: %s %s (%d values)\n", g_type_name (type), value, v->value_name, v->value_nick, class->n_values);
    g_type_class_unref (class);
}

static void print_proxies (const char *what, gchar **proxies, GError *error)
{
    printf ("%s:", what);
    if (!proxies) {
        printf (" error %s\n", error->message);
        g_error_free (error);
        return;
    }
    for (gchar **p = proxies; *p; p++) printf (" %s", *p);
    printf ("\n");
    g_strfreev (proxies);
}

static void lookup (GProxyResolver *resolver, const char *uri)
{
    GError *error = NULL;
    gchar **proxies = g_proxy_resolver_lookup (resolver, uri, NULL, &error);
    print_proxies (uri, proxies, error);
}

static void looked_up (GObject *source, GAsyncResult *result, gpointer data)
{
    GError *error = NULL;
    gchar **proxies = g_proxy_resolver_lookup_finish (G_PROXY_RESOLVER (source), result, &error);
    print_proxies ("async", proxies, error);
    g_main_loop_quit (data);
}

static void proxies (void)
{
    GProxyResolver *resolver = g_proxy_resolver_get_default ();
    const gchar *ignore[] = { "localhost", ".example.com", "10.0.0.0/8", "::1", "*.local", "host.test:8080", NULL };
    GMainLoop *loop;
    printf ("default is a resolver %d, same %d\n", G_IS_PROXY_RESOLVER (resolver), resolver == g_proxy_resolver_get_default ());
    resolver = g_simple_proxy_resolver_new ("http://proxy.test:3128", (gchar **) ignore);
    /* (GLib's simple resolver has no is_supported to call.) */
    printf ("simple: a %s\n", G_OBJECT_TYPE_NAME (resolver));
    lookup (resolver, "http://www.gnome.org/");
    lookup (resolver, "http://localhost/");
    lookup (resolver, "https://www.example.com/x");
    lookup (resolver, "http://example.com/");
    lookup (resolver, "http://10.2.3.4:81/");
    lookup (resolver, "http://11.2.3.4/");
    lookup (resolver, "http://[::1]/");
    lookup (resolver, "http://host.test:8080/");
    lookup (resolver, "http://host.test:8081/");
    lookup (resolver, "not a uri");
    g_simple_proxy_resolver_set_uri_proxy (G_SIMPLE_PROXY_RESOLVER (resolver), "ftp", "socks://socks.test:1080");
    lookup (resolver, "ftp://files.test/");
    g_simple_proxy_resolver_set_default_proxy (G_SIMPLE_PROXY_RESOLVER (resolver), NULL);
    lookup (resolver, "http://www.gnome.org/");
    {
        gchar *proxy = NULL;
        gchar **hosts = NULL;
        g_simple_proxy_resolver_set_default_proxy (G_SIMPLE_PROXY_RESOLVER (resolver), "http://other.test");
        g_object_get (resolver, "default-proxy", &proxy, "ignore-hosts", &hosts, NULL);
        printf ("properties %s %u\n", proxy, g_strv_length (hosts));
        g_free (proxy);
        g_strfreev (hosts);
    }
    loop = g_main_loop_new (NULL, FALSE);
    g_proxy_resolver_lookup_async (resolver, "http://www.gnome.org/", NULL, looked_up, loop);
    g_main_loop_run (loop);
    g_main_loop_unref (loop);
    g_object_unref (resolver);
}

static void network_address (void)
{
    GSocketConnectable *address = g_network_address_new ("www.example.com", 443);
    gchar *host = NULL;
    guint port = 0;
    g_object_get (address, "hostname", &host, "port", &port, NULL);
    printf ("network address %s %u, %s %u, a connectable %d, %s\n",
            g_network_address_get_hostname (G_NETWORK_ADDRESS (address)),
            g_network_address_get_port (G_NETWORK_ADDRESS (address)),
            host, port, G_IS_SOCKET_CONNECTABLE (address), G_OBJECT_TYPE_NAME (address));
    g_free (host);
    g_object_unref (address);
}

static void print_addresses (const char *what, GList *addresses, GError **errorp)
{
    /* The error is read here, after the lookup the arguments made. */
    GError *error = *errorp;
    printf ("%s:", what);
    if (error) {
        printf (" error %s %d\n", g_quark_to_string (error->domain), error->code);
        g_error_free (error);
        return;
    }
    for (GList *l = addresses; l; l = l->next) {
        gchar *text = g_inet_address_to_string (l->data);
        printf (" %s", text);
        g_free (text);
    }
    printf ("\n");
    g_resolver_free_addresses (addresses);
}

static void resolved (GObject *source, GAsyncResult *result, gpointer data)
{
    GError *error = NULL;
    GList *addresses = g_resolver_lookup_by_name_with_flags_finish (G_RESOLVER (source), result, &error);
    print_addresses ("async 127.0.0.2", addresses, &error);
    g_main_loop_quit (data);
}

static void named (GObject *source, GAsyncResult *result, gpointer data)
{
    GError *error = NULL;
    gchar *name = g_resolver_lookup_by_address_finish (G_RESOLVER (source), result, &error);
    /* Whether ::1 has a name is the system's: either answer will do. */
    printf ("async name of ::1: %s\n", (name != NULL) != (error != NULL) ? "a name or an error" : "both or neither");
    g_clear_error (&error);
    g_free (name);
    g_main_loop_quit (data);
}

/* A resolver of its own, as WebKit's cache of names is: the class's
   functions answer and GResolver's only call them. */
typedef GResolver FixedResolver;
typedef GResolverClass FixedResolverClass;
static void fixed_resolver_class_init (FixedResolverClass *klass);
static void fixed_resolver_init (FixedResolver *resolver);
static GType fixed_resolver_get_type (void)
{
    static GType type;
    if (!type)
        type = g_type_register_static_simple (G_TYPE_RESOLVER, "FixedResolver", sizeof (FixedResolverClass),
                                              (GClassInitFunc) fixed_resolver_class_init, sizeof (FixedResolver),
                                              (GInstanceInitFunc) fixed_resolver_init, 0);
    return type;
}

static GList *fixed_lookup (GResolver *resolver, const gchar *hostname, GResolverNameLookupFlags flags,
                            GCancellable *cancellable, GError **error)
{
    if (g_str_equal (hostname, "fixed.test"))
        return g_list_append (NULL, g_inet_address_new_from_string ("192.0.2.7"));
    g_set_error (error, G_RESOLVER_ERROR, G_RESOLVER_ERROR_NOT_FOUND, "no %s", hostname);
    return NULL;
}

static GList *fixed_lookup_by_name (GResolver *resolver, const gchar *hostname, GCancellable *cancellable, GError **error)
{
    return fixed_lookup (resolver, hostname, 0, cancellable, error);
}

static void fixed_resolver_init (FixedResolver *resolver) { }
static void fixed_resolver_class_init (FixedResolverClass *klass)
{
    klass->lookup_by_name = fixed_lookup_by_name;
    klass->lookup_by_name_with_flags = fixed_lookup;
}

static void resolvers (void)
{
    GResolver *resolver = g_resolver_get_default ();
    GError *error = NULL;
    printf ("default resolver %s, timeout %u\n", G_OBJECT_TYPE_NAME (resolver), g_resolver_get_timeout (resolver));
    print_addresses ("127.0.0.1", g_resolver_lookup_by_name (resolver, "127.0.0.1", NULL, &error), &error);
    error = NULL;
    print_addresses ("[::1]", g_resolver_lookup_by_name (resolver, "::1", NULL, &error), &error);
    error = NULL;
    print_addresses ("localhost v4", g_resolver_lookup_by_name_with_flags (resolver, "localhost", G_RESOLVER_NAME_LOOKUP_FLAGS_IPV4_ONLY, NULL, &error), &error);
    error = NULL;
    print_addresses ("localhost v6", g_resolver_lookup_by_name_with_flags (resolver, "localhost", G_RESOLVER_NAME_LOOKUP_FLAGS_IPV6_ONLY, NULL, &error), &error);

    GMainLoop *loop = g_main_loop_new (NULL, FALSE);
    g_resolver_lookup_by_name_with_flags_async (resolver, "127.0.0.2", G_RESOLVER_NAME_LOOKUP_FLAGS_DEFAULT, NULL, resolved, loop);
    g_main_loop_run (loop);
    GInetAddress *loopback = g_inet_address_new_loopback (G_SOCKET_FAMILY_IPV6);
    g_resolver_lookup_by_address_async (resolver, loopback, NULL, named, loop);
    g_main_loop_run (loop);
    g_object_unref (loopback);
    g_main_loop_unref (loop);

    g_object_set (resolver, "timeout", 1500, NULL);
    guint timeout = 0;
    g_object_get (resolver, "timeout", &timeout, NULL);
    printf ("timeout %u %u\n", timeout, g_resolver_get_timeout (resolver));
    g_resolver_set_timeout (resolver, 0);

    GResolver *fixed = g_object_new (fixed_resolver_get_type (), NULL);
    error = NULL;
    print_addresses ("fixed", g_resolver_lookup_by_name (fixed, "fixed.test", NULL, &error), &error);
    error = NULL;
    print_addresses ("fixed other", g_resolver_lookup_by_name_with_flags (fixed, "other.test", 0, NULL, &error), &error);
    g_resolver_set_default (fixed);
    GResolver *now = g_resolver_get_default ();
    printf ("default now %s, the same %d\n", G_OBJECT_TYPE_NAME (now), now == fixed);
    g_object_unref (now);
    g_resolver_set_default (resolver);
    g_object_unref (fixed);
    g_object_unref (resolver);
}

int main (void)
{
    setvbuf (stdout, NULL, _IOLBF, 0);
    datalists ();
    randoms ();
    languages ("fr_FR:de", NULL, "en_US.UTF-8");
    languages (NULL, "pt_BR.UTF-8@euro", NULL);
    languages (NULL, NULL, "sr@latin");
    languages (NULL, NULL, NULL);
    hostname ("www.gnome.org");
    hostname ("b\xc3\xbc" "cher.example");
    hostname ("M\xc3\xbcnchen.DE");
    hostname ("\xe4\xbe\x8b\xe3\x81\x88.\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88");
    hostname ("bad\xff.org");
    enum_type (G_TYPE_SOCKET_CLIENT_EVENT, G_SOCKET_CLIENT_TLS_HANDSHAKED);
    enum_type (G_TYPE_TLS_AUTHENTICATION_MODE, G_TLS_AUTHENTICATION_REQUIRED);
    proxies ();
    network_address ();
    resolvers ();
    return 0;
}
