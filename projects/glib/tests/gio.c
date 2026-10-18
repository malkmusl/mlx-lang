/* mlx-glib's GIO: cancellables (signal, descriptor, source), tasks
   (returned at once and from idle sources, in threads, failing,
   cancelled), files (paths and URIs, reading, replacing with etags,
   deleting, making directories, monitors), sockets
   (send, receive, would-block, timeouts, sources, connections), a
   resource bundle (plain and compressed entries, children, the
   registry), D-Bus failing as GIO does without a bus, introspection
   data, applications and errno's codes; gio.expected is what GLib 2.80
   prints for the same program. The bundle below is what
   glib-compile-resources makes of hello.txt ("hello from a resource"),
   big.txt (compressed) and sub/inner.txt under /org/mlx/test. */
#include <gio/gio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static const unsigned char test_resource_data[] = {
    0x47, 0x56, 0x61, 0x72, 0x69, 0x61, 0x6e, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x18, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x08, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x05, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
    0x1a, 0x26, 0x20, 0x5a, 0x02, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00, 0x4c, 0x00,
    0x08, 0x01, 0x00, 0x00, 0x14, 0x01, 0x00, 0x00, 0x7a, 0x03, 0x85, 0xee, 0x00, 0x00, 0x00, 0x00,
    0x14, 0x01, 0x00, 0x00, 0x07, 0x00, 0x76, 0x00, 0x20, 0x01, 0x00, 0x00, 0x51, 0x01, 0x00, 0x00,
    0x2b, 0x1e, 0xc3, 0x00, 0x04, 0x00, 0x00, 0x00, 0x51, 0x01, 0x00, 0x00, 0x04, 0x00, 0x4c, 0x00,
    0x58, 0x01, 0x00, 0x00, 0x5c, 0x01, 0x00, 0x00, 0x33, 0xb1, 0xda, 0x1b, 0x00, 0x00, 0x00, 0x00,
    0x5c, 0x01, 0x00, 0x00, 0x04, 0x00, 0x4c, 0x00, 0x60, 0x01, 0x00, 0x00, 0x64, 0x01, 0x00, 0x00,
    0x4b, 0x50, 0x90, 0x0b, 0x06, 0x00, 0x00, 0x00, 0x64, 0x01, 0x00, 0x00, 0x04, 0x00, 0x4c, 0x00,
    0x68, 0x01, 0x00, 0x00, 0x6c, 0x01, 0x00, 0x00, 0x9c, 0x24, 0xb7, 0xbe, 0x00, 0x00, 0x00, 0x00,
    0x6c, 0x01, 0x00, 0x00, 0x09, 0x00, 0x76, 0x00, 0x78, 0x01, 0x00, 0x00, 0x9e, 0x01, 0x00, 0x00,
    0xd4, 0xb5, 0x02, 0x00, 0xff, 0xff, 0xff, 0xff, 0x9e, 0x01, 0x00, 0x00, 0x01, 0x00, 0x4c, 0x00,
    0xa0, 0x01, 0x00, 0x00, 0xa4, 0x01, 0x00, 0x00, 0x5d, 0xcf, 0x29, 0x80, 0x03, 0x00, 0x00, 0x00,
    0xa4, 0x01, 0x00, 0x00, 0x09, 0x00, 0x76, 0x00, 0xb0, 0x01, 0x00, 0x00, 0xc7, 0x01, 0x00, 0x00,
    0x74, 0x65, 0x73, 0x74, 0x2f, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x62, 0x69, 0x67, 0x2e, 0x74, 0x78, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xd0, 0x02, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x78, 0xda, 0x2b, 0x4a, 0x2d, 0x48, 0x4d, 0x2c,
    0x51, 0xc8, 0x4d, 0x55, 0x28, 0xc8, 0x49, 0x4d, 0x2c, 0x4e, 0xd5, 0x53, 0x28, 0x1a, 0x15, 0x19,
    0x15, 0x19, 0xaa, 0x22, 0x5c, 0x00, 0xb1, 0x44, 0xfe, 0x23, 0x00, 0x28, 0x75, 0x75, 0x61, 0x79,
    0x29, 0x6d, 0x6c, 0x78, 0x2f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x73, 0x75, 0x62, 0x2f,
    0x07, 0x00, 0x00, 0x00, 0x6f, 0x72, 0x67, 0x2f, 0x02, 0x00, 0x00, 0x00, 0x68, 0x65, 0x6c, 0x6c,
    0x6f, 0x2e, 0x74, 0x78, 0x74, 0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x66, 0x72, 0x6f, 0x6d, 0x20, 0x61, 0x20, 0x72, 0x65, 0x73,
    0x6f, 0x75, 0x72, 0x63, 0x65, 0x0a, 0x00, 0x00, 0x28, 0x75, 0x75, 0x61, 0x79, 0x29, 0x2f, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x69, 0x6e, 0x6e, 0x65, 0x72, 0x2e, 0x74, 0x78, 0x74, 0x00, 0x00, 0x00,
    0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6e, 0x65, 0x73, 0x74, 0x65, 0x64, 0x0a, 0x00,
    0x00, 0x28, 0x75, 0x75, 0x61, 0x79, 0x29,
};

