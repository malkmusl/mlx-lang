/* mlx-glib's GInitable and GIO's TLS types: certificates from PEM (one,
   a chain, a private key, none), as WebKit makes and copies them
   (g_initable_new with the backend's certificate type and
   "certificate-pem" or "certificate", the issuer chain walked), their
   PEM and DER read back and compared; TLS passwords; the TLS errors; and
   what a backend without TLS answers.

   The certificates part prints what GLib 2.80 prints with its GnuTLS
   backend (glib-networking); the backend part what it prints without a
   TLS backend (GIO_USE_TLS=dummy): no TLS, no database. */
#include <gio/gio.h>
#include <stdio.h>
#include <string.h>

/* What libsoup's headers declare with GIO's beside the TLS types: an
   interface (G_DECLARE_INTERFACE, as SoupSessionFeature) and a final
   type over a filter input stream (SoupMultipartInputStream), whose class
   holds GIO's stream classes whole. Only declared: the types are not
   registered. */
G_BEGIN_DECLS
G_DECLARE_INTERFACE(TestFeature, test_feature, TEST, FEATURE, GObject)
G_DECLARE_FINAL_TYPE(TestFilter, test_filter, TEST, FILTER, GFilterInputStream)
G_END_DECLS

GType test_feature_get_type(void) { return G_TYPE_INVALID; }
GType test_filter_get_type(void) { return G_TYPE_INVALID; }

_Static_assert(sizeof(TestFilterClass) == sizeof(GObjectClass) + 17 * sizeof(gpointer), "GIO's stream classes");

/* Two public root certificates (ISRG Root X2, GTS Root R4), a key, and a
   certificate authority with a certificate it issued, made for this test
   alone. */
static const char *first =
    "-----BEGIN CERTIFICATE-----\n"
    "MIICGzCCAaGgAwIBAgIQQdKd0XLq7qeAwSxs6S+HUjAKBggqhkjOPQQDAzBPMQsw\n"
    "CQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJuZXQgU2VjdXJpdHkgUmVzZWFyY2gg\n"
    "R3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBYMjAeFw0yMDA5MDQwMDAwMDBaFw00\n"
    "MDA5MTcxNjAwMDBaME8xCzAJBgNVBAYTAlVTMSkwJwYDVQQKEyBJbnRlcm5ldCBT\n"
    "ZWN1cml0eSBSZXNlYXJjaCBHcm91cDEVMBMGA1UEAxMMSVNSRyBSb290IFgyMHYw\n"
    "EAYHKoZIzj0CAQYFK4EEACIDYgAEzZvVn4CDCuwJSvMWSj5cz3es3mcFDR0HttwW\n"
    "+1qLFNvicWDEukWVEYmO6gbf9yoWHKS5xcUy4APgHoIYOIvXRdgKam7mAHf7AlF9\n"
    "ItgKbppbd9/w+kHsOdx1ymgHDB/qo0IwQDAOBgNVHQ8BAf8EBAMCAQYwDwYDVR0T\n"
    "AQH/BAUwAwEB/zAdBgNVHQ4EFgQUfEKWrt5LSDv6kviejM9ti6lyN5UwCgYIKoZI\n"
    "zj0EAwMDaAAwZQIwe3lORlCEwkSHRhtFcP9Ymd70/aTSVaYgLXTWNLxBo1BfASdW\n"
    "tL4ndQavEi51mI38AjEAi/V3bNTIZargCyzuFJ0nN6T5U6VR5CmD1/iQMVtCnwr1\n"
    "/q4AaOeMSQ+2b1tbFfLn\n"
    "-----END CERTIFICATE-----\n";

