/* The cookie jar and the public suffix list: base domains as libsoup's
 * TLD tests ask for them, cookies set from headers and given back for
 * URIs (their order, domains, paths, Secure, HttpOnly, SameSite, the
 * __Secure- and __Host- prefixes, expiry, third parties), the "changed"
 * signal, a text jar written and read again. Each line printed is
 * compared with what the system's libsoup prints. */
#include <libsoup/soup.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void base (const char *host)
{
    GError *error = NULL;
    const char *result = soup_tld_get_base_domain (host, &error);
    printf ("base %s: ", host);
    if (result) printf ("%s", result);
    else printf ("error %d %s", error->code, error->message);
    printf (" (public suffix %d)\n", soup_tld_domain_is_public_suffix (host));
    g_clear_error (&error);
}

static void on_changed (SoupCookieJar *jar, SoupCookie *old, SoupCookie *new, gpointer data)
{
    printf ("  changed: %s -> %s\n", old ? soup_cookie_get_name (old) : "(none)", new ? soup_cookie_get_name (new) : "(none)");
}

static void set (SoupCookieJar *jar, const char *uri_text, const char *header)
{
    GUri *uri = g_uri_parse (uri_text, SOUP_HTTP_URI_FLAGS, NULL);
    printf ("set %s: %s\n", uri_text, header);
    soup_cookie_jar_set_cookie (jar, uri, header);
    g_uri_unref (uri);
}

static void get (SoupCookieJar *jar, const char *uri_text, gboolean for_http)
{
    GUri *uri = g_uri_parse (uri_text, SOUP_HTTP_URI_FLAGS, NULL);
    char *header = soup_cookie_jar_get_cookies (jar, uri, for_http);
    printf ("get %s%s: %s\n", uri_text, for_http ? "" : " (script)", header ? header : "(none)");
    g_free (header);
    g_uri_unref (uri);
}

static int compare_names (gconstpointer a, gconstpointer b)
{
    return strcmp (soup_cookie_get_name ((SoupCookie *) a), soup_cookie_get_name ((SoupCookie *) b));
}

static void dump (SoupCookieJar *jar)
{
    GSList *all = g_slist_sort (soup_cookie_jar_all_cookies (jar), compare_names), *l;
    printf ("all:");
    for (l = all; l; l = l->next) {
        SoupCookie *c = l->data;
        printf (" %s=%s[%s%s%s%s%s]", soup_cookie_get_name (c), soup_cookie_get_value (c), soup_cookie_get_domain (c), soup_cookie_get_path (c),
                soup_cookie_get_secure (c) ? " secure" : "", soup_cookie_get_http_only (c) ? " httponly" : "",
                soup_cookie_get_expires (c) ? " expires" : "");
    }
    printf ("\n");
    g_slist_free_full (all, (GDestroyNotify) soup_cookie_free);
}