static GMainLoop *loop;

static void on_cancelled(GCancellable *cancellable, gpointer data)
{
    printf("cancelled handler %s\n", (const char *)data);
}

static gboolean cancel_source_fired(GCancellable *cancellable, gpointer data)
{
    printf("cancellable source fired, cancelled %d\n", g_cancellable_is_cancelled(cancellable));
    g_main_loop_quit(loop);
    return G_SOURCE_REMOVE;
}

static gboolean cancel_later(gpointer data)
{
    g_cancellable_cancel(data);
    return G_SOURCE_REMOVE;
}

static void cancellables(void)
{
    GCancellable *c = g_cancellable_new();
    printf("type %s cancelled %d\n", G_OBJECT_TYPE_NAME(c), g_cancellable_is_cancelled(c));
    gulong id = g_cancellable_connect(c, G_CALLBACK(on_cancelled), "connected", NULL);
    g_signal_connect(c, "cancelled", G_CALLBACK(on_cancelled), "signal");
    int fd = g_cancellable_get_fd(c);
    GPollFD pfd;
    g_cancellable_make_pollfd(c, &pfd);
    printf("fd same %d readable before %d\n", fd == pfd.fd, g_poll(&pfd, 1, 0));
    g_cancellable_cancel(c);
    pfd.revents = 0;
    printf("readable after %d\n", g_poll(&pfd, 1, 0));
    g_cancellable_release_fd(c);
    g_cancellable_release_fd(c);
    GError *error = NULL;
    gboolean set = g_cancellable_set_error_if_cancelled(c, &error);
    printf("set error %d: %s (cancelled code %d)\n", set, error->message, g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED));
    g_clear_error(&error);
    printf("connect after cancel id %lu\n", g_cancellable_connect(c, G_CALLBACK(on_cancelled), "late", NULL));
    g_cancellable_disconnect(c, id);
    g_cancellable_reset(c);
    printf("after reset %d\n", g_cancellable_is_cancelled(c));
    g_cancellable_push_current(c);
    printf("current is c %d\n", g_cancellable_get_current() == c);
    g_cancellable_pop_current(c);
    printf("current after pop %p\n", (void *)g_cancellable_get_current());

    loop = g_main_loop_new(NULL, FALSE);
    GSource *source = g_cancellable_source_new(c);
    g_source_set_callback(source, (GSourceFunc)(void (*)(void))cancel_source_fired, NULL, NULL);
    g_source_attach(source, NULL);
    g_source_unref(source);
    g_timeout_add(5, cancel_later, c);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);
    g_object_unref(c);
}

static void task_done(GObject *source, GAsyncResult *result, gpointer data)
{
    GError *error = NULL;
    printf("callback %s: source %s user data match %d tagged %d", (const char *)data, source ? G_OBJECT_TYPE_NAME(source) : "(none)",
        g_async_result_get_user_data(result) == data, g_async_result_is_tagged(result, task_done));
    if (strcmp(data, "int") == 0)
        printf(" int %ld", (long)g_task_propagate_int(G_TASK(result), &error));
    else if (strcmp(data, "boolean") == 0)
        printf(" boolean %d", g_task_propagate_boolean(G_TASK(result), &error));
    else {
        char *value = g_task_propagate_pointer(G_TASK(result), &error);
        printf(" pointer %s", value ? value : "(null)");
        g_free(value);
    }
    printf(" error %s had error %d\n", error ? error->message : "-", g_task_had_error(G_TASK(result)));
    g_clear_error(&error);
}

