/* mlx-glib's main loop: timeouts, idles, priorities, file descriptor
   sources, a source woken by its ready time as WebKit's run loop does,
   nested loops, thread-default contexts, contexts driven by hand and
   other threads; mainloop.expected is what GLib 2.80 prints for the same
   program. */
#include <glib.h>
#include <glib-unix.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static GMainLoop *loop;
static int remaining;

static gboolean say_once(gpointer data)
{
    printf("timeout %s\n", (const char *)data);
    if (--remaining == 0)
        g_main_loop_quit(loop);
    return G_SOURCE_REMOVE;
}

static void timeouts(void)
{
    loop = g_main_loop_new(NULL, FALSE);
    remaining = 4;
    g_timeout_add(30, say_once, "30");
    g_timeout_add(10, say_once, "10");
    g_timeout_add(20, say_once, "20");
    g_timeout_add_full(G_PRIORITY_HIGH, 0, say_once, "0 high", NULL);
    printf("running %d\n", g_main_loop_is_running(loop));
    g_main_loop_run(loop);
    printf("running %d\n", g_main_loop_is_running(loop));
    g_main_loop_unref(loop);
}

static int idle_count;

static gboolean idle_counter(gpointer data)
{
    idle_count++;
    printf("idle %s %d\n", (const char *)data, idle_count);
    if (idle_count == 3) {
        g_main_loop_quit(loop);
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

static gboolean once_default(gpointer data)
{
    printf("default priority idle %s\n", (const char *)data);
    return G_SOURCE_REMOVE;
}

static void destroyed(gpointer data)
{
    printf("destroy notify %s\n", (const char *)data);
}

static void idles(void)
{
    loop = g_main_loop_new(NULL, FALSE);
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, idle_counter, "low", destroyed);
    g_idle_add_full(G_PRIORITY_DEFAULT, once_default, "first", NULL);
    g_idle_add_full(G_PRIORITY_DEFAULT, once_default, "second", NULL);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);
}

static int pipe_fds[2];

static gboolean readable(gint fd, GIOCondition condition, gpointer data)
{
    char buffer[32];
    ssize_t n = read(fd, buffer, sizeof buffer - 1);
    buffer[n > 0 ? n : 0] = 0;
    printf("fd readable cond %d read '%s'\n", condition & G_IO_IN, buffer);
    g_main_loop_quit(loop);
    return G_SOURCE_REMOVE;
}

static gboolean writer(gpointer data)
{
    printf("writing\n");
    write(pipe_fds[1], "ping", 4);
    return G_SOURCE_REMOVE;
}

static void fd_sources(void)
{
    loop = g_main_loop_new(NULL, FALSE);
    if (pipe(pipe_fds) != 0)
        return;
    guint id = g_unix_fd_add(pipe_fds[0], G_IO_IN, readable, NULL);
    printf("fd source id nonzero %d\n", id != 0);
    g_timeout_add(5, writer, NULL);
    g_main_loop_run(loop);
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    g_main_loop_unref(loop);
}

/* A source with no prepare or check of its own, dispatched when its ready
   time comes, like the one under WebKit's RunLoop. */
typedef struct {
    GSource source;
    int dispatched;
} RunLoopSource;

static gboolean run_loop_dispatch(GSource *source, GSourceFunc callback, gpointer data)
{
    RunLoopSource *self = (RunLoopSource *)source;
    g_source_set_ready_time(source, -1);
    self->dispatched++;
    printf("run loop source dispatched %d name %s\n", self->dispatched, g_source_get_name(source));
    return callback ? callback(data) : G_SOURCE_CONTINUE;
}

static GSourceFuncs run_loop_funcs = { NULL, NULL, run_loop_dispatch, NULL, NULL, NULL };

static gboolean run_loop_callback(gpointer data)
{
    RunLoopSource *self = data;
    if (self->dispatched == 2)
        g_main_loop_quit(loop);
    return G_SOURCE_CONTINUE;
}

static gpointer wake_from_thread(gpointer data)
{
    g_usleep(10000);
    g_source_set_ready_time((GSource *)data, 0);
    return NULL;
}