static const char *second =
    "-----BEGIN CERTIFICATE-----\n"
    "MIICCTCCAY6gAwIBAgINAgPlwGjvYxqccpBQUjAKBggqhkjOPQQDAzBHMQswCQYD\n"
    "VQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEUMBIG\n"
    "A1UEAxMLR1RTIFJvb3QgUjQwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAwMDAw\n"
    "WjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2Vz\n"
    "IExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjQwdjAQBgcqhkjOPQIBBgUrgQQAIgNi\n"
    "AATzdHOnaItgrkO4NcWBMHtLSZ37wWHO5t5GvWvVYRg1rkDdc/eJkTBa6zzuhXyi\n"
    "QHY7qca4R9gq55KRanPpsXI5nymfopjTX15YhmUPoYRlBtHci8nHc8iMai/lxKvR\n"
    "HYqjQjBAMA4GA1UdDwEB/wQEAwIBhjAPBgNVHRMBAf8EBTADAQH/MB0GA1UdDgQW\n"
    "BBSATNbrdP9JNqPV2Py1PsVq8JQdjDAKBggqhkjOPQQDAwNpADBmAjEA6ED/g94D\n"
    "9J+uHXqnLrmvT/aDHQ4thQEd0dlq7A/Cr8deVl5c1RxYIigL9zC2L7F8AjEA8GE8\n"
    "p/SgguMh1YQdc4acLa/KNJvxn7kjNuK8YAOdgLOaVsjh4rsUecrNIdSUtUlD\n"
    "-----END CERTIFICATE-----\n";

static const char *key =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgQ7sXml93Uu4RSCYk\n"
    "MKHRmQuxX9Q0BsGm1THRXm8uH5uhRANCAASm1QOwAODnJdG1+lsznWb6ElrUadoC\n"
    "PCCw395RrPJqiQtrLwf6eLXk4e7OIp/D/pm8KKeCdvyO+6hb2fCQvpM0\n"
    "-----END PRIVATE KEY-----\n";

static const char *leaf =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBITCByQIUJiNwYJxRtYTFOU+3HSQl0CvEOSEwCgYIKoZIzj0EAwIwEjEQMA4G\n"
    "A1UEAwwHVGVzdCBDQTAgFw0yNjEwMTgyMjA5MDJaGA8yMTI2MDkyNDIyMDkwMlow\n"
    "FDESMBAGA1UEAwwJbGVhZi50ZXN0MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE\n"
    "r+eYqia7c7YTPM5+p8hbcAe5f+4yIw3pUeuyBz5dRxdorJQephfTrdnJVc9r80O7\n"
    "omjRKCn0VqSNE5T0PBtyojAKBggqhkjOPQQDAgNHADBEAiA/tCz0tEEIHhhYbl0+\n"
    "o+u8EpInuPyXQCBNNQZB9m0azAIgG1iGTrI6HvJbOLXEwe/CKaO1WBQM/LCwUX/b\n"
    "P80c+RY=\n"
    "-----END CERTIFICATE-----\n";

static const char *authority =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBezCCASGgAwIBAgIUDoOFpjtSFXQec6MEahsI+GiaNlswCgYIKoZIzj0EAwIw\n"
    "EjEQMA4GA1UEAwwHVGVzdCBDQTAgFw0yNjEwMTgyMjA5MDJaGA8yMTI2MDkyNDIy\n"
    "MDkwMlowEjEQMA4GA1UEAwwHVGVzdCBDQTBZMBMGByqGSM49AgEGCCqGSM49AwEH\n"
    "A0IABJ+I/OY/jRvx9eWAonnvD0fW8NioCfCrujR1+PkXU18J1urFgGtIsj3Hc+Eh\n"
    "MHh3WAzlbZpwuKHpNDQIxI0cL1GjUzBRMB0GA1UdDgQWBBRLu8jkjb50fGTcrh1m\n"
    "PJbaI1gfMjAfBgNVHSMEGDAWgBRLu8jkjb50fGTcrh1mPJbaI1gfMjAPBgNVHRMB\n"
    "Af8EBTADAQH/MAoGCCqGSM49BAMCA0gAMEUCIQC84c6s6YBRY+sTxjxwrECiGzpD\n"
    "XGhxJl+kkRfoM75RygIgYMVYTkMorJCjh8JHKqZYP0XwuYEn2Y/FtNX+wJ7pm1o=\n"
    "-----END CERTIFICATE-----\n";