static void thread_work(GTask *task, gpointer source, gpointer task_data, GCancellable *cancellable)
{
    g_task_return_pointer(task, g_strdup_printf("computed %d", GPOINTER_TO_INT(task_data)), g_free);
}

static int pending_tasks;

static void count_done(GObject *source, GAsyncResult *result, gpointer data)
{
    task_done(source, result, data);
    if (--pending_tasks == 0)
        g_main_loop_quit(loop);
}

static void tasks(void)
{
    loop = g_main_loop_new(NULL, FALSE);
    GObject *owner = g_object_new(G_TYPE_OBJECT, NULL);
    pending_tasks = 4;
    GTask *task = g_task_new(owner, NULL, count_done, "int");
    g_task_set_source_tag(task, task_done);
    printf("valid %d source same %d\n", g_task_is_valid(task, owner), g_task_get_source_object(task) == owner);
    g_task_return_int(task, 42);
    printf("completed right away %d\n", g_task_get_completed(task));
    g_object_unref(task);
    task = g_task_new(NULL, NULL, count_done, "boolean");
    g_task_return_boolean(task, TRUE);
    g_object_unref(task);
    task = g_task_new(owner, NULL, count_done, "error");
    g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "nothing at %s", "/x");
    g_object_unref(task);
    GCancellable *c = g_cancellable_new();
    g_cancellable_cancel(c);
    task = g_task_new(owner, c, count_done, "cancelled");
    g_task_return_pointer(task, g_strdup("ignored"), g_free);
    g_object_unref(task);
    g_object_unref(c);
    g_main_loop_run(loop);
    /* Alone, so that its callback comes in a known order. */
    pending_tasks = 1;
    task = g_task_new(owner, NULL, count_done, "thread");
    g_task_set_task_data(task, GINT_TO_POINTER(7), NULL);
    g_task_run_in_thread(task, thread_work);
    g_object_unref(task);
    g_main_loop_run(loop);
    task = g_task_new(owner, NULL, NULL, NULL);
    g_task_run_in_thread_sync(task, thread_work);
    char *value = g_task_propagate_pointer(task, NULL);
    printf("sync result %s completed %d\n", value, g_task_get_completed(task));
    g_free(value);
    g_object_unref(task);
    g_object_unref(owner);
    g_main_loop_unref(loop);
}

static void file_changed(GFileMonitor *monitor, GFile *file, GFile *other, GFileMonitorEvent event, gpointer data)
{
    char *name = g_file_get_basename(file);
    /* The temporary file of g_file_set_contents has a random suffix. */
    if (g_str_has_prefix(name, "watched.txt.")) {
        g_free(name);
        name = g_strdup("watched.txt.(temporary)");
    }
    GEnumClass *events = g_type_class_ref(g_type_from_name("GFileMonitorEvent"));
    GEnumValue *value = g_enum_get_value(events, event);
    printf("monitor %s: %s %s\n", (const char *)data, name, value ? value->value_nick : "?");
    g_type_class_unref(events);
    g_free(name);
}

static gboolean stop_loop(gpointer data)
{
    g_main_loop_quit(loop);
    return G_SOURCE_REMOVE;
}

static void run_for(int ms)
{
    g_timeout_add(ms, stop_loop, NULL);
    g_main_loop_run(loop);
}