int main (void)
{
    SoupCookieJar *jar;
    GUri *first, *uri;
    GSList *list, *l;
    char path[256];

    setvbuf (stdout, NULL, _IOLBF, 0);
    base ("www.example.com");
    base ("example.com");
    base ("com");
    base ("www.example.co.uk");
    base ("co.uk");
    base ("a.b.c.kawasaki.jp");
    base ("city.kawasaki.jp");
    base ("www.city.kawasaki.jp");
    base ("foo.bar.ck");
    base ("www.ck");
    base ("example.invalidtld");
    base ("invalidtld");
    base (".example.com");
    base ("192.168.1.1");
    base ("foo.github.io");
    base ("github.io");
    base ("EXAMPLE.COM");

    jar = soup_cookie_jar_new ();
    g_signal_connect (jar, "changed", G_CALLBACK (on_changed), NULL);
    printf ("policy %d, persistent %d\n", soup_cookie_jar_get_accept_policy (jar), soup_cookie_jar_is_persistent (jar));
    set (jar, "http://www.example.com/a/b", "one=1");
    set (jar, "http://www.example.com/a/b", "two=2; Path=/");
    set (jar, "http://www.example.com/a/b", "three=3; Path=/a/b/c");
    set (jar, "http://www.example.com/", "dom=d; Domain=example.com");
    set (jar, "http://www.example.com/", "http=h; HttpOnly");
    set (jar, "http://www.example.com/", "sec=s; Secure");
    set (jar, "https://www.example.com/", "sec=s; Secure; Path=/");
    set (jar, "https://www.example.com/", "__Secure-a=1");
    set (jar, "https://www.example.com/", "__Secure-b=1; Secure");
    set (jar, "https://www.example.com/", "__Host-a=1; Secure; Path=/; Domain=example.com");
    set (jar, "https://www.example.com/", "__Host-b=1; Secure; Path=/");
    set (jar, "http://www.example.com/", "none=n; SameSite=None");
    set (jar, "https://www.example.com/", "lax=l; SameSite=Lax; Secure");
    set (jar, "http://www.example.com/", "public=p; Domain=com");
    set (jar, "http://www.example.com/", "other=o; Domain=other.com");
    set (jar, "http://www.example.com/", "gone=g; Max-Age=0");
    set (jar, "http://www.example.com/", "two=22; Path=/");
    set (jar, "http://www.example.com/", "ctl=a\x01b");
    get (jar, "http://www.example.com/a/b/c", TRUE);
    get (jar, "http://www.example.com/a/b", TRUE);
    get (jar, "https://www.example.com/", TRUE);
    get (jar, "https://www.example.com/", FALSE);
    get (jar, "http://example.com/", TRUE);
    get (jar, "http://sub.www.example.com/", TRUE);
    get (jar, "http://other.org/", TRUE);
    dump (jar);

    /* Expiring a cookie by setting it again in the past. */
    set (jar, "http://www.example.com/a/b", "one=x; Expires=Thu, 01 Jan 1970 00:00:00 GMT");
    get (jar, "http://www.example.com/a/b", TRUE);

    /* Deleting. */
    list = soup_cookie_jar_get_cookie_list (jar, g_uri_parse ("http://www.example.com/", SOUP_HTTP_URI_FLAGS, NULL), TRUE);
    for (l = list; l; l = l->next)
        if (strcmp (soup_cookie_get_name (l->data), "dom") == 0) soup_cookie_jar_delete_cookie (jar, l->data);
    g_slist_free_full (list, (GDestroyNotify) soup_cookie_free);
    get (jar, "http://www.example.com/", TRUE);

    /* Third parties. */
    soup_cookie_jar_set_accept_policy (jar, SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY);
    first = g_uri_parse ("http://www.example.com/", SOUP_HTTP_URI_FLAGS, NULL);
    uri = g_uri_parse ("http://tracker.net/", SOUP_HTTP_URI_FLAGS, NULL);
    soup_cookie_jar_set_cookie_with_first_party (jar, uri, first, "track=1");
    g_uri_unref (uri);
    uri = g_uri_parse ("http://img.example.com/", SOUP_HTTP_URI_FLAGS, NULL);
    soup_cookie_jar_set_cookie_with_first_party (jar, uri, first, "img=1");
    get (jar, "http://tracker.net/", TRUE);
    get (jar, "http://img.example.com/", TRUE);
    g_uri_unref (uri);
    soup_cookie_jar_set_accept_policy (jar, SOUP_COOKIE_JAR_ACCEPT_GRANDFATHERED_THIRD_PARTY);
    uri = g_uri_parse ("http://img.example.com/", SOUP_HTTP_URI_FLAGS, NULL);
    soup_cookie_jar_set_cookie_with_first_party (jar, uri, g_uri_parse ("http://elsewhere.org/", SOUP_HTTP_URI_FLAGS, NULL), "grand=1");
    get (jar, "http://img.example.com/", TRUE);
    g_uri_unref (uri);
    g_uri_unref (first);
    printf ("policy now %d\n", soup_cookie_jar_get_accept_policy (jar));
    g_object_unref (jar);

    /* A text jar, written and read again. */
    snprintf (path, sizeof path, "/tmp/mlx-soup-cookies-%d.txt", (int) getpid ());
    unlink (path);
    jar = soup_cookie_jar_text_new (path, FALSE);
    printf ("text jar persistent %d\n", soup_cookie_jar_is_persistent (jar));
    set (jar, "http://www.example.com/", "keep=1; Max-Age=3600");
    set (jar, "http://www.example.com/", "session=1");
    set (jar, "https://www.example.com/", "secure=1; Max-Age=3600; Secure; HttpOnly; SameSite=Strict");
    set (jar, "http://www.example.com/", "keep=2; Max-Age=3600");
    g_object_unref (jar);
    jar = soup_cookie_jar_text_new (path, TRUE);
    dump (jar);
    g_object_unref (jar);
    {
        char *contents = NULL;
        g_file_get_contents (path, &contents, NULL, NULL);
        /* The expiry times vary: the lines without them. */
        for (char *line = contents ? strtok (contents, "\n") : NULL; line; line = strtok (NULL, "\n")) {
            char *tab = line;
            int field = 0;
            printf ("file:");
            for (char *p = line; ; p++) {
                if (*p == '\t' || *p == 0) {
                    if (field != 4) printf (" %.*s", (int) (p - tab), tab);
                    field++;
                    if (*p == 0) break;
                    tab = p + 1;
                }
            }
            printf ("\n");
        }
        g_free (contents);
    }
    unlink (path);
    return 0;
}
