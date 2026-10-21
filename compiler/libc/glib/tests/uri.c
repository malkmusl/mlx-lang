/* mlx-glib's GUri: URIs parsed (strictly, relaxed, with users and
   passwords, encoded, normalized), resolved against a base, written
   back, split, escaped and unescaped, and query parameters; uri.expected
   is what GLib 2.80 prints for the same program. */
#include <glib.h>
#include <stdio.h>
#include <string.h>

static const char *or_null(const char *text)
{
    return text ? text : "(null)";
}

static void parse(const char *text, GUriFlags flags)
{
    GError *error = NULL;
    GUri *uri = g_uri_parse(text, flags, &error);
    if (!uri) {
        printf("parse '%s' %d: error %d: %s\n", text, flags, error->code, error->message);
        g_error_free(error);
        return;
    }
    char *back = g_uri_to_string(uri);
    printf("parse '%s' %d: scheme %s userinfo %s user %s password %s auth %s host %s port %d path %s query %s fragment %s -> %s\n",
        text, flags, or_null(g_uri_get_scheme(uri)), or_null(g_uri_get_userinfo(uri)), or_null(g_uri_get_user(uri)),
        or_null(g_uri_get_password(uri)), or_null(g_uri_get_auth_params(uri)), or_null(g_uri_get_host(uri)),
        g_uri_get_port(uri), or_null(g_uri_get_path(uri)), or_null(g_uri_get_query(uri)), or_null(g_uri_get_fragment(uri)), back);
    char *partial = g_uri_to_string_partial(uri, G_URI_HIDE_USERINFO | G_URI_HIDE_QUERY);
    printf("  partial %s\n", partial);
    g_free(partial);
    g_free(back);
    g_uri_unref(g_uri_ref(uri));
    g_uri_unref(uri);
}

static void resolve(const char *base, const char *ref)
{
    GError *error = NULL;
    char *resolved = g_uri_resolve_relative(base, ref, G_URI_FLAGS_NONE, &error);
    if (!resolved) {
        printf("resolve '%s' '%s': error %d: %s\n", or_null(base), ref, error->code, error->message);
        g_error_free(error);
        return;
    }
    printf("resolve '%s' '%s' = %s\n", or_null(base), ref, resolved);
    g_free(resolved);
}