static void files(void)
{
    char *dir = g_dir_make_tmp("mlx-gio-XXXXXX", NULL);
    GFile *root = g_file_new_for_path(dir);
    GFile *file = g_file_get_child(root, "note.txt");
    GFile *same = g_file_new_for_path("/tmp/../tmp/./x/../y");
    char *path = g_file_get_path(same);
    printf("canonical %s native %d scheme %s\n", path, g_file_is_native(same), g_file_get_uri_scheme(same));
    g_free(path);
    GFile *uri = g_file_new_for_uri("file:///tmp/a%20b/c.txt");
    path = g_file_get_path(uri);
    char *back = g_file_get_uri(uri);
    char *base = g_file_get_basename(uri);
    printf("from uri %s back %s basename %s\n", path, back, base);
    g_free(path);
    g_free(back);
    g_free(base);
    GFile *parent = g_file_get_parent(uri);
    char *relative = g_file_get_relative_path(parent, uri);
    printf("parent has child %d prefix %d relative %s equal %d\n", g_file_has_parent(uri, parent), g_file_has_prefix(uri, parent), relative,
        g_file_equal(parent, uri));
    g_free(relative);
    GFile *web = g_file_new_for_uri("https://example.com/path/page.html?q=1");
    base = g_file_get_basename(web);
    back = g_file_get_uri(web);
    printf("web native %d path %p uri %s basename %s has https %d\n", g_file_is_native(web), (void *)g_file_get_path(web), back, base,
        g_file_has_uri_scheme(web, "HTTPS"));
    g_free(base);
    g_free(back);
    GFile *built = g_file_new_build_filename("/usr", "share", "doc", NULL);
    path = g_file_get_path(built);
    GFile *resolved = g_file_resolve_relative_path(built, "../lib/./x");
    char *resolved_path = g_file_get_path(resolved);
    printf("built %s resolved %s\n", path, resolved_path);
    g_free(path);
    g_free(resolved_path);

    loop = g_main_loop_new(NULL, FALSE);
    GError *error = NULL;
    GFileMonitor *monitor = g_file_monitor_directory(root, G_FILE_MONITOR_NONE, NULL, &error);
    g_signal_connect(monitor, "changed", G_CALLBACK(file_changed), "dir");
    printf("exists before %d type %d\n", g_file_query_exists(file, NULL), g_file_query_file_type(file, 0, NULL));
    char *etag = NULL;
    gboolean ok = g_file_replace_contents(file, "first", 5, NULL, FALSE, G_FILE_CREATE_NONE, &etag, NULL, &error);
    printf("replace %d etag set %d\n", ok, etag != NULL);
    char *contents;
    gsize length;
    char *etag2 = NULL;
    ok = g_file_load_contents(file, NULL, &contents, &length, &etag2, &error);
    printf("load %d '%s' %lu etags equal %d\n", ok, contents, (unsigned long)length, g_strcmp0(etag, etag2) == 0);
    g_free(contents);
    g_free(etag);
    g_free(etag2);
    ok = g_file_replace_contents(file, "x", 1, "0:0", FALSE, G_FILE_CREATE_NONE, NULL, NULL, &error);
    printf("wrong etag %d code %d %s\n", ok, error->code, error->message);
    g_clear_error(&error);
    GBytes *bytes = g_file_load_bytes(file, NULL, NULL, &error);
    printf("bytes %lu\n", (unsigned long)g_bytes_get_size(bytes));
    g_bytes_unref(bytes);
    run_for(150);
    GFile *missing = g_file_get_child(root, "missing.txt");
    ok = g_file_load_contents(missing, NULL, &contents, NULL, NULL, &error);
    char *prefix = g_strdup_printf("Error opening file %s/missing.txt: ", dir);
    printf("missing %d code %d message ok %d\n", ok, error->code, g_str_has_prefix(error->message, prefix));
    g_free(prefix);
    g_clear_error(&error);
    GFile *nested = g_file_resolve_relative_path(root, "a/b/c");
    printf("make directory with parents %d", g_file_make_directory_with_parents(nested, NULL, &error));
    ok = g_file_make_directory_with_parents(nested, NULL, &error);
    printf(" again %d code %d\n", ok, error ? error->code : -1);
    g_clear_error(&error);
    printf("directory type %d\n", g_file_query_file_type(nested, 0, NULL));
    run_for(150);
    printf("delete %d", g_file_delete(file, NULL, &error));
    ok = g_file_delete(file, NULL, &error);
    printf(" again %d code %d\n", ok, error ? error->code : -1);
    g_clear_error(&error);
    run_for(150);
    g_file_monitor_cancel(monitor);
    printf("monitor cancelled %d\n", g_file_monitor_is_cancelled(monitor));
    g_object_unref(monitor);

    GFile *watched = g_file_get_child(root, "watched.txt");
    monitor = g_file_monitor_file(watched, G_FILE_MONITOR_NONE, NULL, &error);
    g_file_monitor_set_rate_limit(monitor, 100);
    g_signal_connect(monitor, "changed", G_CALLBACK(file_changed), "file");
    g_file_replace_contents(g_file_get_child(root, "other.txt"), "o", 1, NULL, FALSE, 0, NULL, NULL, NULL);
    g_file_set_contents(g_file_peek_path(watched), "1", 1, NULL);
    run_for(150);
    /* A write: the change, then the hint when it ends. (When changes
       that follow one another are sent depends on the timing of GIO's
       inotify reader, so they are not part of this test.) */
    int fd = open(g_file_peek_path(watched), O_WRONLY | O_APPEND);
    write(fd, "2", 1);
    close(fd);
    run_for(150);
    g_object_unref(monitor);

    char *rm = g_strdup_printf("rm -rf %s", dir);
    g_spawn_command_line_sync(rm, NULL, NULL, NULL, NULL);
    g_free(rm);
    g_free(dir);
    g_main_loop_unref(loop);
}