static void error(const char *what, GError **e)
{
    if (!*e) {
        printf("%s: no error\n", what);
        return;
    }
    printf("%s: %s %d %s\n", what, g_quark_to_string((*e)->domain), (*e)->code, (*e)->message);
    g_clear_error(e);
}

static void certificates(void)
{
    GError *e = NULL;
    GTlsCertificate *one = g_tls_certificate_new_from_pem(first, -1, &e);
    error("new_from_pem", &e);
    printf("certificate %d issuer %p\n", G_IS_TLS_CERTIFICATE(one), (void *)g_tls_certificate_get_issuer(one));
    gchar *pem = NULL;
    GByteArray *der = NULL;
    g_object_get(one, "certificate-pem", &pem, "certificate", &der, NULL);
    printf("pem the same %d, der %u bytes, first %02x %02x\n", strcmp(pem, first) == 0, der->len, der->data[0], der->data[1]);

    /* As WebKit copies a certificate: by its type and its PEM, then by DER. */
    GType type = g_tls_backend_get_certificate_type(g_tls_backend_get_default());
    printf("type is a GTlsCertificate %d, a GInitable %d\n", g_type_is_a(type, G_TYPE_TLS_CERTIFICATE), g_type_is_a(type, G_TYPE_INITABLE));
    GTlsCertificate *copy = G_TLS_CERTIFICATE(g_initable_new(type, NULL, &e, "certificate-pem", pem, "issuer", NULL, "private-key-pem", NULL, "private-key-pkcs11-uri", NULL, NULL));
    error("initable_new pem", &e);
    GTlsCertificate *fromDer = G_TLS_CERTIFICATE(g_initable_new(type, NULL, &e, "certificate", der, "issuer", NULL, NULL));
    error("initable_new der", &e);
    GTlsCertificate *other = g_tls_certificate_new_from_pem(second, -1, NULL);
    printf("same %d %d %d, other %d\n", g_tls_certificate_is_same(one, one), g_tls_certificate_is_same(one, copy), g_tls_certificate_is_same(copy, fromDer), g_tls_certificate_is_same(one, other));

    /* A chain: the authority issued the leaf. Two certificates that are
       not a chain are the first alone. */
    gchar *both = g_strconcat(leaf, authority, NULL);
    GTlsCertificate *chain = g_tls_certificate_new_from_pem(both, -1, &e);
    error("chain", &e);
    GTlsCertificate *root = g_tls_certificate_new_from_pem(authority, -1, NULL);
    int depth = 0;
    for (GTlsCertificate *c = chain; c; c = g_tls_certificate_get_issuer(c))
        depth++;
    printf("chain depth %d, issuer %d\n", depth, g_tls_certificate_is_same(g_tls_certificate_get_issuer(chain), root));
    gchar *unrelated = g_strconcat(first, second, NULL);
    GTlsCertificate *alone = g_tls_certificate_new_from_pem(unrelated, -1, &e);
    error("not a chain", &e);
    printf("not a chain: issuer %p, first %d\n", (void *)g_tls_certificate_get_issuer(alone), g_tls_certificate_is_same(alone, one));

    /* A key with the certificate. */
    gchar *withKey = g_strconcat(first, key, NULL);
    GTlsCertificate *keyed = g_tls_certificate_new_from_pem(withKey, -1, &e);
    error("with key", &e);
    gchar *keyPem = NULL;
    GByteArray *keyDer = NULL;
    g_object_get(keyed, "private-key-pem", &keyPem, "private-key", &keyDer, NULL);
    printf("key pem %d, key der %d\n", keyPem != NULL, keyDer != NULL);
    g_object_get(one, "private-key-pem", &keyPem, NULL);
    printf("no key: %p\n", (void *)keyPem);

    /* No certificate, a broken one. */
    GTlsCertificate *none = g_tls_certificate_new_from_pem("no certificate here", -1, &e);
    printf("none %p\n", (void *)none);
    error("no certificate", &e);
    none = g_tls_certificate_new_from_pem(key, -1, &e);
    printf("a key alone %p\n", (void *)none);
    error("a key alone", &e);

    g_free(pem);
    g_free(both);
    g_free(unrelated);
    g_free(withKey);
    g_byte_array_unref(der);
    if (keyDer)
        g_byte_array_unref(keyDer);
    g_object_unref(one);
    g_object_unref(copy);
    g_object_unref(fromDer);
    g_object_unref(other);
    g_object_unref(chain);
    g_object_unref(root);
    g_object_unref(alone);
    g_object_unref(keyed);
}

