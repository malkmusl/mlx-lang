/* mlx-glib's strings, string arrays, GString, UTF-8, Base64, quarks,
   errors and paths, printed; strings.expected is what GLib 2.80 prints
   for the same program. */
#include <glib.h>
#include <glib/gprintf.h>
#include <stdio.h>
#include <string.h>

static void show_strv(const char *label, char **v)
{
    printf("%s [%u]:", label, g_strv_length(v));
    for (char **p = v; *p; p++)
        printf(" '%s'", *p);
    printf("\n");
}

static void show(const char *label, char *text)
{
    printf("%s: '%s'\n", label, text ? text : "(null)");
    g_free(text);
}

int main(void)
{
    show("strdup", g_strdup("hello"));
    show("strndup", g_strndup("hello world", 5));
    show("printf", g_strdup_printf("%d-%s-%.2f-%c-%lu-%x", 42, "x", 3.14159, 'z', 123456789012UL, 255));
    show("printf many", g_strdup_printf("%d %d %d %d %d %d %d %d %s %g %g", 1, 2, 3, 4, 5, 6, 7, 8, "nine", 1.5, 2.25));
    show("concat", g_strconcat("a", "bc", "", "def", NULL));
    show("join", g_strjoin(", ", "one", "two", "three", NULL));
    show("nfill", g_strnfill(4, '*'));

    char **v = g_strsplit("a,b,,c", ",", -1);
    show_strv("split", v);
    show("joinv", g_strjoinv("|", v));
    printf("contains b %d, contains z %d\n", g_strv_contains((const char * const *)v, "b"), g_strv_contains((const char * const *)v, "z"));
    char **copy = g_strdupv(v);
    printf("equal %d\n", g_strv_equal((const char * const *)v, (const char * const *)copy));
    g_strfreev(copy);
    g_strfreev(v);
    v = g_strsplit("a::b::c", "::", 2);
    show_strv("split max 2", v);
    g_strfreev(v);
    v = g_strsplit("", ",", -1);
    show_strv("split empty", v);
    g_strfreev(v);
    v = g_strsplit("CHASSIS=desktop\nOTHER=1\n", "\n", -1);
    show_strv("split lines", v);
    g_strfreev(v);
    v = g_strsplit_set("a b,c;d", " ,;", -1);
    show_strv("split set", v);
    g_strfreev(v);

    char buffer[8];
    gsize n = g_strlcpy(buffer, "truncated", sizeof buffer);
    printf("strlcpy %zu '%s'\n", (size_t)n, buffer);
    n = g_strlcat(buffer, "xyz", sizeof buffer);
    printf("strlcat %zu '%s'\n", (size_t)n, buffer);
    printf("prefix %d %d suffix %d %d\n", g_str_has_prefix("glib", "gl"), g_str_has_prefix("glib", "lib"), g_str_has_suffix("glib", "ib"), g_str_has_suffix("glib", "gl"));
    printf("strcmp0 %d %d %d\n", g_strcmp0(NULL, NULL), g_strcmp0(NULL, "a") < 0, g_strcmp0("b", "a") > 0);
    printf("casecmp %d %d\n", g_ascii_strcasecmp("HeLLo", "hello"), g_ascii_strncasecmp("abcX", "ABCy", 3));
    show("strdown", g_ascii_strdown("MiXeD 123", -1));
    show("strup", g_ascii_strup("MiXeD 123", 3));
    char spaced[] = "  \t trim me \n ";
    printf("strstrip '%s'\n", g_strstrip(spaced));
    show("escape", g_strescape("tab\there \"q\" back\\ nl\n\001", NULL));
    show("compress", g_strcompress("a\\tb\\n\\101\\\\"));
    printf("strstr_len '%s' rstr '%s'\n", g_strstr_len("hello world", -1, "o w"), g_strrstr("abcabc", "bc"));
    printf("strtoll %ld strtoull %lu\n", (long)g_ascii_strtoll("-1234xyz", NULL, 10), (unsigned long)g_ascii_strtoull("ff", NULL, 16));
    char dbuf[G_ASCII_DTOSTR_BUF_SIZE];
    printf("dtostr %s %s\n", g_ascii_dtostr(dbuf, sizeof dbuf, 0.1), g_ascii_dtostr(dbuf, sizeof dbuf, 1e100));
    printf("digits %d %d %d\n", g_ascii_digit_value('7'), g_ascii_xdigit_value('c'), g_ascii_isspace('\v'));

    GString *s = g_string_new("abc");
    g_string_append(s, "def");
    g_string_append_c(s, '!');
    g_string_prepend(s, ">>");
    g_string_insert(s, 2, "[");
    g_string_append_printf(s, " %d/%s", 7, "seven");
    g_string_append_unichar(s, 0x20AC);
    printf("gstring '%s' len %zu\n", s->str, (size_t)s->len);
    g_string_erase(s, 0, 3);
    g_string_truncate(s, 6);
    printf("gstring erased '%s' len %zu\n", s->str, (size_t)s->len);
    g_string_printf(s, "%05d", 42);
    g_string_ascii_up(g_string_append(s, "abc"));
    printf("gstring printf '%s'\n", s->str);
    show("gstring free", g_string_free(s, FALSE));

    const char *utf8 = "a\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80";
    printf("utf8 valid %d len %ld char2 %x\n", g_utf8_validate(utf8, -1, NULL), (long)g_utf8_strlen(utf8, -1), g_utf8_get_char(g_utf8_offset_to_pointer(utf8, 2)));
    const char *end = NULL;
    const char *invalid = "ab\xc3x";
    gboolean valid = g_utf8_validate(invalid, -1, &end);
    printf("utf8 invalid %d at %ld\n", valid, (long)(end - invalid));
    show("utf8 substring", g_utf8_substring(utf8, 1, 3));
    show("make_valid", g_utf8_make_valid("ok\xff!", -1));
    char ubuf[8] = { 0 };
    int ulen = g_unichar_to_utf8(0x1F600, ubuf);
    printf("unichar_to_utf8 %d %02x%02x%02x%02x\n", ulen, (guchar)ubuf[0], (guchar)ubuf[1], (guchar)ubuf[2], (guchar)ubuf[3]);

    show("base64", g_base64_encode((const guchar *)"Hello, GLib!", 12));
    gsize decoded_len = 0;
    guchar *decoded = g_base64_decode("SGVsbG8sIEdMaWIh", &decoded_len);
    printf("base64 decoded %zu '%.*s'\n", (size_t)decoded_len, (int)decoded_len, decoded);
    g_free(decoded);

    GQuark q = g_quark_from_string("my-quark");
    printf("quark %d same %d string '%s' try %d\n", q != 0, q == g_quark_from_static_string("my-quark"), g_quark_to_string(q), g_quark_try_string("never-made") == 0);
    printf("intern same %d\n", g_intern_string("interned") == g_intern_static_string("interned"));

    GError *error = NULL;
    g_set_error(&error, G_FILE_ERROR, G_FILE_ERROR_NOENT, "No such file: %s (%d)", "x.txt", 2);
    printf("error '%s' matches %d %d\n", error->message, g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOENT), g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_EXIST));
    GError *copied = g_error_copy(error);
    g_prefix_error(&copied, "While %s: ", "testing");
    printf("prefixed '%s'\n", copied->message);
    g_error_free(copied);
    g_clear_error(&error);
    printf("cleared %p\n", (void *)error);
    printf("file error from errno %d %d\n", g_file_error_from_errno(2), g_file_error_from_errno(13));

    show("basename", g_path_get_basename("/usr/lib/libglib.so"));
    show("basename slash", g_path_get_basename("/usr/lib/"));
    show("basename root", g_path_get_basename("///"));
    show("basename empty", g_path_get_basename(""));
    show("dirname", g_path_get_dirname("/usr/lib/libglib.so"));
    show("dirname none", g_path_get_dirname("file"));
    show("dirname root", g_path_get_dirname("/file"));
    show("build_filename", g_build_filename("/usr", "share/", "/wpe", "inspector.gresource", NULL));
    show("build_filename leading", g_build_filename("//a/", "b//", NULL));
    show("build_filename one", g_build_filename("/", NULL));
    show("build_path", g_build_path(":", "a:", ":b", "c", NULL));
    show("canonicalize", g_canonicalize_filename("a/./b/../c//d", "/root"));
    printf("absolute %d %d\n", g_path_is_absolute("/x"), g_path_is_absolute("x"));

    g_setenv("MLX_GLIB_TEST", "value", TRUE);
    printf("getenv '%s'\n", g_getenv("MLX_GLIB_TEST"));
    g_setenv("MLX_GLIB_TEST", "other", FALSE);
    printf("not overwritten '%s'\n", g_getenv("MLX_GLIB_TEST"));
    g_unsetenv("MLX_GLIB_TEST");
    printf("unset %p\n", (void *)g_getenv("MLX_GLIB_TEST"));
    char **env = g_environ_setenv(NULL, "A", "1", TRUE);
    env = g_environ_setenv(env, "B", "2", TRUE);
    printf("environ B=%s\n", g_environ_getenv(env, "B"));
    g_strfreev(env);
    printf("prgname %s\n", g_get_prgname() ? g_get_prgname() : "(null)");
    g_set_prgname("strings-test");
    printf("prgname %s\n", g_get_prgname());
    printf("monotonic %d\n", g_get_monotonic_time() > 0);
    {
        char buffer[64];
        int written = g_sprintf(buffer, "%s-%05.1f-%x", "gprintf", 3.14159, 255);
        g_printf("g_sprintf %d '%s'\n", written, buffer);
        g_fprintf(stdout, "g_fprintf %ld %c\n", 1234567890123L, 'z');
        int length = g_snprintf(buffer, 4, "%d", 123456);
        printf("g_snprintf %d '%s'\n", length, buffer);
        fflush(stdout);
    }
    return 0;
}