static gboolean socket_ready(GSocket *socket, GIOCondition condition, gpointer data)
{
    char buffer[32] = { 0 };
    gssize n = g_socket_receive(socket, buffer, sizeof buffer - 1, NULL, NULL);
    printf("socket source condition in %d received %ld '%s'\n", (condition & G_IO_IN) != 0, (long)n, buffer);
    g_main_loop_quit(loop);
    return G_SOURCE_REMOVE;
}

static void sockets(void)
{
    int fds[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    GError *error = NULL;
    GSocket *a = g_socket_new_from_fd(fds[0], &error);
    GSocket *b = g_socket_new_from_fd(fds[1], &error);
    printf("family %d type %d connected %d blocking %d fd %d\n", g_socket_get_family(a), g_socket_get_socket_type(a), g_socket_is_connected(a),
        g_socket_get_blocking(a), g_socket_get_fd(a) == fds[0]);
    printf("sent %ld\n", (long)g_socket_send(a, "hello", 5, NULL, &error));
    char buffer[32] = { 0 };
    long available = (long)g_socket_get_available_bytes(b);
    long received = (long)g_socket_receive(b, buffer, sizeof buffer, NULL, &error);
    printf("available %ld received %ld '%s'\n", available, received, buffer);
    g_socket_set_blocking(b, FALSE);
    gssize n = g_socket_receive(b, buffer, sizeof buffer, NULL, &error);
    printf("would block %ld code %d %s\n", (long)n, error->code, error->message);
    g_clear_error(&error);
    printf("condition out %d in %d\n", g_socket_condition_check(a, G_IO_OUT) == G_IO_OUT, g_socket_condition_check(b, G_IO_IN));
    g_socket_set_timeout(b, 1);
    g_socket_set_blocking(b, TRUE);
    printf("timed wait %d", g_socket_condition_timed_wait(b, G_IO_IN, 10000, NULL, &error));
    printf(" code %d %s\n", error->code, error->message);
    g_clear_error(&error);
    loop = g_main_loop_new(NULL, FALSE);
    GSource *source = g_socket_create_source(b, G_IO_IN, NULL);
    g_source_set_callback(source, (GSourceFunc)(void (*)(void))socket_ready, NULL, NULL);
    g_source_attach(source, NULL);
    g_source_unref(source);
    g_socket_send(a, "later", 5, NULL, NULL);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);
    GSocketConnection *connection = g_socket_connection_factory_create_connection(a);
    printf("connection %s socket same %d connected %d\n", G_OBJECT_TYPE_NAME(connection), g_socket_connection_get_socket(connection) == a,
        g_socket_connection_is_connected(connection));
    gboolean closed = g_io_stream_close(G_IO_STREAM(connection), NULL, &error);
    printf("close %d closed %d socket closed %d\n", closed, g_io_stream_is_closed(G_IO_STREAM(connection)), g_socket_is_closed(a));
    long sent = (long)g_socket_send(a, "x", 1, NULL, &error);
    printf("send after close %ld %s\n", sent, error->message);
    g_clear_error(&error);
    g_object_unref(connection);
    GSocket *fresh = g_socket_new(G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_TCP, &error);
    printf("new socket %d family %d keepalive %d", fresh != NULL, g_socket_get_family(fresh), g_socket_get_keepalive(fresh));
    g_socket_set_keepalive(fresh, TRUE);
    printf(" after %d\n", g_socket_get_keepalive(fresh));
    g_object_unref(fresh);
    GSocket *bad = g_socket_new_from_fd(open("/dev/null", O_RDONLY), &error);
    printf("not a socket %p code %d %s\n", (void *)bad, error->code, error->message);
    g_clear_error(&error);
    g_object_unref(a);
    g_object_unref(b);
}

