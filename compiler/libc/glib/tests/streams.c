/* GIO's streams: memory input and output streams read, skipped, written
   and closed, synchronously and asynchronously; their errors (a closed
   stream, an outstanding operation); splicing; a filter stream over a
   memory stream; a stream of the program's own with only read_fn, read
   asynchronously (in a thread); pollable streams and their sources; the
   socket streams of a connection over a socket pair, read before and
   after the other end writes; GSimpleIOStream. */
#include <gio/gio.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static GMainLoop *loop;

static void show_error(const char *what, GError *error)
{
    if (error)
        printf("%s: error %s %d: %s\n", what, g_quark_to_string(error->domain), error->code, error->message);
    else
        printf("%s: no error\n", what);
}

static void read_done(GObject *source, GAsyncResult *result, gpointer data)
{
    GError *error = NULL;
    char *buffer = data;
    gssize n = g_input_stream_read_finish(G_INPUT_STREAM(source), result, &error);
    printf("read_async: %ld \"%.*s\" pending=%d\n", (long)n, n > 0 ? (int)n : 0, buffer, g_input_stream_has_pending(G_INPUT_STREAM(source)));
    if (error) {
        show_error("read_async", error);
        g_error_free(error);
    }
    g_main_loop_quit(loop);
}

static void read_bytes_done(GObject *source, GAsyncResult *result, gpointer data)
{
    GError *error = NULL;
    GBytes *bytes = g_input_stream_read_bytes_finish(G_INPUT_STREAM(source), result, &error);
    gsize size = 0;
    const char *text = bytes ? g_bytes_get_data(bytes, &size) : NULL;
    printf("read_bytes_async: %lu \"%.*s\"\n", (unsigned long)size, (int)size, text ? text : "");
    if (bytes)
        g_bytes_unref(bytes);
    g_main_loop_quit(loop);
}

static void close_done(GObject *source, GAsyncResult *result, gpointer data)
{
    GError *error = NULL;
    gboolean ok = g_input_stream_close_finish(G_INPUT_STREAM(source), result, &error);
    printf("close_async: %d closed=%d\n", ok, g_input_stream_is_closed(G_INPUT_STREAM(source)));
    g_main_loop_quit(loop);
}

static void skip_done(GObject *source, GAsyncResult *result, gpointer data)
{
    GError *error = NULL;
    gssize n = g_input_stream_skip_finish(G_INPUT_STREAM(source), result, &error);
    printf("skip_async: %ld\n", (long)n);
    g_main_loop_quit(loop);
}

static void read_all_done(GObject *source, GAsyncResult *result, gpointer data)
{
    GError *error = NULL;
    gsize n = 0;
    gboolean ok = g_input_stream_read_all_finish(G_INPUT_STREAM(source), result, &n, &error);
    printf("read_all_async: %d %lu \"%.*s\"\n", ok, (unsigned long)n, (int)n, (char *)data);
    g_main_loop_quit(loop);
}

static void write_all_done(GObject *source, GAsyncResult *result, gpointer data)
{
    GError *error = NULL;
    gsize n = 0;
    gboolean ok = g_output_stream_write_all_finish(G_OUTPUT_STREAM(source), result, &n, &error);
    printf("write_all_async: %d %lu\n", ok, (unsigned long)n);
    g_main_loop_quit(loop);
}

static void splice_done(GObject *source, GAsyncResult *result, gpointer data)
{
    GError *error = NULL;
    gssize n = g_output_stream_splice_finish(G_OUTPUT_STREAM(source), result, &error);
    printf("splice_async: %ld closed=%d\n", (long)n, g_output_stream_is_closed(G_OUTPUT_STREAM(source)));
    g_main_loop_quit(loop);
}

static void out_close_done(GObject *source, GAsyncResult *result, gpointer data)
{
    GError *error = NULL;
    gboolean ok = g_output_stream_close_finish(G_OUTPUT_STREAM(source), result, &error);
    printf("output close_async: %d closed=%d\n", ok, g_output_stream_is_closed(G_OUTPUT_STREAM(source)));
    g_main_loop_quit(loop);
}

/* A stream of the program's own: read_fn alone, giving "abc" a byte at a
   time from whatever thread reads it. */
typedef struct { GInputStream parent; int at; } SlowStream;
typedef struct { GInputStreamClass parent_class; } SlowStreamClass;

static gssize slow_read(GInputStream *stream, void *buffer, gsize count, GCancellable *cancellable, GError **error)
{
    SlowStream *self = (SlowStream *)stream;
    if (self->at >= 3)
        return 0;
    ((char *)buffer)[0] = "abc"[self->at++];
    return 1;
}

static void slow_class_init(gpointer klass, gpointer data)
{
    G_INPUT_STREAM_CLASS(klass)->read_fn = slow_read;
}

