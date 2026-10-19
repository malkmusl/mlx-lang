/* GFileInfo: its attributes of each type, masks, matchers; the information
   of files of this system (g_file_query_info, synchronous and
   asynchronous, following links or not; g_file_input_stream_query_info),
   checked against what stat says. What GLib 2.80 prints is
   fileinfo.expected. Run in an empty directory of its own. */
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>

static GMainLoop *loop;

static void attributes(void)
{
    GFileInfo *info = g_file_info_new();
    g_file_info_set_attribute_string(info, "custom::string", "text");
    g_file_info_set_attribute_byte_string(info, "custom::bytes", "a\tb\xff");
    g_file_info_set_attribute_boolean(info, "custom::bool", TRUE);
    g_file_info_set_attribute_uint32(info, "custom::u32", 4000000000u);
    g_file_info_set_attribute_int32(info, "custom::i32", -7);
    g_file_info_set_attribute_uint64(info, "custom::u64", G_GUINT64_CONSTANT(18000000000000000000));
    g_file_info_set_attribute_int64(info, "custom::i64", G_GINT64_CONSTANT(-9000000000));
    char *v[] = { "one", "two", NULL };
    g_file_info_set_attribute_stringv(info, "custom::strv", v);
    GObject *object = g_object_new(G_TYPE_OBJECT, NULL);
    g_file_info_set_attribute_object(info, "custom::object", object);
    g_object_unref(object);
    char **names = g_file_info_list_attributes(info, "custom");
    for (int i = 0; names[i]; i++) {
        char *s = g_file_info_get_attribute_as_string(info, names[i]);
        printf("%s type %d status %d: %s\n", names[i], g_file_info_get_attribute_type(info, names[i]),
               g_file_info_get_attribute_status(info, names[i]), strcmp(names[i], "custom::object") == 0 ? "(object)" : s);
        g_free(s);
    }
    g_strfreev(names);
    printf("string %s bytes %s bool %d u32 %u i32 %d u64 %" G_GUINT64_FORMAT " i64 %" G_GINT64_FORMAT " strv %s object %d\n",
           g_file_info_get_attribute_string(info, "custom::string"), g_file_info_get_attribute_byte_string(info, "custom::bytes") ? "set" : "unset",
           g_file_info_get_attribute_boolean(info, "custom::bool"), g_file_info_get_attribute_uint32(info, "custom::u32"),
           g_file_info_get_attribute_int32(info, "custom::i32"), g_file_info_get_attribute_uint64(info, "custom::u64"),
           g_file_info_get_attribute_int64(info, "custom::i64"), g_file_info_get_attribute_stringv(info, "custom::strv")[1],
           G_IS_OBJECT(g_file_info_get_attribute_object(info, "custom::object")));
    printf("has custom %d other %d; has custom::u32 %d nothing %d\n", g_file_info_has_namespace(info, "custom"),
           g_file_info_has_namespace(info, "other"), g_file_info_has_attribute(info, "custom::u32"), g_file_info_has_attribute(info, "custom::none"));
    printf("missing type %d status %d string %p\n", g_file_info_get_attribute_type(info, "custom::none"),
           g_file_info_get_attribute_status(info, "custom::none"), (void *)g_file_info_get_attribute_string(info, "custom::none"));
    g_file_info_set_attribute_status(info, "custom::bool", G_FILE_ATTRIBUTE_STATUS_ERROR_SETTING);
    printf("status set %d\n", g_file_info_get_attribute_status(info, "custom::bool"));
    g_file_info_clear_status(info);
    printf("status cleared %d\n", g_file_info_get_attribute_status(info, "custom::bool"));
    g_file_info_remove_attribute(info, "custom::i32");
    printf("removed %d\n", g_file_info_has_attribute(info, "custom::i32"));

    GFileInfo *copy = g_file_info_dup(info);
    printf("dup u64 %" G_GUINT64_FORMAT " string %s\n", g_file_info_get_attribute_uint64(copy, "custom::u64"), g_file_info_get_attribute_string(copy, "custom::string"));
    g_object_unref(copy);

    /* The common attributes' setters and getters. */
    GFileInfo *common = g_file_info_new();
    g_file_info_set_name(common, "file.txt");
    g_file_info_set_display_name(common, "File");
    g_file_info_set_edit_name(common, "file.txt");
    g_file_info_set_content_type(common, "text/plain");
    g_file_info_set_size(common, 123456789012);
    g_file_info_set_file_type(common, G_FILE_TYPE_REGULAR);
    g_file_info_set_is_hidden(common, TRUE);
    g_file_info_set_is_symlink(common, FALSE);
    g_file_info_set_symlink_target(common, "/target");
    g_file_info_set_sort_order(common, -3);
    GDateTime *when = g_date_time_new_from_unix_utc(1700000000);
    GDateTime *precise = g_date_time_add(when, 123456);
    g_file_info_set_modification_date_time(common, precise);
    g_file_info_set_access_date_time(common, when);
    GDateTime *back = g_file_info_get_modification_date_time(common);
    GDateTime *access = g_file_info_get_access_date_time(common);
    printf("name %s display %s edit %s type %s size %" G_GOFFSET_FORMAT " file type %d hidden %d symlink %d target %s order %d\n",
           g_file_info_get_name(common), g_file_info_get_display_name(common), g_file_info_get_edit_name(common),
           g_file_info_get_content_type(common), g_file_info_get_size(common), g_file_info_get_file_type(common),
           g_file_info_get_is_hidden(common), g_file_info_get_is_symlink(common), g_file_info_get_symlink_target(common),
           g_file_info_get_sort_order(common));
    printf("modified %" G_GINT64_FORMAT ".%06d usec attr %u access %" G_GINT64_FORMAT ".%06d created %p\n", g_date_time_to_unix(back),
           g_date_time_get_microsecond(back), g_file_info_get_attribute_uint32(common, G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC),
           g_date_time_to_unix(access), g_date_time_get_microsecond(access), (void *)g_file_info_get_creation_date_time(common));
    g_date_time_unref(back);
    g_date_time_unref(access);
    g_date_time_unref(precise);
    g_date_time_unref(when);

    /* A mask: what it leaves out is not set, nor kept. */
    GFileAttributeMatcher *mask = g_file_attribute_matcher_new("standard::name,custom::*");
    g_file_info_set_attribute_mask(common, mask);
    printf("masked: name %d size %d\n", g_file_info_has_attribute(common, "standard::name"), g_file_info_has_attribute(common, "standard::size"));
    g_file_info_set_size(common, 5);
    g_file_info_set_attribute_string(common, "custom::kept", "yes");
    printf("after: size %d custom %d\n", g_file_info_has_attribute(common, "standard::size"), g_file_info_has_attribute(common, "custom::kept"));
    g_file_info_unset_attribute_mask(common);
    g_file_info_set_size(common, 6);
    printf("unmasked: size %" G_GOFFSET_FORMAT "\n", g_file_info_get_size(common));
    g_file_attribute_matcher_unref(mask);
    g_object_unref(common);
    g_object_unref(info);

    GFileAttributeMatcher *matcher = g_file_attribute_matcher_new("standard::name,standard::size,time::*,owner::user");
    printf("matches name %d type %d modified %d user %d group %d\n", g_file_attribute_matcher_matches(matcher, "standard::name"),
           g_file_attribute_matcher_matches(matcher, "standard::type"), g_file_attribute_matcher_matches(matcher, "time::modified"),
           g_file_attribute_matcher_matches(matcher, "owner::user"), g_file_attribute_matcher_matches(matcher, "owner::group"));
    printf("only name %d modified %d\n", g_file_attribute_matcher_matches_only(matcher, "standard::name"),
           g_file_attribute_matcher_matches_only(matcher, "time::modified"));
    printf("enumerate standard %d:", g_file_attribute_matcher_enumerate_namespace(matcher, "standard"));
    const char *name;
    while ((name = g_file_attribute_matcher_enumerate_next(matcher))) printf(" %s", name);
    printf("\nenumerate time %d\n", g_file_attribute_matcher_enumerate_namespace(matcher, "time"));
    g_file_attribute_matcher_unref(matcher);
    matcher = g_file_attribute_matcher_new("*");
    printf("all %d %d\n", g_file_attribute_matcher_matches(matcher, "anything::here"), g_file_attribute_matcher_matches_only(matcher, "anything::here"));
    g_file_attribute_matcher_unref(matcher);
}