static void resources(void)
{
    guint8 *copy = g_memdup2(test_resource_data, sizeof test_resource_data);
    GBytes *data = g_bytes_new_take(copy, sizeof test_resource_data);
    GError *error = NULL;
    GResource *resource = g_resource_new_from_data(data, &error);
    printf("resource %d\n", resource != NULL);
    GBytes *hello = g_resource_lookup_data(resource, "/org/mlx/test/hello.txt", 0, &error);
    printf("hello %lu '%.*s'\n", (unsigned long)g_bytes_get_size(hello), (int)g_bytes_get_size(hello) - 1, (const char *)g_bytes_get_data(hello, NULL));
    g_bytes_unref(hello);
    gsize size;
    guint32 flags;
    printf("info %d", g_resource_get_info(resource, "/org/mlx/test/big.txt", 0, &size, &flags, &error));
    printf(" size %lu compressed %u\n", (unsigned long)size, flags);
    GBytes *big = g_resource_lookup_data(resource, "/org/mlx/test/big.txt", 0, &error);
    printf("big %lu starts '%.17s' error %s\n", (unsigned long)g_bytes_get_size(big), (const char *)g_bytes_get_data(big, NULL), error ? error->message : "-");
    g_clear_error(&error);
    g_bytes_unref(big);
    char **children = g_resource_enumerate_children(resource, "/org/mlx/test", 0, &error);
    printf("children");
    for (int i = 0; children && children[i]; i++)
        printf(" %s", children[i]);
    printf("\n");
    g_strfreev(children);
    GBytes *none = g_resource_lookup_data(resource, "/org/mlx/test/none", 0, &error);
    printf("none %p domain %d code %d %s\n", (void *)none, error->domain == G_RESOURCE_ERROR, error->code, error->message);
    g_clear_error(&error);
    g_resources_register(resource);
    GBytes *inner = g_resources_lookup_data("/org/mlx/test/sub/inner.txt", 0, &error);
    printf("registered inner '%.6s'\n", (const char *)g_bytes_get_data(inner, NULL));
    g_bytes_unref(inner);
    g_resources_unregister(resource);
    inner = g_resources_lookup_data("/org/mlx/test/sub/inner.txt", 0, &error);
    printf("after unregister %p %s\n", (void *)inner, error->message);
    g_clear_error(&error);
    g_resource_unref(resource);
    g_bytes_unref(data);
    GBytes *junk = g_bytes_new_static("not a resource bundle at all", 28);
    resource = g_resource_new_from_data(junk, &error);
    printf("junk %p %s\n", (void *)resource, error->message);
    g_clear_error(&error);
    g_bytes_unref(junk);
    printf("boxed %s\n", g_type_name(G_TYPE_RESOURCE));
}

static void proxy_ready(GObject *source, GAsyncResult *result, gpointer data)
{
    GError *error = NULL;
    GDBusProxy *proxy = g_dbus_proxy_new_for_bus_finish(result, &error);
    printf("async proxy %p: %s code %d\n", (void *)proxy, error->message, error->code);
    g_clear_error(&error);
    g_main_loop_quit(loop);
}

static const char introspection[] =
    "<node>"
    "  <interface name='org.example.Thing'>"
    "    <method name='Frob'><arg name='a' type='s' direction='in'/><arg name='b' type='i' direction='out'/></method>"
    "    <signal name='Changed'><arg name='value' type='v'/></signal>"
    "    <property name='Count' type='u' access='read'><annotation name='org.example.Note' value='yes'/></property>"
    "  </interface>"
    "  <node name='child'/>"
    "</node>";

