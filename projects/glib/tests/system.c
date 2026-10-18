/* mlx-glib's markup parser, programs run with g_spawn, Unix pipes and
   signal sources, threads that may fail, and dates and times;
   system.expected is what GLib 2.80 prints for the same program. */
#include <glib.h>
#include <glib-unix.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static void start_element(GMarkupParseContext *context, const char *name, const char **names, const char **values, gpointer data, GError **error)
{
    printf("start %s", name);
    for (int i = 0; names[i]; i++)
        printf(" %s='%s'", names[i], values[i]);
    printf(" (current %s)\n", g_markup_parse_context_get_element(context));
    if (strcmp(name, "forbidden") == 0)
        g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT, "element %s is not allowed", name);
}

static void end_element(GMarkupParseContext *context, const char *name, gpointer data, GError **error)
{
    printf("end %s\n", name);
}

static void text(GMarkupParseContext *context, const char *text, gsize length, gpointer data, GError **error)
{
    printf("text '%.*s'\n", (int)length, text);
}

static void passthrough(GMarkupParseContext *context, const char *text, gsize length, gpointer data, GError **error)
{
    printf("passthrough '%.*s'\n", (int)length, text);
}

static void on_error(GMarkupParseContext *context, GError *error, gpointer data)
{
    printf("error callback: %s\n", error->message);
}

static const GMarkupParser parser = { start_element, end_element, text, passthrough, on_error };

static void parse(const char *document, int pieces, GMarkupParseFlags flags)
{
    GMarkupParseContext *context = g_markup_parse_context_new(&parser, flags, NULL, NULL);
    GError *error = NULL;
    size_t length = strlen(document);
    size_t step = pieces > 1 ? (length + pieces - 1) / pieces : length;
    gboolean ok = TRUE;
    for (size_t at = 0; ok && at < length; at += step)
        ok = g_markup_parse_context_parse(context, document + at, at + step > length ? length - at : step, &error);
    if (ok)
        ok = g_markup_parse_context_end_parse(context, &error);
    printf("parse result %d %s\n", ok, error ? error->message : "");
    if (error)
        printf("error domain markup %d code %d\n", error->domain == G_MARKUP_ERROR, error->code);
    g_clear_error(&error);
    g_markup_parse_context_free(context);
}

static void markup(void)
{
    parse("<root a=\"1\" b='two &amp; three'>hello &lt;world&gt; &#65;&#x42;<child/><!-- note --><?pi x?>tail</root>", 1, 0);
    parse("<root>\r\n  <item name=\"x\">in pieces</item>\n</root>", 5, 0);
    parse("<root><![CDATA[raw <text>]]></root>", 1, 0);
    parse("<root><![CDATA[raw <text>]]></root>", 1, G_MARKUP_TREAT_CDATA_AS_TEXT);
    parse("<root><a></b></root>", 1, 0);
    parse("<root>unclosed", 1, 0);
    parse("   ", 1, 0);
    parse("<root><forbidden/></root>", 1, 0);
    parse("<root>&bogus;</root>", 1, 0);
    char *escaped = g_markup_escape_text("a < b & 'c' \"d\" > e\x01", -1);
    printf("escaped %s\n", escaped);
    g_free(escaped);
    escaped = g_markup_printf_escaped("<a href=\"%s\">%d%% of %s</a>", "x&y", 50, "<all>");
    printf("printf escaped %s\n", escaped);
    g_free(escaped);
}

static void spawning(void)
{
    char *out = NULL, *err = NULL;
    int status = 0;
    GError *error = NULL;
    char *argv[] = { "/bin/sh", "-c", "echo out; echo err >&2; exit 3", NULL };
    gboolean ok = g_spawn_sync(NULL, argv, NULL, 0, NULL, NULL, &out, &err, &status, &error);
    printf("spawn sync %d out '%s' err '%s'\n", ok, out, err);
    g_free(out);
    g_free(err);
    ok = g_spawn_check_wait_status(status, &error);
    printf("check %d domain exit %d code %d message %s\n", ok, error->domain == G_SPAWN_EXIT_ERROR, error->code, error->message);
    g_clear_error(&error);
    ok = g_spawn_command_line_sync("printf '%s-%s' a 'b c'", &out, NULL, &status, &error);
    printf("command line %d '%s' status ok %d\n", ok, out, g_spawn_check_wait_status(status, NULL));
    g_free(out);
    char *missing[] = { "/nonexistent/program", NULL };
    ok = g_spawn_sync(NULL, missing, NULL, 0, NULL, NULL, NULL, NULL, NULL, &error);
    printf("missing %d spawn error %d code %d: %s\n", ok, error->domain == G_SPAWN_ERROR, error->code, error->message);
    g_clear_error(&error);
    char *search[] = { "true", NULL };
    ok = g_spawn_sync("/", search, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, &status, &error);
    printf("search path %d status %d\n", ok, status);
    ok = g_spawn_sync("/nonexistent-directory", search, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, &status, &error);
    printf("bad directory %d code %d: %s\n", ok, error->code, error->message);
    g_clear_error(&error);
    char *env[] = { "MLX_TEST_VALUE=from env", NULL };
    char *show[] = { "/bin/sh", "-c", "echo $MLX_TEST_VALUE", NULL };
    ok = g_spawn_sync(NULL, show, env, 0, NULL, NULL, &out, NULL, NULL, &error);
    printf("environment %d '%s'", ok, out);
    g_free(out);
    int child_out = -1;
    GPid pid;
    char *cat[] = { "/bin/echo", "async", NULL };
    ok = g_spawn_async_with_pipes(NULL, cat, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid, NULL, &child_out, NULL, &error);
    char buffer[64] = { 0 };
    ssize_t n = read(child_out, buffer, sizeof buffer - 1);
    printf("async %d read '%.*s'", ok, (int)(n > 0 ? n : 0), buffer);
    close(child_out);
    int wstatus;
    waitpid(pid, &wstatus, 0);
    printf("async child exit ok %d\n", g_spawn_check_wait_status(wstatus, NULL));
}

