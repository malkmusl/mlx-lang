/* GOptionContext and GOptionGroup (as JavaScriptCore's GLib API makes a
   group of its options for a program's context), and the gettext
   functions and macros of a library; options.expected is what GLib 2.80
   prints for the same program. */
#define GETTEXT_PACKAGE "test"
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <stdio.h>
#include <string.h>

static gboolean verbose;
static gboolean quiet = TRUE;
static char *name;
static int count = -1;
static double ratio;
static gint64 big;
static char **words;
static char *file;
static char **rest;
static int group_level;
static char *group_mode;

static gboolean on_option(const char *option, const char *value, gpointer data, GError **error)
{
    printf("  callback %s = %s (data %s)\n", option, value ? value : "(none)", (const char *)data);
    if (value && strcmp(value, "bad") == 0) {
        g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE, "Failed parse value '%s' for %s", value, option);
        return FALSE;
    }
    if (value && strcmp(value, "silent") == 0)
        return FALSE;
    return TRUE;
}

static gboolean pre_parse(GOptionContext *context, GOptionGroup *group, gpointer data, GError **error)
{
    printf("  pre-parse (%s)\n", (const char *)data);
    return TRUE;
}

static gboolean post_parse(GOptionContext *context, GOptionGroup *group, gpointer data, GError **error)
{
    printf("  post-parse (%s)\n", (const char *)data);
    return TRUE;
}

static void destroyed(gpointer data)
{
    printf("  group data destroyed (%s)\n", (const char *)data);
}

static const GOptionEntry main_entries[] = {
    { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Say more", NULL },
    { "loud", 'l', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &quiet, "Not quiet", NULL },
    { "name", 'n', 0, G_OPTION_ARG_STRING, &name, "A name", "NAME" },
    { "count", 'c', 0, G_OPTION_ARG_INT, &count, "A count", "N" },
    { "ratio", 0, 0, G_OPTION_ARG_DOUBLE, &ratio, "A ratio", NULL },
    { "big", 0, 0, G_OPTION_ARG_INT64, &big, "A big one", NULL },
    { "word", 'w', 0, G_OPTION_ARG_STRING_ARRAY, &words, "Words", NULL },
    { "file", 'f', 0, G_OPTION_ARG_FILENAME, &file, "A file", NULL },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &rest, NULL, NULL },
    { NULL }
};

static void reset(void)
{
    verbose = FALSE;
    quiet = TRUE;
    g_clear_pointer(&name, g_free);
    count = -1;
    ratio = 0;
    big = 0;
    g_clear_pointer(&words, g_strfreev);
    g_clear_pointer(&file, g_free);
    g_clear_pointer(&rest, g_strfreev);
    group_level = 0;
    g_clear_pointer(&group_mode, g_free);
}

static GOptionContext *make_context(gboolean ignore_unknown)
{
    GOptionContext *context = g_option_context_new("- a test");
    g_option_context_add_main_entries(context, main_entries, GETTEXT_PACKAGE);
    GOptionGroup *group = g_option_group_new("extra", _("Extra Options"), _("Show extra options"), (gpointer) "extra data", destroyed);
    GOptionEntry group_entries[] = {
        { "level", 0, 0, G_OPTION_ARG_INT, &group_level, "A level", NULL },
        { "mode", 0, 0, G_OPTION_ARG_STRING, &group_mode, "A mode", NULL },
        { "call", 0, 0, G_OPTION_ARG_CALLBACK, (gpointer) on_option, "A callback", NULL },
        { "flag", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, (gpointer) on_option, "A flag", NULL },
        { "maybe", 0, G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, (gpointer) on_option, "Maybe a value", NULL },
        { NULL }
    };
    g_option_group_add_entries(group, group_entries);
    g_option_group_set_translation_domain(group, GETTEXT_PACKAGE);
    g_option_group_set_parse_hooks(group, pre_parse, post_parse);
    g_option_context_add_group(context, group);
    g_option_context_set_ignore_unknown_options(context, ignore_unknown);
    return context;
}