static void ready_time_source(void)
{
    loop = g_main_loop_new(NULL, FALSE);
    GSource *source = g_source_new(&run_loop_funcs, sizeof(RunLoopSource));
    RunLoopSource *self = (RunLoopSource *)source;
    self->dispatched = 0;
    g_source_set_name(source, "[WebKit] RunLoop work");
    g_source_set_priority(source, G_PRIORITY_DEFAULT);
    g_source_set_callback(source, run_loop_callback, self, NULL);
    g_source_attach(source, NULL);
    printf("ready time %ld priority %d\n", (long)g_source_get_ready_time(source), g_source_get_priority(source));
    g_source_set_ready_time(source, 0);
    GThread *thread = NULL;
    /* The first dispatch happens on its own; the second only once the
       other thread sets the ready time again, waking the poll. */
    g_main_context_iteration(NULL, FALSE);
    thread = g_thread_new("waker", wake_from_thread, source);
    g_main_loop_run(loop);
    g_thread_join(thread);
    printf("destroyed %d\n", g_source_is_destroyed(source));
    g_source_destroy(source);
    printf("destroyed %d\n", g_source_is_destroyed(source));
    g_source_unref(source);
    g_main_loop_unref(loop);
}

static GMainLoop *outer;

static gboolean quit_inner(gpointer data)
{
    printf("quit inner at depth %d\n", g_main_depth());
    g_main_loop_quit((GMainLoop *)data);
    return G_SOURCE_REMOVE;
}

static gboolean run_nested(gpointer data)
{
    printf("nested start depth %d\n", g_main_depth());
    GMainLoop *inner = g_main_loop_new(NULL, FALSE);
    g_timeout_add(5, quit_inner, inner);
    g_main_loop_run(inner);
    g_main_loop_unref(inner);
    printf("nested end depth %d\n", g_main_depth());
    g_main_loop_quit(outer);
    return G_SOURCE_REMOVE;
}

static void nested(void)
{
    outer = g_main_loop_new(NULL, FALSE);
    printf("depth outside %d\n", g_main_depth());
    g_idle_add(run_nested, NULL);
    g_main_loop_run(outer);
    g_main_loop_unref(outer);
}

static gboolean in_context(gpointer data)
{
    printf("invoked in %s context, thread default %d\n", (const char *)data,
        g_main_context_get_thread_default() == NULL ? 0 : 1);
    return G_SOURCE_REMOVE;
}

static GMainContext *worker_context;
static GMainLoop *worker_loop;

static gboolean stop_worker(gpointer data)
{
    printf("worker stops, its context is thread default %d\n",
        g_main_context_get_thread_default() == worker_context);
    g_main_loop_quit(worker_loop);
    return G_SOURCE_REMOVE;
}

static gpointer worker(gpointer data)
{
    g_main_context_push_thread_default(worker_context);
    g_main_loop_run(worker_loop);
    g_main_context_pop_thread_default(worker_context);
    return GINT_TO_POINTER(7);
}

static void thread_default(void)
{
    GMainContext *context = g_main_context_new();
    printf("default before push %d\n", g_main_context_get_thread_default() == NULL);
    g_main_context_push_thread_default(context);
    printf("pushed %d\n", g_main_context_get_thread_default() == context);
    GMainContext *ref = g_main_context_ref_thread_default();
    printf("ref is pushed %d\n", ref == context);
    g_main_context_unref(ref);
    g_main_context_invoke(context, in_context, "pushed");
    g_main_context_pop_thread_default(context);
    printf("popped default %d\n", g_main_context_get_thread_default() == NULL);
    ref = g_main_context_ref_thread_default();
    printf("ref after pop is global default %d\n", ref == g_main_context_default());
    g_main_context_unref(ref);
    printf("pending %d\n", g_main_context_pending(context));
    while (g_main_context_iteration(context, FALSE)) { }
    g_main_context_unref(context);

    worker_context = g_main_context_new();
    worker_loop = g_main_loop_new(worker_context, FALSE);
    GThread *thread = g_thread_new("worker", worker, NULL);
    GSource *source = g_timeout_source_new(10);
    g_source_set_callback(source, stop_worker, NULL, NULL);
    g_source_attach(source, worker_context);
    g_source_unref(source);
    printf("worker returned %d\n", GPOINTER_TO_INT(g_thread_join(thread)));
    g_main_loop_unref(worker_loop);
    g_main_context_unref(worker_context);
}

static gboolean manual_dispatch(GSource *source, GSourceFunc callback, gpointer data)
{
    printf("manual source dispatched\n");
    g_source_set_ready_time(source, -1);
    return G_SOURCE_CONTINUE;
}