static const char *ATTRIBUTES = "standard::type,standard::name,standard::display-name,standard::edit-name,standard::size,"
                                "standard::is-hidden,standard::is-backup,standard::is-symlink,standard::symlink-target,"
                                "standard::content-type,standard::fast-content-type,standard::allocated-size,unix::*,time::*,"
                                "etag::value,id::file,access::can-read,access::can-write,access::can-execute,owner::user";

static void show(const char *path, GFileQueryInfoFlags flags)
{
    GFile *file = g_file_new_for_path(path);
    GError *error = NULL;
    GFileInfo *info = g_file_query_info(file, ATTRIBUTES, flags, NULL, &error);
    printf("%s%s: ", path, flags ? " (no follow)" : "");
    if (!info) {
        printf("error %d: %s\n", error->code, strstr(error->message, path) ? "(names the file)" : error->message);
        g_error_free(error);
        g_object_unref(file);
        return;
    }
    printf("type %d name %s display %s edit %s size %" G_GOFFSET_FORMAT " hidden %d backup %d symlink %d",
           g_file_info_get_file_type(info), g_file_info_get_name(info), g_file_info_get_display_name(info),
           g_file_info_get_edit_name(info), g_file_info_get_size(info), g_file_info_get_is_hidden(info),
           g_file_info_get_is_backup(info), g_file_info_get_is_symlink(info));
    if (g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET))
        printf(" target %s", g_file_info_get_symlink_target(info));
    printf(" content %s fast %s", g_file_info_get_content_type(info),
           g_file_info_get_attribute_string(info, G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE));
    printf(" read %d write %d execute %d\n", g_file_info_get_attribute_boolean(info, G_FILE_ATTRIBUTE_ACCESS_CAN_READ),
           g_file_info_get_attribute_boolean(info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE),
           g_file_info_get_attribute_boolean(info, G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE));
    struct stat st;
    int ok = (flags ? lstat(path, &st) : stat(path, &st)) == 0;
    if (ok) {
        char etag[64], id[64];
        snprintf(etag, sizeof etag, "%lu:%lu:%lu", (unsigned long)st.st_mtim.tv_sec, (unsigned long)st.st_mtim.tv_nsec / 1000, (unsigned long)st.st_mtim.tv_nsec);
        snprintf(id, sizeof id, "l%lu:%lu", (unsigned long)st.st_dev, (unsigned long)st.st_ino);
        GDateTime *modified = g_file_info_get_modification_date_time(info);
        struct passwd *pw = getpwuid(st.st_uid);
        printf("  mode %o uid %d nlink %d inode %d blocks %d allocated %d etag %d id %d modified %d usec %d nsec %d changed %d access %d user %d\n",
               g_file_info_get_attribute_uint32(info, G_FILE_ATTRIBUTE_UNIX_MODE),
               g_file_info_get_attribute_uint32(info, G_FILE_ATTRIBUTE_UNIX_UID) == st.st_uid,
               g_file_info_get_attribute_uint32(info, G_FILE_ATTRIBUTE_UNIX_NLINK),
               g_file_info_get_attribute_uint64(info, G_FILE_ATTRIBUTE_UNIX_INODE) == st.st_ino,
               g_file_info_get_attribute_uint64(info, G_FILE_ATTRIBUTE_UNIX_BLOCKS) == (guint64)st.st_blocks,
               g_file_info_get_attribute_uint64(info, G_FILE_ATTRIBUTE_STANDARD_ALLOCATED_SIZE) == (guint64)st.st_blocks * 512,
               strcmp(g_file_info_get_etag(info), etag) == 0,
               strcmp(g_file_info_get_attribute_string(info, G_FILE_ATTRIBUTE_ID_FILE), id) == 0,
               modified && g_date_time_to_unix(modified) == st.st_mtim.tv_sec && g_date_time_get_microsecond(modified) == st.st_mtim.tv_nsec / 1000,
               g_file_info_get_attribute_uint32(info, G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC) == st.st_mtim.tv_nsec / 1000,
               g_file_info_get_attribute_uint32(info, G_FILE_ATTRIBUTE_TIME_MODIFIED_NSEC) == st.st_mtim.tv_nsec,
               g_file_info_get_attribute_uint64(info, G_FILE_ATTRIBUTE_TIME_CHANGED) == (guint64)st.st_ctim.tv_sec,
               g_file_info_get_attribute_uint64(info, G_FILE_ATTRIBUTE_TIME_ACCESS) == (guint64)st.st_atim.tv_sec,
               pw && strcmp(g_file_info_get_attribute_string(info, G_FILE_ATTRIBUTE_OWNER_USER), pw->pw_name) == 0);
        if (modified) g_date_time_unref(modified);
    }
    g_object_unref(info);
    g_object_unref(file);
}