int main(void)
{
    parse("http://example.com/a/b?q=1#frag", G_URI_FLAGS_NONE);
    parse("HTTPS://User:Secret@Example.COM:8443/p%61th/%7euser/x%2fy?a=%41&b#f%20g", G_URI_FLAGS_NONE);
    parse("https://user:secret;auth=x@host/path", G_URI_FLAGS_HAS_PASSWORD | G_URI_FLAGS_HAS_AUTH_PARAMS);
    parse("https://user:secret@host/path", G_URI_FLAGS_HAS_PASSWORD);
    parse("http://[::1]:80/", G_URI_FLAGS_NONE);
    parse("http://[fe80::1%25eth0]/", G_URI_FLAGS_NONE);
    parse("http://[fe80::1%eth0]/", G_URI_FLAGS_PARSE_RELAXED);
    parse("http://192.168.0.1:8080/x", G_URI_FLAGS_NONE);
    parse("file:///home/user/file%20name.txt", G_URI_FLAGS_NONE);
    parse("mailto:someone@example.com", G_URI_FLAGS_NONE);
    parse("urn:isbn:0451450523", G_URI_FLAGS_NONE);
    parse("http://example.com", G_URI_FLAGS_SCHEME_NORMALIZE);
    parse("http://example.com:80/x", G_URI_FLAGS_SCHEME_NORMALIZE);
    parse("https://example.com:443", G_URI_FLAGS_SCHEME_NORMALIZE);
    parse("http://example.com/%7Ea%2fb%zz", G_URI_FLAGS_ENCODED);
    parse("http://example.com/%7Ea%2fb%zz", G_URI_FLAGS_ENCODED | G_URI_FLAGS_PARSE_RELAXED);
    parse("http://example.com/a b", G_URI_FLAGS_NONE);
    parse("  http://example.com/a b\n ", G_URI_FLAGS_PARSE_RELAXED);
    parse("http://exa%41mple.com/", G_URI_FLAGS_NONE);
    parse("http://host:99999/", G_URI_FLAGS_NONE);
    parse("http://host:12a/", G_URI_FLAGS_NONE);
    parse("http://host:/", G_URI_FLAGS_NONE);
    parse("http://[::1/", G_URI_FLAGS_NONE);
    parse("http://%31%32%37.0.0.1/", G_URI_FLAGS_NONE);
    parse("http://bücher.example/straße", G_URI_FLAGS_NONE);
    parse("relative/path", G_URI_FLAGS_NONE);
    parse("http://a/b/c/./../../g", G_URI_FLAGS_NONE);
    parse("http://a@b@c/", G_URI_FLAGS_PARSE_RELAXED);
    parse("http://host;semi/path", G_URI_FLAGS_PARSE_RELAXED);
    parse("http://example.com/\xff", G_URI_FLAGS_NONE);
    parse("http://example.com/%ff", G_URI_FLAGS_NONE);
    parse("http://example.com/%ff", G_URI_FLAGS_ENCODED);
    parse("http://example.com/path?q=a b", G_URI_FLAGS_ENCODED_QUERY);

    const char *base = "http://a/b/c/d;p?q";
    const char *refs[] = { "g:h", "g", "./g", "g/", "/g", "//g", "?y", "g?y", "#s", "g#s", "g?y#s", ";x", "g;x",
        "g;x?y#s", "", ".", "./", "..", "../", "../g", "../..", "../../", "../../g", "../../../g", "../../../../g",
        "/./g", "/../g", "g.", ".g", "g..", "..g", "./../g", "./g/.", "g/./h", "g/../h", "g;x=1/./y", "g;x=1/../y",
        "g?y/./x", "g?y/../x", "g#s/./x", "g#s/../x", "http:g" };
    for (unsigned i = 0; i < G_N_ELEMENTS(refs); i++)
        resolve(base, refs[i]);
    resolve(NULL, "relative");
    resolve(NULL, "http://x/y/../z");
    resolve("http://example.com", "path");

    GError *error = NULL;
    char *scheme, *userinfo, *host, *path, *query, *fragment;
    int port;
    gboolean ok = g_uri_split("ftp://u@h:21/p?q#f", G_URI_FLAGS_NONE, &scheme, &userinfo, &host, &port, &path, &query, &fragment, &error);
    printf("split %d %s %s %s %d %s %s %s\n", ok, scheme, userinfo, host, port, path, query, fragment);
    g_free(scheme); g_free(userinfo); g_free(host); g_free(path); g_free(query); g_free(fragment);
    char *user, *password, *auth;
    ok = g_uri_split_with_user("s://a:b;c@h/", G_URI_FLAGS_HAS_PASSWORD | G_URI_FLAGS_HAS_AUTH_PARAMS, &scheme, &user, &password, &auth, &host, &port, &path, &query, &fragment, &error);
    printf("split with user %d %s %s %s %s %s %d %s %s %s\n", ok, scheme, user, password, auth, host, port, path, or_null(query), or_null(fragment));
    g_free(scheme); g_free(user); g_free(password); g_free(auth); g_free(host); g_free(path);
    ok = g_uri_split_network("https://example.org:444/x", G_URI_FLAGS_NONE, &scheme, &host, &port, &error);
    printf("split network %d %s %s %d\n", ok, scheme, host, port);
    g_free(scheme); g_free(host);
    ok = g_uri_split_network("/just/a/path", G_URI_FLAGS_NONE, &scheme, &host, &port, &error);
    printf("split network %d %s\n", ok, error->message);
    g_clear_error(&error);
    ok = g_uri_split_network("file:///x", G_URI_FLAGS_NONE, &scheme, &host, &port, &error);
    printf("split network %d '%s' %s\n", ok, host, error ? error->message : "no error");
    g_clear_error(&error);
    g_free(scheme); g_free(host);
    ok = g_uri_split_network("file:/x", G_URI_FLAGS_NONE, &scheme, &host, &port, &error);
    printf("split network %d %s\n", ok, error ? error->message : "no error");
    g_clear_error(&error);
    printf("valid %d", g_uri_is_valid("http://x/", G_URI_FLAGS_NONE, NULL));
    printf(" %d", g_uri_is_valid("x/y", G_URI_FLAGS_NONE, &error));
    printf(" %s\n", error->message);
    g_clear_error(&error);

    char *joined = g_uri_join(G_URI_FLAGS_NONE, "http", "us er", "h:st", 8080, "/a b/c", "q=1 2", "f g");
    printf("join %s\n", joined);
    g_free(joined);
    joined = g_uri_join(G_URI_FLAGS_NONE, "http", NULL, "::1", -1, "/", NULL, NULL);
    printf("join ipv6 %s\n", joined);
    g_free(joined);
    joined = g_uri_join_with_user(G_URI_FLAGS_NONE, "imap", "us:er", "p@ss", "auth;x", "mail.example", 143, "", NULL, NULL);
    printf("join with user %s\n", joined);
    g_free(joined);
    joined = g_uri_join(G_URI_FLAGS_SCHEME_NORMALIZE, "HTTP", NULL, "h", 80, "", NULL, NULL);
    printf("join normalized %s\n", joined);
    g_free(joined);
    GUri *built = g_uri_build_with_user(G_URI_FLAGS_HAS_PASSWORD, "s", "u", "p", NULL, "h", 1, "/x", "q", NULL);
    char *text = g_uri_to_string(built);
    printf("built %s userinfo %s\n", text, g_uri_get_userinfo(built));
    g_free(text);
    text = g_uri_to_string_partial(built, G_URI_HIDE_PASSWORD);
    printf("built without password %s\n", text);
    g_free(text);
    g_uri_unref(built);

    char *escaped = g_uri_escape_string("a b/c?d=é&f", NULL, FALSE);
    printf("escape %s\n", escaped);
    g_free(escaped);
    escaped = g_uri_escape_string("a b/c?d=é&f", G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, TRUE);
    printf("escape path utf8 %s\n", escaped);
    g_free(escaped);
    escaped = g_uri_escape_bytes((const guint8 *)"a\0b", 3, NULL);
    printf("escape bytes %s\n", escaped);
    g_free(escaped);
    char *unescaped = g_uri_unescape_string("a%20b%2Fc%zz", NULL);
    printf("unescape %s\n", or_null(unescaped));
    g_free(unescaped);
    unescaped = g_uri_unescape_string("a%20b%2Fc", "/");
    printf("unescape illegal %s\n", or_null(unescaped));
    g_free(unescaped);
    unescaped = g_uri_unescape_string("a%00b", NULL);
    printf("unescape nul %s\n", or_null(unescaped));
    g_free(unescaped);
    unescaped = g_uri_unescape_segment("abc%41def", NULL, NULL);
    printf("unescape segment %s\n", unescaped);
    g_free(unescaped);
    GBytes *bytes = g_uri_unescape_bytes("x%00y%41", -1, NULL, NULL);
    printf("unescape bytes %lu\n", (unsigned long)g_bytes_get_size(bytes));
    g_bytes_unref(bytes);
    GString *s = g_string_new("pre:");
    g_string_append_uri_escaped(s, "a b", NULL, FALSE);
    printf("append escaped %s\n", s->str);
    g_string_free(s, TRUE);

    char *parsed_scheme = g_uri_parse_scheme("HTTP://x");
    printf("schemes %s %s %s %s\n", parsed_scheme, or_null(g_uri_peek_scheme("HTTP://x")), or_null(g_uri_parse_scheme("1abc:x")),
        or_null(g_uri_peek_scheme("no-colon")));
    g_free(parsed_scheme);

    GUriParamsIter iter;
    char *attribute, *value;
    g_uri_params_iter_init(&iter, "a=1&b=x+y%20z;c=&d", -1, "&;", G_URI_PARAMS_WWW_FORM);
    while (g_uri_params_iter_next(&iter, &attribute, &value, &error)) {
        printf("param %s = '%s'\n", attribute, value);
        g_free(attribute);
        g_free(value);
    }
    if (error) {
        printf("params error %d %s\n", error->code, error->message);
        g_clear_error(&error);
    }
    GHashTable *params = g_uri_parse_params("Key=1&other=2&KEY=3", -1, "&", G_URI_PARAMS_CASE_INSENSITIVE, &error);
    printf("params %u key %s other %s\n", g_hash_table_size(params), (char *)g_hash_table_lookup(params, "key"),
        (char *)g_hash_table_lookup(params, "OTHER"));
    g_hash_table_unref(params);
    params = g_uri_parse_params("a=1&bad", -1, "&", G_URI_PARAMS_NONE, &error);
    printf("params bad %p %s\n", (void *)params, error->message);
    g_clear_error(&error);
    printf("error quark %s\n", g_quark_to_string(G_URI_ERROR));
    return 0;
}