static GMainLoop *loop;

static gboolean signal_quit(gpointer data)
{
    printf("got SIGUSR1\n");
    g_main_loop_quit(loop);
    return G_SOURCE_REMOVE;
}

static gboolean send_signal(gpointer data)
{
    kill(getpid(), SIGUSR1);
    return G_SOURCE_REMOVE;
}

static void unix_parts(void)
{
    int fds[2];
    GError *error = NULL;
    printf("open pipe %d", g_unix_open_pipe(fds, FD_CLOEXEC, &error));
    printf(" nonblocking %d", g_unix_set_fd_nonblocking(fds[0], TRUE, &error));
    char c;
    printf(" read empty %d\n", (int)read(fds[0], &c, 1));
    close(fds[0]);
    close(fds[1]);
    printf("bad fd %d", g_unix_set_fd_nonblocking(-1, TRUE, &error));
    printf(" unix error %d: %s\n", error->domain == G_UNIX_ERROR, error->message);
    g_clear_error(&error);
    loop = g_main_loop_new(NULL, FALSE);
    g_unix_signal_add(SIGUSR1, signal_quit, NULL);
    g_idle_add(send_signal, NULL);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);
}

static gpointer thread_func(gpointer data)
{
    return GINT_TO_POINTER(GPOINTER_TO_INT(data) * 2);
}

static void dates(void)
{
    GError *error = NULL;
    GThread *thread = g_thread_try_new("worker", thread_func, GINT_TO_POINTER(21), &error);
    printf("try new %d result %d\n", thread != NULL, GPOINTER_TO_INT(g_thread_join(thread)));

    GDateTime *date = g_date_time_new_utc(2024, 2, 29, 13, 45, 30.5);
    char *text = g_date_time_format_iso8601(date);
    printf("iso %s unix %lld\n", text, (long long)g_date_time_to_unix(date));
    g_free(text);
    printf("fields %d-%d-%d %d:%d:%d.%06d dow %d doy %d seconds %g\n", g_date_time_get_year(date), g_date_time_get_month(date),
        g_date_time_get_day_of_month(date), g_date_time_get_hour(date), g_date_time_get_minute(date), g_date_time_get_second(date),
        g_date_time_get_microsecond(date), g_date_time_get_day_of_week(date), g_date_time_get_day_of_year(date), g_date_time_get_seconds(date));
    GDateTime *later = g_date_time_add(date, G_TIME_SPAN_DAY + G_TIME_SPAN_HOUR);
    text = g_date_time_format_iso8601(later);
    printf("later %s difference %lld compare %d equal %d\n", text, (long long)g_date_time_difference(later, date),
        g_date_time_compare(date, later), g_date_time_equal(date, date));
    g_free(text);
    GTimeZone *zone = g_time_zone_new_offset(5 * 3600 + 30 * 60);
    GDateTime *shifted = g_date_time_new(zone, 2000, 1, 1, 0, 0, 0);
    text = g_date_time_format_iso8601(shifted);
    printf("offset zone %s id %s offset %lld unix %lld\n", text, g_time_zone_get_identifier(zone),
        (long long)g_date_time_get_utc_offset(shifted), (long long)g_date_time_to_unix(shifted));
    g_free(text);
    GTimeZone *parsed = g_time_zone_new_identifier("-03:00");
    printf("parsed zone %s bogus %p\n", g_time_zone_get_identifier(parsed), (void *)g_time_zone_new_identifier("Not/AZone"));
    g_time_zone_unref(parsed);
    GDateTime *epoch = g_date_time_new_from_unix_utc(0);
    text = g_date_time_format_iso8601(epoch);
    printf("epoch %s dow %d\n", text, g_date_time_get_day_of_week(epoch));
    g_free(text);
    GDateTime *old = g_date_time_new_utc(1, 1, 1, 0, 0, 0);
    text = g_date_time_format_iso8601(old);
    printf("year one %s unix %lld invalid %p %p\n", text, (long long)g_date_time_to_unix(old), (void *)g_date_time_new_utc(2023, 2, 29, 0, 0, 0),
        (void *)g_date_time_new_utc(10000, 1, 1, 0, 0, 0));
    g_free(text);
    GDateTime *usec = g_date_time_new_from_unix_utc_usec(1700000000123456LL);
    text = g_date_time_format_iso8601(usec);
    printf("usec %s\n", text);
    g_free(text);
    GDateTime *now = g_date_time_new_now_utc();
    printf("now after 2024 %d\n", g_date_time_get_year(now) >= 2024);
    g_date_time_unref(now);
    g_date_time_unref(usec);
    g_date_time_unref(old);
    g_date_time_unref(epoch);
    g_date_time_unref(shifted);
    g_time_zone_unref(zone);
    g_date_time_unref(later);
    g_date_time_unref(date);
}

int main(void)
{
    printf("== markup\n");
    markup();
    printf("== spawn\n");
    spawning();
    printf("== unix\n");
    unix_parts();
    printf("== dates\n");
    dates();
    return 0;
}