static void dbus(void)
{
    GError *error = NULL;
    g_unsetenv("DBUS_SESSION_BUS_ADDRESS");
    g_unsetenv("DISPLAY");
    g_setenv("XDG_RUNTIME_DIR", "/nonexistent-runtime", TRUE);
    g_setenv("DBUS_SYSTEM_BUS_ADDRESS", "unix:path=/nonexistent/system_bus_socket", TRUE);
    GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    printf("session %p io %d code %d: %s\n", (void *)bus, error->domain == G_IO_ERROR, error->code, error->message);
    g_clear_error(&error);
    GDBusProxy *proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, 0, NULL, "org.freedesktop.timedate1", "/org/freedesktop/timedate1",
        "org.freedesktop.timedate1", NULL, &error);
    printf("system proxy %p code %d: %s\n", (void *)proxy, error->code, error->message);
    g_clear_error(&error);
    char *address = g_dbus_address_get_for_bus_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    printf("system address %s\n", address);
    g_free(address);
    g_setenv("DBUS_SESSION_BUS_ADDRESS", "bogus", TRUE);
    bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    printf("bogus %p code %d: %s\n", (void *)bus, error->code, error->message);
    g_clear_error(&error);
    loop = g_main_loop_new(NULL, FALSE);
    g_dbus_proxy_new_for_bus(G_BUS_TYPE_SYSTEM, 0, NULL, "a.b", "/a", "a.b", NULL, proxy_ready, NULL);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);
    GError *remote = g_error_new_literal(G_IO_ERROR, G_IO_ERROR_DBUS_ERROR, "GDBus.Error:org.example.Failed: it broke");
    char *name = g_dbus_error_get_remote_error(remote);
    printf("remote %d name %s", g_dbus_error_is_remote_error(remote), name);
    g_free(name);
    g_dbus_error_strip_remote_error(remote);
    printf(" stripped '%s'\n", remote->message);
    g_error_free(remote);
    GDBusNodeInfo *node = g_dbus_node_info_new_for_xml(introspection, &error);
    GDBusInterfaceInfo *iface = g_dbus_node_info_lookup_interface(node, "org.example.Thing");
    GDBusMethodInfo *method = g_dbus_interface_info_lookup_method(iface, "Frob");
    GDBusSignalInfo *signal = g_dbus_interface_info_lookup_signal(iface, "Changed");
    GDBusPropertyInfo *property = g_dbus_interface_info_lookup_property(iface, "Count");
    printf("node %s child %s method in %s:%s out %s:%s signal arg %s property %s flags %d annotation %s=%s missing %p\n", iface->name,
        node->nodes[0]->path, method->in_args[0]->name, method->in_args[0]->signature, method->out_args[0]->name, method->out_args[0]->signature,
        signal->args[0]->signature, property->signature, property->flags, property->annotations[0]->key, property->annotations[0]->value,
        (void *)g_dbus_interface_info_lookup_method(iface, "Nope"));
    g_dbus_node_info_ref(node);
    g_dbus_node_info_unref(node);
    g_dbus_node_info_unref(node);
    node = g_dbus_node_info_new_for_xml("<node><interface name='x'>", &error);
    printf("bad xml %p %s\n", (void *)node, error ? "error" : "no error");
    g_clear_error(&error);
}

static void applications(void)
{
    printf("default %p\n", (void *)g_application_get_default());
    const char *ids[] = { "org.example.App", "noDots", "org..double", "_ok.x-y", "1.starts.digit", "trailing.", "a.b" };
    for (unsigned i = 0; i < G_N_ELEMENTS(ids); i++)
        printf("id %s %d\n", ids[i], g_application_id_is_valid(ids[i]));
    GApplication *app = g_application_new("org.example.App", G_APPLICATION_NON_UNIQUE);
    printf("app id %s flags %d default is app %d\n", g_application_get_application_id(app), g_application_get_flags(app),
        g_application_get_default() == app);
    g_object_unref(app);
    printf("default after unref %p\n", (void *)g_application_get_default());
    printf("io errors %d %d %d %d %d\n", g_io_error_from_errno(ENOENT), g_io_error_from_errno(EEXIST), g_io_error_from_errno(EAGAIN),
        g_io_error_from_errno(9999), g_io_error_from_file_error(G_FILE_ERROR_NOENT));
}

int main(void)
{
    printf("== cancellables\n");
    cancellables();
    printf("== tasks\n");
    tasks();
    printf("== files\n");
    files();
    printf("== sockets\n");
    sockets();
    printf("== resources\n");
    resources();
    printf("== dbus\n");
    dbus();
    printf("== applications\n");
    applications();
    return 0;
}