static void queried(GObject *source, GAsyncResult *result, gpointer data)
{
    GError *error = NULL;
    GFileInfo *info = g_file_query_info_finish(G_FILE(source), result, &error);
    if (info) {
        printf("async: name %s size %" G_GOFFSET_FORMAT "\n", g_file_info_get_name(info), g_file_info_get_size(info));
        g_object_unref(info);
    } else {
        printf("async error %d\n", error->code);
        g_error_free(error);
    }
    g_main_loop_quit(loop);
}

static void stream_queried(GObject *source, GAsyncResult *result, gpointer data)
{
    GError *error = NULL;
    GFileInfo *info = g_file_input_stream_query_info_finish(G_FILE_INPUT_STREAM(source), result, &error);
    printf("stream async: size %" G_GOFFSET_FORMAT " type %d\n", g_file_info_get_size(info), g_file_info_get_file_type(info));
    g_object_unref(info);
    g_main_loop_quit(loop);
}

static void files(void)
{
    g_file_set_contents("a.txt", "hello\n", -1, NULL);
    g_file_set_contents(".hidden-file", "x", -1, NULL);
    g_file_set_contents("backup.txt~", "old", -1, NULL);
    g_file_set_contents("empty", "", 0, NULL);
    g_file_set_contents("data.bin", "\x01\x02\x03\x00\xff\xfe", 6, NULL);
    g_file_set_contents("page.html", "<html><body>hi</body></html>", -1, NULL);
    g_file_set_contents("listed", "y", -1, NULL);
    g_file_set_contents(".hidden", "listed\n", -1, NULL);
    g_mkdir("sub", 0755);
    symlink("a.txt", "link");
    symlink("nowhere", "dangling");
    chmod("a.txt", 0640);
    chmod("data.bin", 0755);
    const char *paths[] = { "a.txt", ".hidden-file", "backup.txt~", "empty", "data.bin", "page.html", "listed", "sub", "link", "dangling", "missing", NULL };
    for (int i = 0; paths[i]; i++) show(paths[i], G_FILE_QUERY_INFO_NONE);
    show("link", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS);
    show("dangling", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS);

    /* Only what is asked for is there. */
    GFile *file = g_file_new_for_path("a.txt");
    GFileInfo *info = g_file_query_info(file, "standard::size", 0, NULL, NULL);
    printf("only size: size %d name %d type %d\n", g_file_info_has_attribute(info, "standard::size"),
           g_file_info_has_attribute(info, "standard::name"), g_file_info_has_attribute(info, "standard::type"));
    g_object_unref(info);
    info = g_file_query_info(file, "time::modified,unix::mode", 0, NULL, NULL);
    printf("time and mode: modified %d usec %d mode %d size %d\n", g_file_info_has_attribute(info, "time::modified"),
           g_file_info_has_attribute(info, "time::modified-usec"), g_file_info_has_attribute(info, "unix::mode"),
           g_file_info_has_attribute(info, "standard::size"));
    g_object_unref(info);

    loop = g_main_loop_new(NULL, FALSE);
    g_file_query_info_async(file, "standard::name,standard::size", 0, G_PRIORITY_DEFAULT, NULL, queried, NULL);
    g_main_loop_run(loop);
    GFile *missing = g_file_new_for_path("missing");
    g_file_query_info_async(missing, "standard::name", 0, G_PRIORITY_DEFAULT, NULL, queried, NULL);
    g_main_loop_run(loop);
    g_object_unref(missing);

    GFileInputStream *stream = g_file_read(file, NULL, NULL);
    GError *error = NULL;
    info = g_file_input_stream_query_info(stream, "standard::size,standard::type,unix::mode,standard::name", NULL, &error);
    printf("stream: size %" G_GOFFSET_FORMAT " type %d mode %o name %d\n", g_file_info_get_size(info), g_file_info_get_file_type(info),
           g_file_info_get_attribute_uint32(info, G_FILE_ATTRIBUTE_UNIX_MODE), g_file_info_has_attribute(info, "standard::name"));
    g_object_unref(info);
    g_file_input_stream_query_info_async(stream, "standard::size,standard::type", G_PRIORITY_DEFAULT, NULL, stream_queried, NULL);
    g_main_loop_run(loop);
    g_input_stream_close(G_INPUT_STREAM(stream), NULL, NULL);
    g_object_unref(stream);
    printf("file type %d %d %d\n", g_file_query_file_type(file, 0, NULL), g_file_query_file_type(g_file_new_for_path("sub"), 0, NULL),
           g_file_query_file_type(g_file_new_for_path("link"), G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL));
    g_object_unref(file);
    g_main_loop_unref(loop);
}

int main(void)
{
    printf("== attributes\n");
    attributes();
    printf("== files\n");
    files();
    return 0;
}