static void passwords(void)
{
    GTlsPassword *password = g_tls_password_new(G_TLS_PASSWORD_RETRY, "Token PIN");
    printf("password flags %d description %s warning %s\n", g_tls_password_get_flags(password), g_tls_password_get_description(password), g_tls_password_get_warning(password));
    g_tls_password_set_value(password, (const guchar *)"secret", -1);
    gsize length = 0;
    const guchar *value = g_tls_password_get_value(password, &length);
    printf("value %.*s %zu\n", (int)length, (const char *)value, length);
    g_tls_password_set_flags(password, G_TLS_PASSWORD_FINAL_TRY);
    printf("warning %s\n", g_tls_password_get_warning(password));
    g_tls_password_set_flags(password, G_TLS_PASSWORD_MANY_TRIES | G_TLS_PASSWORD_RETRY);
    printf("warning %s\n", g_tls_password_get_warning(password));
    g_tls_password_set_warning(password, "Careful");
    g_object_set(password, "description", "Another", NULL);
    GTlsPasswordFlags flags;
    gchar *description = NULL;
    gchar *warning = NULL;
    g_object_get(password, "flags", &flags, "description", &description, "warning", &warning, NULL);
    printf("properties %d %s %s\n", flags, description, warning);
    g_free(description);
    g_free(warning);
    g_object_unref(password);
    password = g_tls_password_new(G_TLS_PASSWORD_NONE, "Key");
    printf("no warning %p\n", (void *)g_tls_password_get_warning(password));
    g_object_unref(password);
}

static void backend(void)
{
    GTlsBackend *backend = g_tls_backend_get_default();
    GTlsDatabase *database = g_tls_backend_get_default_database(backend);
    printf("backend %d, the same %d, tls %d, dtls %d, database %d\n", G_IS_TLS_BACKEND(backend), backend == g_tls_backend_get_default(), g_tls_backend_supports_tls(backend), g_tls_backend_supports_dtls(backend), G_IS_TLS_DATABASE(database));
    g_object_unref(database);
    GError *e = NULL;
    database = g_tls_file_database_new("/etc/ssl/certs/ca-certificates.crt", &e);
    printf("file database %p\n", (void *)database);
    error("file database", &e);
    printf("types %d %d %d %d %d\n", g_type_is_a(G_TYPE_TLS_CONNECTION, G_TYPE_IO_STREAM), g_type_is_a(G_TYPE_TLS_CLIENT_CONNECTION, G_TYPE_TLS_CONNECTION), g_type_is_a(G_TYPE_TLS_FILE_DATABASE, G_TYPE_TLS_DATABASE), G_TYPE_IS_ABSTRACT(G_TYPE_TLS_CERTIFICATE), G_TYPE_IS_INTERFACE(G_TYPE_INITABLE));
    printf("errors %s %s %d\n", g_quark_to_string(G_TLS_ERROR), g_quark_to_string(G_TLS_CHANNEL_BINDING_ERROR), G_TLS_ERROR_BAD_CERTIFICATE);
}

int main(int argc, char **argv)
{
    /* (GLib's run: "certificates" with GnuTLS, "backend" without.) */
    const char *part = argc > 1 ? argv[1] : "all";
    setvbuf(stdout, NULL, _IONBF, 0);
    if (!strcmp(part, "all") || !strcmp(part, "certificates")) {
        certificates();
        passwords();
    }
    if (!strcmp(part, "all") || !strcmp(part, "backend"))
        backend();
    return 0;
}