static GType slow_stream_get_type(void)
{
    static GType type = 0;
    if (!type)
        type = g_type_register_static_simple(G_TYPE_INPUT_STREAM, "SlowStream", sizeof(SlowStreamClass), slow_class_init, sizeof(SlowStream), NULL, 0);
    return type;
}

static gboolean source_ready(GObject *stream, gpointer data)
{
    char buffer[32];
    GError *error = NULL;
    gssize n = g_pollable_input_stream_read_nonblocking(G_POLLABLE_INPUT_STREAM(stream), buffer, sizeof buffer, NULL, &error);
    printf("pollable source: %ld \"%.*s\"\n", (long)n, n > 0 ? (int)n : 0, buffer);
    if (error)
        g_error_free(error);
    g_main_loop_quit(loop);
    return G_SOURCE_REMOVE;
}

static gboolean write_later(gpointer data)
{
    int fd = GPOINTER_TO_INT(data);
    if (write(fd, "late", 4) != 4)
        printf("write failed\n");
    return G_SOURCE_REMOVE;
}

int main(void)
{
    GError *error = NULL;
    char buffer[64];
    loop = g_main_loop_new(NULL, FALSE);

    printf("types: %s %s %s %s\n", g_type_name(G_TYPE_MEMORY_INPUT_STREAM), g_type_name(g_type_parent(G_TYPE_MEMORY_INPUT_STREAM)), g_type_name(G_TYPE_FILTER_INPUT_STREAM), g_type_name(G_TYPE_MEMORY_OUTPUT_STREAM));

    /* Memory input: chunks, reads across them, skip, the end. */
    GInputStream *in = g_memory_input_stream_new_from_data("Hello, ", -1, NULL);
    GBytes *world = g_bytes_new_static("world!", 6);
    g_memory_input_stream_add_bytes(G_MEMORY_INPUT_STREAM(in), world);
    g_bytes_unref(world);
    gssize n = g_input_stream_read(in, buffer, 5, NULL, &error);
    printf("read: %ld \"%.*s\"\n", (long)n, (int)n, buffer);
    n = g_input_stream_skip(in, 2, NULL, &error);
    printf("skip: %ld\n", (long)n);
    n = g_input_stream_read(in, buffer, sizeof buffer, NULL, &error);
    printf("read: %ld \"%.*s\"\n", (long)n, (int)n, buffer);
    n = g_input_stream_read(in, buffer, sizeof buffer, NULL, &error);
    printf("read at the end: %ld\n", (long)n);
    printf("pollable: %d readable: %d\n", G_IS_POLLABLE_INPUT_STREAM(in), g_pollable_input_stream_is_readable(G_POLLABLE_INPUT_STREAM(in)));

    /* An outstanding operation, then a closed stream. */
    g_input_stream_set_pending(in, NULL);
    n = g_input_stream_read(in, buffer, 1, NULL, &error);
    printf("read while pending: %ld\n", (long)n);
    show_error("read while pending", error);
    g_clear_error(&error);
    g_input_stream_clear_pending(in);
    gboolean ok = g_input_stream_close(in, NULL, &error);
    printf("close: %d closed=%d\n", ok, g_input_stream_is_closed(in));
    n = g_input_stream_read(in, buffer, 1, NULL, &error);
    show_error("read when closed", error);
    g_clear_error(&error);
    printf("close again: %d\n", g_input_stream_close(in, NULL, &error));
    g_object_unref(in);

    /* Asynchronously. */
    in = g_memory_input_stream_new_from_data("asynchronous data", -1, NULL);
    memset(buffer, 0, sizeof buffer);
    g_input_stream_read_async(in, buffer, 12, G_PRIORITY_DEFAULT, NULL, read_done, buffer);
    printf("pending during read_async: %d\n", g_input_stream_has_pending(in));
    g_main_loop_run(loop);
    g_input_stream_skip_async(in, 1, G_PRIORITY_DEFAULT, NULL, skip_done, NULL);
    g_main_loop_run(loop);
    g_input_stream_read_bytes_async(in, 10, G_PRIORITY_DEFAULT, NULL, read_bytes_done, NULL);
    g_main_loop_run(loop);
    g_input_stream_read_async(in, buffer, 0, G_PRIORITY_DEFAULT, NULL, read_done, buffer);
    g_main_loop_run(loop);
    g_input_stream_close_async(in, G_PRIORITY_DEFAULT, NULL, close_done, NULL);
    g_main_loop_run(loop);
    g_input_stream_read_async(in, buffer, 4, G_PRIORITY_DEFAULT, NULL, read_done, buffer);
    g_main_loop_run(loop);
    g_object_unref(in);

    in = g_memory_input_stream_new_from_data("read all of it", -1, NULL);
    memset(buffer, 0, sizeof buffer);
    g_input_stream_read_all_async(in, buffer, sizeof buffer, G_PRIORITY_DEFAULT, NULL, read_all_done, buffer);
    g_main_loop_run(loop);
    g_object_unref(in);

    /* A stream with only read_fn: its asynchronous read runs in a thread. */
    GInputStream *slow = g_object_new(slow_stream_get_type(), NULL);
    memset(buffer, 0, sizeof buffer);
    g_input_stream_read_all_async(slow, buffer, 10, G_PRIORITY_DEFAULT, NULL, read_all_done, buffer);
    g_main_loop_run(loop);
    gsize got = 0;
    ok = g_input_stream_read_all(slow, buffer, 4, &got, NULL, NULL);
    printf("read_all at the end: %d %lu\n", ok, (unsigned long)got);
    g_object_unref(slow);

    /* Memory output: growing, closed, stolen. */
    GOutputStream *out = g_memory_output_stream_new_resizable();
    gsize written = 0;
    ok = g_output_stream_write_all(out, "0123456789", 10, &written, NULL, &error);
    printf("write_all: %d %lu\n", ok, (unsigned long)written);
    n = g_output_stream_write(out, "abcdefghijklmnopqrstuvwxyz", 26, NULL, &error);
    printf("write: %ld size=%lu data_size=%lu\n", (long)n, (unsigned long)g_memory_output_stream_get_size(G_MEMORY_OUTPUT_STREAM(out)), (unsigned long)g_memory_output_stream_get_data_size(G_MEMORY_OUTPUT_STREAM(out)));
    g_output_stream_write_all_async(out, "!?", 2, G_PRIORITY_DEFAULT, NULL, write_all_done, NULL);
    g_main_loop_run(loop);
    printf("flush: %d\n", g_output_stream_flush(out, NULL, &error));
    g_output_stream_close_async(out, G_PRIORITY_DEFAULT, NULL, out_close_done, NULL);
    g_main_loop_run(loop);
    n = g_output_stream_write(out, "x", 1, NULL, &error);
    show_error("write when closed", error);
    g_clear_error(&error);
    GBytes *bytes = g_memory_output_stream_steal_as_bytes(G_MEMORY_OUTPUT_STREAM(out));
    printf("stolen: %lu \"%.*s\"\n", (unsigned long)g_bytes_get_size(bytes), (int)g_bytes_get_size(bytes), (const char *)g_bytes_get_data(bytes, NULL));
    g_bytes_unref(bytes);
    g_object_unref(out);

    /* A fixed buffer that is not resizable. */
    char fixed[4];
    out = g_memory_output_stream_new(fixed, sizeof fixed, NULL, NULL);
    n = g_output_stream_write(out, "toolong", 7, NULL, &error);
    printf("write past a fixed buffer: %ld \"%.4s\"\n", (long)n, fixed);
    show_error("write past a fixed buffer", error);
    n = g_output_stream_write(out, "more", 4, NULL, &error);
    printf("write to a full fixed buffer: %ld\n", (long)n);
    show_error("write to a full fixed buffer", error);
    g_clear_error(&error);
    g_object_unref(out);

    /* Splicing, synchronously and asynchronously, with the close flags. */
    in = g_memory_input_stream_new_from_data("spliced text", -1, NULL);
    out = g_memory_output_stream_new_resizable();
    n = g_output_stream_splice(out, in, G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE, NULL, &error);
    printf("splice: %ld source closed=%d target closed=%d\n", (long)n, g_input_stream_is_closed(in), g_output_stream_is_closed(out));
    n = g_output_stream_splice(out, in, 0, NULL, &error);
    show_error("splice from a closed source", error);
    g_clear_error(&error);
    g_object_unref(in);
    in = g_memory_input_stream_new_from_data(" and more", -1, NULL);
    g_output_stream_splice_async(out, in, G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET, G_PRIORITY_DEFAULT, NULL, splice_done, NULL);
    g_main_loop_run(loop);
    printf("data: \"%.*s\"\n", (int)g_memory_output_stream_get_data_size(G_MEMORY_OUTPUT_STREAM(out)), (char *)g_memory_output_stream_get_data(G_MEMORY_OUTPUT_STREAM(out)));
    g_object_unref(in);
    g_object_unref(out);

    /* A filter stream: GDataInputStream is not here, so a bare subclass. */
    static GType filter_type = 0;
    if (!filter_type)
        filter_type = g_type_register_static_simple(G_TYPE_FILTER_INPUT_STREAM, "TestFilter", sizeof(GFilterInputStreamClass), NULL, sizeof(GFilterInputStream), NULL, 0);
    in = g_memory_input_stream_new_from_data("filtered", -1, NULL);
    GInputStream *filter = g_object_new(filter_type, "base-stream", in, "close-base-stream", FALSE, NULL);
    printf("base: %d close-base: %d\n", g_filter_input_stream_get_base_stream(G_FILTER_INPUT_STREAM(filter)) == in, g_filter_input_stream_get_close_base_stream(G_FILTER_INPUT_STREAM(filter)));
    n = g_input_stream_read(filter, buffer, 4, NULL, &error);
    printf("filter read: %ld \"%.*s\"\n", (long)n, (int)n, buffer);
    g_input_stream_close(filter, NULL, NULL);
    printf("filter closed: %d base closed: %d\n", g_input_stream_is_closed(filter), g_input_stream_is_closed(in));
    g_object_unref(filter);
    filter = g_object_new(filter_type, "base-stream", in, NULL);
    g_input_stream_close(filter, NULL, NULL);
    printf("with close-base: base closed: %d\n", g_input_stream_is_closed(in));
    g_object_unref(filter);
    g_object_unref(in);

    /* A memory stream's pollable source. */
    in = g_memory_input_stream_new_from_data("polled", -1, NULL);
    GSource *source = g_pollable_input_stream_create_source(G_POLLABLE_INPUT_STREAM(in), NULL);
    g_source_set_callback(source, (GSourceFunc)source_ready, NULL, NULL);
    g_source_attach(source, NULL);
    g_source_unref(source);
    g_main_loop_run(loop);
    g_object_unref(in);

    /* The socket streams of a connection over a socket pair. */
    int fds[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    GSocket *socket = g_socket_new_from_fd(fds[0], &error);
    GSocketConnection *connection = g_socket_connection_factory_create_connection(socket);
    GInputStream *sin = g_io_stream_get_input_stream(G_IO_STREAM(connection));
    GOutputStream *sout = g_io_stream_get_output_stream(G_IO_STREAM(connection));
    printf("socket streams: %s %s pollable=%d,%d same=%d\n", G_OBJECT_TYPE_NAME(sin), G_OBJECT_TYPE_NAME(sout), G_IS_POLLABLE_INPUT_STREAM(sin), G_IS_POLLABLE_OUTPUT_STREAM(sout), sin == g_io_stream_get_input_stream(G_IO_STREAM(connection)));
    printf("readable before: %d\n", g_pollable_input_stream_is_readable(G_POLLABLE_INPUT_STREAM(sin)));
    n = g_pollable_input_stream_read_nonblocking(G_POLLABLE_INPUT_STREAM(sin), buffer, sizeof buffer, NULL, &error);
    printf("read_nonblocking: %ld\n", (long)n);
    show_error("read_nonblocking", error);
    g_clear_error(&error);
    n = g_output_stream_write(sout, "ping", 4, NULL, &error);
    char other[16];
    printf("written: %ld, the other end reads %ld\n", (long)n, (long)read(fds[1], other, sizeof other));
    memset(buffer, 0, sizeof buffer);
    g_input_stream_read_async(sin, buffer, sizeof buffer, G_PRIORITY_DEFAULT, NULL, read_done, buffer);
    g_timeout_add(20, write_later, GINT_TO_POINTER(fds[1]));
    g_main_loop_run(loop);
    if (write(fds[1], "sync", 4) != 4)
        printf("write failed\n");
    n = g_input_stream_read(sin, buffer, sizeof buffer, NULL, &error);
    printf("read: %ld \"%.*s\"\n", (long)n, (int)n, buffer);
    ok = g_io_stream_close(G_IO_STREAM(connection), NULL, &error);
    printf("close: %d closed=%d input closed=%d output closed=%d socket closed=%d\n", ok, g_io_stream_is_closed(G_IO_STREAM(connection)), g_input_stream_is_closed(sin), g_output_stream_is_closed(sout), g_socket_is_closed(socket));
    g_object_unref(connection);
    g_object_unref(socket);
    close(fds[1]);

    /* GSimpleIOStream. */
    in = g_memory_input_stream_new_from_data("simple", -1, NULL);
    out = g_memory_output_stream_new_resizable();
    GIOStream *simple = g_simple_io_stream_new(in, out);
    printf("simple: %d %d\n", g_io_stream_get_input_stream(simple) == in, g_io_stream_get_output_stream(simple) == out);
    ok = g_io_stream_close(simple, NULL, NULL);
    printf("simple close: %d %d %d\n", ok, g_input_stream_is_closed(in), g_output_stream_is_closed(out));
    g_object_unref(simple);
    g_object_unref(in);
    g_object_unref(out);

    g_main_loop_unref(loop);
    return 0;
}