static void parse(gboolean ignore_unknown, const char *first, ...)
{
    reset();
    GPtrArray *args = g_ptr_array_new();
    g_ptr_array_add(args, g_strdup("program"));
    va_list list;
    va_start(list, first);
    for (const char *arg = first; arg; arg = va_arg(list, const char *))
        g_ptr_array_add(args, g_strdup(arg));
    va_end(list);
    g_ptr_array_add(args, NULL);
    int argc = args->len - 1;
    char **argv = (char **)g_ptr_array_free(args, FALSE);
    char **original = g_strdupv(argv);
    printf("parse:");
    for (int i = 1; i < argc; i++)
        printf(" %s", argv[i]);
    printf("\n");
    GOptionContext *context = make_context(ignore_unknown);
    GError *error = NULL;
    gboolean ok = g_option_context_parse(context, &argc, &argv, &error);
    printf("  ok %d", ok);
    if (error)
        printf(", error %s %d: %s", g_quark_to_string(error->domain), error->code, error->message);
    printf("\n  verbose %d quiet %d name %s count %d ratio %g big %" G_GINT64_FORMAT " file %s level %d mode %s\n", verbose, quiet, name ? name : "-", count, ratio, big,
        file ? file : "-", group_level, group_mode ? group_mode : "-");
    if (words) {
        printf("  words:");
        for (int i = 0; words[i]; i++)
            printf(" [%s]", words[i]);
        printf("\n");
    }
    if (rest) {
        printf("  remaining:");
        for (int i = 0; rest[i]; i++)
            printf(" [%s]", rest[i]);
        printf("\n");
    }
    printf("  left %d:", argc);
    for (int i = 0; i < argc; i++)
        printf(" %s", argv[i]);
    printf("\n");
    g_clear_error(&error);
    g_option_context_free(context);
    g_strfreev(argv);
    g_strfreev(original);
}

int main(void)
{
    printf("gettext: %s | %s | %s | %s | %s\n", _("Hello"), N_("Marked"), C_("menu", "Open"), g_dngettext(GETTEXT_PACKAGE, "one file", "%d files", 1),
        g_dngettext(GETTEXT_PACKAGE, "one file", "%d files", 3));
    const char *with_context = "ctx|text";
    const char *plain = "plain";
    printf("strip: %s | %s | %s\n", g_strip_context(with_context, with_context), g_strip_context(plain, plain), g_dpgettext(NULL, "menu|Close", 0));
    printf("error quark: %s\n", g_quark_to_string(G_OPTION_ERROR));

    parse(FALSE, "-v", "--name=Ada", "-c", "12", "--ratio", "0.5", "--big=-9000000000", "-w", "a", "--word=b", "-f", "/tmp/x", "plain", NULL);
    parse(FALSE, "--loud", "-n", "Grace", "--count=-3", "one", "--", "--verbose", "two", NULL);
    parse(FALSE, "--level=4", "--extra-mode", "fast", "--call", "value", "--flag", "--maybe", "--maybe=given", NULL);
    parse(FALSE, "--count", "x12", NULL);
    parse(FALSE, "--count=99999999999", NULL);
    parse(FALSE, "--ratio=pi", NULL);
    parse(FALSE, "--unknown", "-v", NULL);
    parse(FALSE, "-z", NULL);
    parse(FALSE, "--name", NULL);
    parse(FALSE, "--call=bad", NULL);
    parse(FALSE, "--call=silent", NULL);
    parse(TRUE, "--unknown", "-v", "--other=1", "file", NULL);
    parse(FALSE, "-vc", "5", NULL);
    parse(FALSE, "-c7", NULL);

    /* A context of main entries alone, without G_OPTION_REMAINING. */
    {
        GOptionContext *context = g_option_context_new(NULL);
        GOptionEntry entries[] = { { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Say more", NULL }, { NULL } };
        g_option_context_add_main_entries(context, entries, NULL);
        char *list[] = { g_strdup("program"), g_strdup("a"), g_strdup("-v"), g_strdup("--"), g_strdup("-v"), g_strdup("b"), NULL };
        char **argv = list;
        int argc = 6;
        verbose = FALSE;
        gboolean ok = g_option_context_parse(context, &argc, &argv, NULL);
        printf("plain: ok %d verbose %d left %d:", ok, verbose, argc);
        for (int i = 0; i < argc; i++)
            printf(" %s", argv[i]);
        printf("\nmain group: %d\n", g_option_context_get_main_group(context) != NULL);
        g_option_context_free(context);
    }

    GOptionGroup *group = g_option_group_new("solo", "Solo", "Solo options", NULL, NULL);
    g_option_group_ref(group);
    g_option_group_unref(group);
    g_option_group_unref(group);
    printf("done\n");
    return 0;
}