static GSourceFuncs manual_funcs = { NULL, NULL, manual_dispatch, NULL, NULL, NULL };

static void by_hand(void)
{
    GMainContext *context = g_main_context_new();
    GSource *source = g_source_new(&manual_funcs, sizeof(GSource));
    g_source_attach(source, context);
    gboolean owner = g_main_context_acquire(context);
    printf("acquired %d owner %d\n", owner, g_main_context_is_owner(context));
    for (int round = 0; round < 2; round++) {
        if (round == 1)
            g_source_set_ready_time(source, g_get_monotonic_time() - 1000);
        gint priority = 0;
        gboolean ready = g_main_context_prepare(context, &priority);
        GPollFD fds[8];
        gint timeout = 0;
        gint n = g_main_context_query(context, priority, &timeout, fds, 8);
        printf("round %d prepare %d fds %d timeout %d\n", round, ready, n > 0, timeout);
        GPollFunc poll_func = g_main_context_get_poll_func(context);
        if (!ready)
            poll_func(fds, n, 0);
        gboolean dispatch = g_main_context_check(context, priority, fds, n);
        printf("round %d check %d\n", round, dispatch);
        if (dispatch)
            g_main_context_dispatch(context);
    }
    g_main_context_release(context);
    g_source_destroy(source);
    g_source_unref(source);
    g_main_context_unref(context);
}

static gboolean never(gpointer data)
{
    return G_SOURCE_CONTINUE;
}

static char some_data[] = "data";

static void bookkeeping(void)
{
    guint id = g_timeout_add_full(G_PRIORITY_DEFAULT, 1000, never, some_data, destroyed);
    GSource *found = g_main_context_find_source_by_id(NULL, id);
    printf("found %d id matches %d\n", found != NULL, found && g_source_get_id(found) == id);
    printf("by user data %d\n", g_main_context_find_source_by_user_data(NULL, some_data) == found);
    printf("remove %d\n", g_source_remove(id));
    printf("found after remove %d\n", g_main_context_find_source_by_id(NULL, id) != NULL);
    printf("iteration without work %d\n", g_main_context_iteration(NULL, FALSE));

    guint seconds = g_timeout_add_seconds(1, never, NULL);
    GSource *s = g_main_context_find_source_by_id(NULL, seconds);
    printf("seconds source name %s\n", g_source_get_name(s) ? g_source_get_name(s) : "(none)");
    g_source_remove(seconds);

    GSource *idle = g_idle_source_new();
    printf("idle name %s priority %d\n", g_source_get_name(idle), g_source_get_priority(idle));
    g_source_unref(idle);
    GSource *timeout = g_timeout_source_new(1);
    printf("timeout name %s\n", g_source_get_name(timeout) ? g_source_get_name(timeout) : "(none)");
    g_source_unref(timeout);

    guint handle = g_idle_add(never, NULL);
    g_clear_handle_id(&handle, g_source_remove);
    printf("cleared handle %u\n", handle);
    printf("idle remove by data %d\n", g_idle_remove_by_data((gpointer)"nothing"));
}

static gboolean wake_iteration(gpointer data)
{
    return G_SOURCE_REMOVE;
}

static gpointer waker(gpointer data)
{
    g_usleep(10000);
    g_main_context_wakeup((GMainContext *)data);
    return NULL;
}

static void wakeup(void)
{
    GMainContext *context = g_main_context_new();
    GThread *thread = g_thread_new("wakeup", waker, context);
    gint64 start = g_get_monotonic_time();
    gboolean dispatched = g_main_context_iteration(context, TRUE);
    gint64 took = g_get_monotonic_time() - start;
    printf("woken dispatched %d in time %d\n", dispatched, took >= 5000 && took < 2000000);
    g_thread_join(thread);
    g_main_context_unref(context);
}

int main(void)
{
    printf("== timeouts\n");
    timeouts();
    printf("== idles\n");
    idles();
    printf("== fd sources\n");
    fd_sources();
    printf("== ready time\n");
    ready_time_source();
    printf("== nested\n");
    nested();
    printf("== thread default\n");
    thread_default();
    printf("== by hand\n");
    by_hand();
    printf("== bookkeeping\n");
    bookkeeping();
    printf("== wakeup\n");
    wakeup();
    return 0;
}
