/* GDBus against a private bus (DBUS_SESSION_BUS_ADDRESS, a dbus-daemon
   of the test's own): connections, names owned, objects exported with a
   vtable and called (methods, errors, properties, Introspect, Peer),
   signals emitted and subscribed to (match rules, arg0 namespaces,
   NO_MATCH_RULE), proxies (sync and async, cached properties, g-signal,
   g-properties-changed, names without an owner), descriptors passed both
   ways, timeouts, cancellation, closing; names' syntax and the mapping of
   errors. The exported objects run in a thread of their own (their
   context), so that the main thread's synchronous calls get answers. */
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static GDBusConnection *client;
static GDBusConnection *service;
static const char *client_name;
static const char *service_name;
static GMainContext *service_context;
static GMainLoop *service_loop;
static GDBusMethodInvocation *held;
static guint count = 5;
static char *label;
static int events;

static const char introspection[] =
    "<node>"
    "  <interface name='org.mlx.Test'>"
    "    <annotation name='org.mlx.Note' value='a &amp; b'/>"
    "    <method name='Echo'><arg name='text' type='s' direction='in'/><arg name='echo' type='s' direction='out'/></method>"
    "    <method name='Add'><arg type='i' direction='in'/><arg type='i' direction='in'/><arg name='sum' type='i' direction='out'/></method>"
    "    <method name='Fail'/>"
    "    <method name='Unsupported'/>"
    "    <method name='Custom'/>"
    "    <method name='Hold'/>"
    "    <method name='Emit'/>"
    "    <method name='Pipe'><arg name='in' type='h' direction='in'/><arg name='out' type='h' direction='out'/></method>"
    "    <method name='Types'><arg name='value' type='(ybnqiuxtdsogav)' direction='in'/><arg name='value' type='(ybnqiuxtdsogav)' direction='out'/></method>"
    "    <method name='Dict'><arg name='d' type='a{sv}' direction='in'/><arg name='keys' type='as' direction='out'/></method>"
    "    <signal name='Changed'><arg name='what' type='s'/></signal>"
    "    <property name='Count' type='u' access='read'/>"
    "    <property name='Label' type='s' access='readwrite'/>"
    "    <property name='Broken' type='s' access='read'/>"
    "    <property name='Secret' type='s' access='write'/>"
    "  </interface>"
    "</node>";

static void print_error(const char *what, GError *error)
{
    if (!error) {
        printf("%s: no error\n", what);
        return;
    }
    printf("%s: %s %d: %s\n", what, g_quark_to_string(error->domain), error->code, error->message);
}

static void print_value(const char *what, GVariant *value)
{
    if (!value) {
        printf("%s: (null)\n", what);
        return;
    }
    char *text = g_variant_print(value, TRUE);
    printf("%s: %s %s\n", what, g_variant_get_type_string(value), text);
    g_free(text);
}

static const char *who(const char *sender)
{
    if (!sender)
        return "(none)";
    if (!g_strcmp0(sender, client_name))
        return "client";
    if (!g_strcmp0(sender, service_name))
        return "service";
    if (!g_strcmp0(sender, "org.freedesktop.DBus"))
        return "bus";
    return "other";
}

static gboolean watchdog(gpointer data)
{
    printf("timed out waiting for %s\n", (const char *)data);
    exit(3);
}

static void wait_events(int wanted, const char *what)
{
    GSource *source = g_timeout_source_new(10000);
    g_source_set_callback(source, watchdog, (gpointer)what, NULL);
    g_source_attach(source, NULL);
    while (events < wanted)
        g_main_context_iteration(NULL, TRUE);
    g_source_destroy(source);
    g_source_unref(source);
    events = 0;
}

/* Runs the main context until nothing is pending. */
static void drain(void)
{
    for (int i = 0; i < 50; i++) {
        while (g_main_context_iteration(NULL, FALSE)) { }
        g_usleep(2000);
    }
}

/* ---- The service (its thread's context) ---- */

static void method_call(GDBusConnection *connection, const gchar *sender, const gchar *path, const gchar *iface, const gchar *method, GVariant *parameters, GDBusMethodInvocation *invocation, gpointer data)
{
    char *args = g_variant_print(parameters, TRUE);
    printf("  service: %s.%s%s on %s from %s (%s)\n", iface, method, args, path, who(sender), (const char *)data);
    g_free(args);
    if (!strcmp(method, "Echo")) {
        const char *text;
        g_variant_get(parameters, "(&s)", &text);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", text));
    } else if (!strcmp(method, "Add")) {
        gint a, b;
        g_variant_get(parameters, "(ii)", &a, &b);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(i)", a + b));
    } else if (!strcmp(method, "Fail")) {
        g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED, "%s.%s.%s is not available now", path, iface, method);
    } else if (!strcmp(method, "Unsupported")) {
        g_dbus_method_invocation_return_error_literal(invocation, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "");
    } else if (!strcmp(method, "Custom")) {
        g_dbus_method_invocation_return_dbus_error(invocation, "org.mlx.Error.Custom", "a custom error");
    } else if (!strcmp(method, "Hold")) {
        held = invocation;
    } else if (!strcmp(method, "Emit")) {
        GError *error = NULL;
        gboolean ok = g_dbus_connection_emit_signal(connection, NULL, path, "org.mlx.Test", "Changed", g_variant_new("(s)", "hello"), &error);
        GVariantBuilder changed;
        g_variant_builder_init(&changed, G_VARIANT_TYPE("a{sv}"));
        g_variant_builder_add(&changed, "{sv}", "Count", g_variant_new_uint32(7));
        const char *invalidated[] = { "Label", NULL };
        ok = ok && g_dbus_connection_emit_signal(connection, NULL, path, "org.freedesktop.DBus.Properties", "PropertiesChanged",
            g_variant_new("(sa{sv}^as)", "org.mlx.Test", &changed, invalidated), &error);
        ok = ok && g_dbus_connection_emit_signal(connection, client_name, "/org/mlx/Direct", "org.mlx.Direct", "Poke", g_variant_new("(u)", 42), &error);
        printf("  service: emitted %d\n", ok);
        g_dbus_method_invocation_return_value(invocation, NULL);
    } else if (!strcmp(method, "Pipe")) {
        GDBusMessage *message = g_dbus_method_invocation_get_message(invocation);
        GUnixFDList *in = g_dbus_message_get_unix_fd_list(message);
        gint32 handle;
        g_variant_get(parameters, "(h)", &handle);
        GError *error = NULL;
        int fd = g_unix_fd_list_get(in, handle, &error);
        printf("  service: %d descriptors, got %d\n", g_unix_fd_list_get_length(in), fd >= 0);
        if (write(fd, "ping", 4) != 4)
            printf("  service: write failed\n");
        close(fd);
        int fds[2];
        if (pipe(fds) != 0)
            printf("  service: pipe failed\n");
        if (write(fds[1], "pong", 4) != 4)
            printf("  service: write failed\n");
        close(fds[1]);
        GUnixFDList *out = g_unix_fd_list_new();
        int index = g_unix_fd_list_append(out, fds[0], &error);
        close(fds[0]);
        g_dbus_method_invocation_return_value_with_unix_fd_list(invocation, g_variant_new("(h)", index), out);
        g_object_unref(out);
    } else if (!strcmp(method, "Types")) {
        g_dbus_method_invocation_return_value(invocation, g_variant_ref(parameters));
    } else if (!strcmp(method, "Dict")) {
        GVariant *dict;
        g_variant_get(parameters, "(@a{sv})", &dict);
        GVariantBuilder keys;
        g_variant_builder_init(&keys, G_VARIANT_TYPE("as"));
        GVariantIter iter;
        const char *key;
        GVariant *value;
        g_variant_iter_init(&iter, dict);
        while (g_variant_iter_next(&iter, "{&sv}", &key, &value)) {
            g_variant_builder_add(&keys, "s", key);
            g_variant_unref(value);
        }
        g_variant_unref(dict);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(as)", &keys));
    } else if (!strcmp(method, "Get") || !strcmp(method, "Set") || !strcmp(method, "GetAll")) {
        /* The property calls of an interface without getters. */
        printf("  service: property call through method_call, property info %s\n",
            g_dbus_method_invocation_get_property_info(invocation) ? g_dbus_method_invocation_get_property_info(invocation)->name : "(none)");
        if (!strcmp(method, "Get"))
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(v)", g_variant_new_string("from method_call")));
        else if (!strcmp(method, "Set"))
            g_dbus_method_invocation_return_value(invocation, NULL);
        else {
            GVariantBuilder all;
            g_variant_builder_init(&all, G_VARIANT_TYPE("a{sv}"));
            g_variant_builder_add(&all, "{sv}", "Label", g_variant_new_string("all"));
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(a{sv})", &all));
        }
    } else {
        g_dbus_method_invocation_return_dbus_error(invocation, "org.mlx.Error.Unexpected", method);
    }
}

static GVariant *get_property(GDBusConnection *connection, const gchar *sender, const gchar *path, const gchar *iface, const gchar *name, GError **error, gpointer data)
{
    printf("  service: get %s on %s from %s\n", name, path, who(sender));
    if (!strcmp(name, "Count"))
        return g_variant_new_uint32(count);
    if (!strcmp(name, "Label"))
        return g_variant_new_string(label);
    g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "%s.%s %s is not supported", path, iface, name);
    return NULL;
}

static gboolean set_property(GDBusConnection *connection, const gchar *sender, const gchar *path, const gchar *iface, const gchar *name, GVariant *value, GError **error, gpointer data)
{
    char *text = g_variant_print(value, TRUE);
    printf("  service: set %s to %s from %s\n", name, text, who(sender));
    g_free(text);
    if (!strcmp(name, "Label")) {
        g_free(label);
        label = g_variant_dup_string(value, NULL);
        return TRUE;
    }
    g_set_error(error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED, "Unknown property '%s'", name);
    return FALSE;
}

static const GDBusInterfaceVTable vtable = { method_call, get_property, set_property, { NULL } };
static const GDBusInterfaceVTable method_only = { method_call, NULL, NULL, { NULL } };

static void registration_gone(gpointer data)
{
    printf("registration data %s freed\n", (const char *)data);
}

static gpointer service_thread(gpointer data)
{
    g_main_context_push_thread_default(service_context);
    g_main_loop_run(service_loop);
    g_main_context_pop_thread_default(service_context);
    return NULL;
}

static gboolean release_held(gpointer data)
{
    printf("  service: replying to the held call\n");
    g_dbus_method_invocation_return_value(held, NULL);
    held = NULL;
    return G_SOURCE_REMOVE;
}

/* ---- Names and errors ---- */

static void names(void)
{
    const char *strings[] = { "org.mlx.Test", ":1.42", "org", "org.", ".org.mlx", "org..mlx", "org.mlx-x.y", "org.1mlx", ":1.2x-y", "a_b.c", "Echo", "1Echo", "Ec-ho", "", NULL };
    for (int i = 0; strings[i]; i++)
        printf("name '%s': name %d unique %d member %d interface %d error %d\n", strings[i], g_dbus_is_name(strings[i]), g_dbus_is_unique_name(strings[i]),
            g_dbus_is_member_name(strings[i]), g_dbus_is_interface_name(strings[i]), g_dbus_is_error_name(strings[i]));
    printf("guid %d %d\n", g_dbus_is_guid("0123456789abcdef0123456789abcdef"), g_dbus_is_guid("0123456789abcdef"));
    char *guid = g_dbus_generate_guid();
    printf("generated guid %d\n", g_dbus_is_guid(guid));
    g_free(guid);

    GError *e = g_error_new_literal(G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "x");
    char *name = g_dbus_error_encode_gerror(e);
    printf("encoded io: %s\n", name);
    g_free(name);
    g_error_free(e);
    e = g_error_new_literal(G_DBUS_ERROR, G_DBUS_ERROR_ACCESS_DENIED, "x");
    name = g_dbus_error_encode_gerror(e);
    printf("encoded dbus: %s\n", name);
    g_free(name);
    g_error_free(e);
    e = g_error_new_literal(g_quark_from_static_string("my-odd domain!"), 7, "x");
    name = g_dbus_error_encode_gerror(e);
    printf("encoded odd: %s\n", name);
    GError *back = g_dbus_error_new_for_dbus_error(name, "it broke");
    print_error("decoded odd", back);
    g_error_free(back);
    g_free(name);
    g_error_free(e);
    e = g_dbus_error_new_for_dbus_error("org.freedesktop.DBus.Error.ServiceUnknown", "nope");
    print_error("registered", e);
    char *remote = g_dbus_error_get_remote_error(e);
    printf("remote %d %s\n", g_dbus_error_is_remote_error(e), remote);
    g_free(remote);
    g_dbus_error_strip_remote_error(e);
    print_error("stripped", e);
    g_error_free(e);
    e = g_dbus_error_new_for_dbus_error("org.example.Unknown", "who knows");
    print_error("unknown", e);
    g_error_free(e);
    static const GDBusErrorEntry entries[] = { { 0, "org.mlx.Error.First" }, { 1, "org.mlx.Error.Second" } };
    static gsize quark = 0;
    g_dbus_error_register_error_domain("mlx-test-error-quark", &quark, entries, G_N_ELEMENTS(entries));
    e = g_dbus_error_new_for_dbus_error("org.mlx.Error.Second", "second");
    print_error("domain registered", e);
    g_error_free(e);
    printf("register again %d, unregister %d %d\n", g_dbus_error_register_error((GQuark)quark, 1, "org.mlx.Error.Second"),
        g_dbus_error_unregister_error((GQuark)quark, 1, "org.mlx.Error.Second"), g_dbus_error_unregister_error((GQuark)quark, 1, "org.mlx.Error.Second"));
    GError *set = NULL;
    g_dbus_error_set_dbus_error(&set, "org.mlx.Error.First", "first", "while %s", "testing");
    print_error("set", set);
    g_error_free(set);

    GDBusNodeInfo *node = g_dbus_node_info_new_for_xml(introspection, NULL);
    GString *xml = g_string_new(NULL);
    g_dbus_node_info_generate_xml(node, 1, xml);
    printf("generated:\n%s", xml->str);
    g_string_free(xml, TRUE);
    g_dbus_node_info_unref(node);
}

/* ---- Connecting ---- */

static void connected(GObject *source, GAsyncResult *result, gpointer data)
{
    GError *error = NULL;
    GDBusConnection **slot = data;
    *slot = g_dbus_connection_new_for_address_finish(result, &error);
    printf("async connection %d: %s, closed %d\n", *slot != NULL, error ? error->message : "ok", *slot ? g_dbus_connection_is_closed(*slot) : -1);
    g_clear_error(&error);
    events++;
}

static void name_acquired(GDBusConnection *connection, const gchar *name, gpointer data)
{
    printf("name %s acquired by %s\n", name, (const char *)data);
    events++;
}

static void name_lost(GDBusConnection *connection, const gchar *name, gpointer data)
{
    printf("name %s lost by %s (connection %d)\n", name, (const char *)data, connection != NULL);
    events++;
}

static void owner_gone(gpointer data)
{
    printf("owner data %s freed\n", (const char *)data);
}

static void closed(GDBusConnection *connection, gboolean vanished, GError *error, gpointer data)
{
    printf("closed %s: vanished %d error %s\n", (const char *)data, vanished, error ? error->message : "(none)");
    events++;
}

/* ---- Signals ---- */

static void on_signal(GDBusConnection *connection, const gchar *sender, const gchar *path, const gchar *iface, const gchar *member, GVariant *parameters, gpointer data)
{
    char *text = g_variant_print(parameters, TRUE);
    printf("signal %s: %s %s.%s%s from %s\n", (const char *)data, path, iface, member, text, who(sender));
    g_free(text);
    events++;
}

/* (Logged, and printed once the waiting is over: the owner's own
   callbacks come from another connection, in no set order with these.) */
static GString *owner_log;

static void on_owner_changed(GDBusConnection *connection, const gchar *sender, const gchar *path, const gchar *iface, const gchar *member, GVariant *parameters, gpointer data)
{
    const char *name, *old_owner, *new_owner;
    g_variant_get(parameters, "(&s&s&s)", &name, &old_owner, &new_owner);
    g_string_append_printf(owner_log, "owner of %s changed (%s): old %s new %s from %s\n", name, (const char *)data, *old_owner ? "set" : "empty", *new_owner ? "set" : "empty", who(sender));
    events++;
}

static void print_owner_log(void)
{
    printf("%s", owner_log->str);
    g_string_truncate(owner_log, 0);
}

static void subscription_gone(gpointer data)
{
    printf("subscription data %s freed\n", (const char *)data);
}

/* ---- Proxies ---- */

static void proxy_signal(GDBusProxy *proxy, const gchar *sender, const gchar *signal, GVariant *parameters, gpointer data)
{
    char *text = g_variant_print(parameters, TRUE);
    printf("g-signal %s: %s%s from %s\n", (const char *)data, signal, text, who(sender));
    g_free(text);
    events++;
}

static void proxy_properties(GDBusProxy *proxy, GVariant *changed, const gchar *const *invalidated, gpointer data)
{
    char *text = g_variant_print(changed, TRUE);
    printf("g-properties-changed %s: %s invalidated", (const char *)data, text);
    for (int i = 0; invalidated[i]; i++)
        printf(" %s", invalidated[i]);
    printf("\n");
    g_free(text);
    events++;
}

static void print_cache(const char *what, GDBusProxy *proxy)
{
    char **names = g_dbus_proxy_get_cached_property_names(proxy);
    printf("%s cached:", what);
    for (int i = 0; names && names[i]; i++) {
        GVariant *value = g_dbus_proxy_get_cached_property(proxy, names[i]);
        char *text = g_variant_print(value, TRUE);
        printf(" %s=%s", names[i], text);
        g_free(text);
        g_variant_unref(value);
    }
    printf("%s\n", names ? "" : " (none)");
    g_strfreev(names);
}

static void proxy_ready(GObject *source, GAsyncResult *result, gpointer data)
{
    GError *error = NULL;
    GDBusProxy **slot = data;
    *slot = g_dbus_proxy_new_finish(result, &error);
    printf("async proxy %d: %s\n", *slot != NULL, error ? error->message : "ok");
    g_clear_error(&error);
    events++;
}

static void bus_proxy_ready(GObject *source, GAsyncResult *result, gpointer data)
{
    GError *error = NULL;
    GDBusProxy **slot = data;
    *slot = g_dbus_proxy_new_for_bus_finish(result, &error);
    printf("async bus proxy %d: %s\n", *slot != NULL, error ? error->message : "ok");
    g_clear_error(&error);
    events++;
}

static void proxy_called(GObject *source, GAsyncResult *result, gpointer data)
{
    GError *error = NULL;
    GVariant *value = g_dbus_proxy_call_finish(G_DBUS_PROXY(source), result, &error);
    print_value((const char *)data, value);
    if (error)
        print_error((const char *)data, error);
    if (value)
        g_variant_unref(value);
    g_clear_error(&error);
    events++;
}

static void connection_called(GObject *source, GAsyncResult *result, gpointer data)
{
    GError *error = NULL;
    GVariant *value = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), result, &error);
    print_value((const char *)data, value);
    if (error)
        print_error((const char *)data, error);
    if (value)
        g_variant_unref(value);
    g_clear_error(&error);
    events++;
}

static GVariant *call(const char *what, const char *path, const char *iface, const char *method, GVariant *parameters, const char *reply)
{
    GError *error = NULL;
    GVariant *value = g_dbus_connection_call_sync(client, "org.mlx.Test", path, iface, method, parameters,
        reply ? G_VARIANT_TYPE(reply) : NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
    if (value)
        print_value(what, value);
    else
        print_error(what, error);
    g_clear_error(&error);
    return value;
}

static void calls(void)
{
    GVariant *v;
    v = call("echo", "/org/mlx/Test", "org.mlx.Test", "Echo", g_variant_new("(s)", "héllo"), "(s)");
    g_variant_unref(v);
    v = call("add", "/org/mlx/Test", "org.mlx.Test", "Add", g_variant_new("(ii)", 40, 2), NULL);
    g_variant_unref(v);
    call("wrong reply type", "/org/mlx/Test", "org.mlx.Test", "Add", g_variant_new("(ii)", 1, 2), "(s)");
    call("fail", "/org/mlx/Test", "org.mlx.Test", "Fail", NULL, NULL);
    call("unsupported", "/org/mlx/Test", "org.mlx.Test", "Unsupported", NULL, NULL);
    call("custom", "/org/mlx/Test", "org.mlx.Test", "Custom", NULL, NULL);
    call("unknown method", "/org/mlx/Test", "org.mlx.Test", "Nope", NULL, NULL);
    call("wrong arguments", "/org/mlx/Test", "org.mlx.Test", "Add", g_variant_new("(s)", "x"), NULL);
    call("unknown interface", "/org/mlx/Test", "org.mlx.Other", "Echo", g_variant_new("(s)", "x"), NULL);
    call("unknown object", "/org/mlx/Nowhere", "org.mlx.Test", "Echo", g_variant_new("(s)", "x"), NULL);
    v = call("ping", "/org/mlx/Nowhere", "org.freedesktop.DBus.Peer", "Ping", NULL, NULL);
    g_variant_unref(v);
    /* (The machine's id itself differs from machine to machine.) */
    v = g_dbus_connection_call_sync(client, "org.mlx.Test", "/", "org.freedesktop.DBus.Peer", "GetMachineId", NULL, G_VARIANT_TYPE("(s)"), 0, -1, NULL, NULL);
    if (v) {
        const char *id;
        g_variant_get(v, "(&s)", &id);
        printf("machine id length %zu\n", strlen(id));
        g_variant_unref(v);
    }
    const char *introspected[] = { "/org/mlx/Test", "/org/mlx/Test/Child", "/", "/org/mlx/Test/Child/Deep", NULL };
    for (int i = 0; introspected[i]; i++) {
        GError *error = NULL;
        v = g_dbus_connection_call_sync(client, "org.mlx.Test", introspected[i], "org.freedesktop.DBus.Introspectable", "Introspect", NULL, G_VARIANT_TYPE("(s)"), 0, -1, NULL, &error);
        if (v) {
            const char *xml;
            g_variant_get(v, "(&s)", &xml);
            printf("introspect %s:\n%s", introspected[i], xml);
            g_variant_unref(v);
        } else {
            print_error("introspect", error);
            g_clear_error(&error);
        }
    }
    v = call("get Count", "/org/mlx/Test", "org.freedesktop.DBus.Properties", "Get", g_variant_new("(ss)", "org.mlx.Test", "Count"), "(v)");
    g_variant_unref(v);
    call("get Broken", "/org/mlx/Test", "org.freedesktop.DBus.Properties", "Get", g_variant_new("(ss)", "org.mlx.Test", "Broken"), NULL);
    call("get Secret", "/org/mlx/Test", "org.freedesktop.DBus.Properties", "Get", g_variant_new("(ss)", "org.mlx.Test", "Secret"), NULL);
    call("get Missing", "/org/mlx/Test", "org.freedesktop.DBus.Properties", "Get", g_variant_new("(ss)", "org.mlx.Test", "Missing"), NULL);
    call("get other interface", "/org/mlx/Test", "org.freedesktop.DBus.Properties", "Get", g_variant_new("(ss)", "org.mlx.Other", "Count"), NULL);
    v = call("set Label", "/org/mlx/Test", "org.freedesktop.DBus.Properties", "Set", g_variant_new("(ssv)", "org.mlx.Test", "Label", g_variant_new_string("new")), NULL);
    g_variant_unref(v);
    call("set Label wrong type", "/org/mlx/Test", "org.freedesktop.DBus.Properties", "Set", g_variant_new("(ssv)", "org.mlx.Test", "Label", g_variant_new_int32(1)), NULL);
    call("set Count", "/org/mlx/Test", "org.freedesktop.DBus.Properties", "Set", g_variant_new("(ssv)", "org.mlx.Test", "Count", g_variant_new_uint32(1)), NULL);
    call("set Secret", "/org/mlx/Test", "org.freedesktop.DBus.Properties", "Set", g_variant_new("(ssv)", "org.mlx.Test", "Secret", g_variant_new_string("s")), NULL);
    v = call("get all", "/org/mlx/Test", "org.freedesktop.DBus.Properties", "GetAll", g_variant_new("(s)", "org.mlx.Test"), NULL);
    g_variant_unref(v);
    v = call("get through method_call", "/org/mlx/Methods", "org.freedesktop.DBus.Properties", "Get", g_variant_new("(ss)", "org.mlx.Test", "Label"), NULL);
    g_variant_unref(v);
    v = call("set through method_call", "/org/mlx/Methods", "org.freedesktop.DBus.Properties", "Set", g_variant_new("(ssv)", "org.mlx.Test", "Label", g_variant_new_string("x")), NULL);
    g_variant_unref(v);
    v = call("get all through method_call", "/org/mlx/Methods", "org.freedesktop.DBus.Properties", "GetAll", g_variant_new("(s)", "org.mlx.Test"), NULL);
    g_variant_unref(v);
    GVariantBuilder av;
    g_variant_builder_init(&av, G_VARIANT_TYPE("av"));
    g_variant_builder_add(&av, "v", g_variant_new_int32(1));
    g_variant_builder_add(&av, "v", g_variant_new_string("two"));
    static const guchar four[] = { 4, 5 };
    g_variant_builder_add(&av, "v", g_variant_new("(i@ay)", 3, g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, four, 2, 1)));
    g_variant_builder_add(&av, "v", g_variant_new("(bb)", TRUE, FALSE));
    v = call("types", "/org/mlx/Test", "org.mlx.Test", "Types",
        g_variant_new("((ybnqiuxtdsogav))", 200, TRUE, (gint16)-3, (guint16)65535, -100000, (guint32)4000000000u, (gint64)-9000000000ll, (guint64)18000000000000000000ull,
            2.5, "ünïcode", "/a/b", "a{sv}", &av), NULL);
    g_variant_unref(v);
    GVariantBuilder dict;
    g_variant_builder_init(&dict, G_VARIANT_TYPE("a{sv}"));
    static const guchar bytes3[] = { 1, 2, 3 };
    g_variant_builder_add(&dict, "{sv}", "a", g_variant_new_int32(1));
    g_variant_builder_add(&dict, "{sv}", "b", g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, bytes3, 3, 1));
    GVariantBuilder ss;
    g_variant_builder_init(&ss, G_VARIANT_TYPE("a{ss}"));
    g_variant_builder_add(&ss, "{ss}", "x", "y");
    g_variant_builder_add(&dict, "{sv}", "c", g_variant_builder_end(&ss));
    v = call("dict", "/org/mlx/Test", "org.mlx.Test", "Dict", g_variant_new("(a{sv})", &dict), NULL);
    g_variant_unref(v);
    /* A big message: an array of 200000 bytes. */
    GVariantBuilder big;
    g_variant_builder_init(&big, G_VARIANT_TYPE("ay"));
    for (int i = 0; i < 200000; i++)
        g_variant_builder_add(&big, "y", (guchar)(i * 7));
    GVariant *bytes = g_variant_builder_end(&big);
    GError *error = NULL;
    v = g_dbus_connection_call_sync(client, "org.mlx.Test", "/org/mlx/Test", "org.mlx.Test", "Echo", g_variant_new("(s)", "big"), NULL, 0, -1, NULL, &error);
    g_variant_unref(v);
    GVariantBuilder holder;
    g_variant_builder_init(&holder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&holder, "{sv}", "big", bytes);
    v = g_dbus_connection_call_sync(client, "org.mlx.Test", "/org/mlx/Test", "org.mlx.Test", "Dict",
        g_variant_new("(a{sv})", &holder), NULL, 0, -1, NULL, &error);
    print_value("big dict", v);
    g_variant_unref(v);

    /* Asynchronous calls, one cancelled. */
    g_dbus_connection_call(client, "org.mlx.Test", "/org/mlx/Test", "org.mlx.Test", "Echo", g_variant_new("(s)", "async"), G_VARIANT_TYPE("(s)"), 0, -1, NULL, connection_called, "async echo");
    wait_events(1, "an asynchronous call");
    g_dbus_connection_call(client, "org.mlx.Test", "/org/mlx/Test", "org.mlx.Test", "Fail", NULL, NULL, 0, -1, NULL, connection_called, "async fail");
    wait_events(1, "an asynchronous call");
    GCancellable *cancellable = g_cancellable_new();
    g_dbus_connection_call(client, "org.mlx.Test", "/org/mlx/Test", "org.mlx.Test", "Hold", NULL, NULL, 0, -1, cancellable, connection_called, "cancelled call");
    g_usleep(100000);
    g_cancellable_cancel(cancellable);
    wait_events(1, "the cancelled call");
    g_object_unref(cancellable);
    g_main_context_invoke(service_context, release_held, NULL);
    g_usleep(100000);
    error = NULL;
    v = g_dbus_connection_call_sync(client, "org.mlx.Test", "/org/mlx/Test", "org.mlx.Test", "Hold", NULL, NULL, 0, 200, NULL, &error);
    print_error("timeout 200", error);
    g_clear_error(&error);
    g_main_context_invoke(service_context, release_held, NULL);
    g_usleep(100000);
    g_dbus_connection_call(client, "org.mlx.Test", "/org/mlx/Test", "org.mlx.Test", "Hold", NULL, NULL, 0, 200, NULL, connection_called, "async timeout");
    wait_events(1, "the async timeout");
    g_main_context_invoke(service_context, release_held, NULL);
    g_usleep(100000);
    /* No reply wanted. */
    g_dbus_connection_call(client, "org.mlx.Test", "/org/mlx/Test", "org.mlx.Test", "Echo", g_variant_new("(s)", "nobody waits"), NULL, 0, -1, NULL, NULL, NULL);
    v = call("after", "/org/mlx/Test", "org.mlx.Test", "Echo", g_variant_new("(s)", "after"), NULL);
    g_variant_unref(v);

    /* Descriptors both ways. */
    int fds[2];
    if (pipe(fds) != 0)
        return;
    GUnixFDList *list = g_unix_fd_list_new();
    int index = g_unix_fd_list_append(list, fds[1], &error);
    close(fds[1]);
    GUnixFDList *out = NULL;
    v = g_dbus_connection_call_with_unix_fd_list_sync(client, "org.mlx.Test", "/org/mlx/Test", "org.mlx.Test", "Pipe", g_variant_new("(h)", index), G_VARIANT_TYPE("(h)"), 0, -1, list, &out, NULL, &error);
    print_value("pipe", v);
    char buffer[8] = { 0 };
    printf("read %zd '%s'\n", read(fds[0], buffer, 4), buffer);
    close(fds[0]);
    gint32 handle;
    g_variant_get(v, "(h)", &handle);
    g_variant_unref(v);
    printf("out list %d length %d\n", out != NULL, out ? g_unix_fd_list_get_length(out) : -1);
    int got = g_unix_fd_list_get(out, handle, &error);
    memset(buffer, 0, sizeof buffer);
    printf("read back %zd '%s'\n", read(got, buffer, 4), buffer);
    close(got);
    gint length;
    const gint *peeked = g_unix_fd_list_peek_fds(out, &length);
    printf("peeked %d, ended %d\n", length, peeked[length] == -1);
    gint *stolen = g_unix_fd_list_steal_fds(out, &length);
    printf("stolen %d, left %d\n", length, g_unix_fd_list_get_length(out));
    for (int i = 0; i < length; i++)
        close(stolen[i]);
    g_free(stolen);
    g_object_unref(out);
    g_object_unref(list);
}

int main(void)
{
    alarm(120);
    owner_log = g_string_new(NULL);
    setvbuf(stdout, NULL, _IOLBF, 0);
    names();

    GError *error = NULL;
    char *address = g_dbus_address_get_for_bus_sync(G_BUS_TYPE_SESSION, NULL, &error);
    printf("address %d\n", address != NULL);
    client = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    print_error("client", error);
    client_name = g_dbus_connection_get_unique_name(client);
    GDBusConnection *again = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
    printf("client %s, same bus %d, flags %d, capabilities %d, closed %d, exit on close %d, guid %d\n", client_name, again == client,
        g_dbus_connection_get_flags(client), g_dbus_connection_get_capabilities(client), g_dbus_connection_is_closed(client),
        g_dbus_connection_get_exit_on_close(client), g_dbus_is_guid(g_dbus_connection_get_guid(client)));
    g_object_unref(again);
    g_dbus_connection_set_exit_on_close(client, FALSE);

    service = g_dbus_connection_new_for_address_sync(address, G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION, NULL, NULL, &error);
    print_error("service", error);
    service_name = g_dbus_connection_get_unique_name(service);
    printf("service %s, exit on close %d\n", service_name, g_dbus_connection_get_exit_on_close(service));

    GDBusConnection *third = NULL;
    g_dbus_connection_new_for_address(address, G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION, NULL, NULL, connected, &third);
    wait_events(1, "the async connection");
    printf("third %s\n", g_dbus_connection_get_unique_name(third));

    /* As MediaSessionGLib makes one. */
    GDBusConnection *made = G_DBUS_CONNECTION(g_object_new(G_TYPE_DBUS_CONNECTION, "address", address,
        "flags", G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION, "exit-on-close", TRUE, NULL));
    gboolean initialized = g_initable_init(G_INITABLE(made), NULL, &error);
    char *made_name = NULL;
    gboolean made_exit = FALSE;
    g_object_get(made, "unique-name", &made_name, "exit-on-close", &made_exit, NULL);
    printf("made %d %s exit on close %d\n", initialized, made_name, made_exit);
    g_free(made_name);
    g_dbus_connection_set_exit_on_close(made, FALSE);

    GDBusConnection *bad = g_dbus_connection_new_for_address_sync("unix:path=/nonexistent/bus", G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT, NULL, NULL, &error);
    print_error("bad address", error);
    g_clear_error(&error);
    printf("bad %p\n", (void *)bad);

    /* The service: objects registered for its thread's context. */
    label = g_strdup("initial");
    service_context = g_main_context_new();
    service_loop = g_main_loop_new(service_context, FALSE);
    GDBusNodeInfo *node = g_dbus_node_info_new_for_xml(introspection, &error);
    GDBusInterfaceInfo *iface = g_dbus_node_info_lookup_interface(node, "org.mlx.Test");
    g_main_context_push_thread_default(service_context);
    guint test_id = g_dbus_connection_register_object(service, "/org/mlx/Test", iface, &vtable, "test", registration_gone, &error);
    guint deep_id = g_dbus_connection_register_object(service, "/org/mlx/Test/Child/Deep", iface, &vtable, "deep", registration_gone, &error);
    guint methods_id = g_dbus_connection_register_object(service, "/org/mlx/Methods", iface, &method_only, "methods", registration_gone, &error);
    guint twice = g_dbus_connection_register_object(service, "/org/mlx/Test", iface, &vtable, "twice", registration_gone, &error);
    print_error("registered twice", error);
    g_clear_error(&error);
    g_main_context_pop_thread_default(service_context);
    printf("registered %d %d %d %d\n", test_id > 0, deep_id > 0, methods_id > 0, twice);
    GThread *thread = g_thread_new("service", service_thread, NULL);

    /* Names. */
    guint owner = g_bus_own_name_on_connection(service, "org.mlx.Test", G_BUS_NAME_OWNER_FLAGS_DO_NOT_QUEUE, name_acquired, name_lost, "service", owner_gone);
    wait_events(1, "the name");
    guint loser = g_bus_own_name_on_connection(third, "org.mlx.Test", G_BUS_NAME_OWNER_FLAGS_DO_NOT_QUEUE, name_acquired, name_lost, "third", owner_gone);
    wait_events(1, "the name lost");
    g_bus_unown_name(loser);
    drain();

    calls();

    /* Signals. */
    guint by_sender = g_dbus_connection_signal_subscribe(client, service_name, "org.mlx.Test", "Changed", "/org/mlx/Test", NULL, G_DBUS_SIGNAL_FLAGS_NONE, on_signal, "by sender", subscription_gone);
    guint by_name = g_dbus_connection_signal_subscribe(client, "org.mlx.Test", "org.mlx.Test", NULL, NULL, NULL, G_DBUS_SIGNAL_FLAGS_NONE, on_signal, "by name", subscription_gone);
    guint arg0 = g_dbus_connection_signal_subscribe(client, NULL, "org.freedesktop.DBus.Properties", "PropertiesChanged", NULL, "org.mlx.Test", G_DBUS_SIGNAL_FLAGS_NONE, on_signal, "arg0", subscription_gone);
    guint direct = g_dbus_connection_signal_subscribe(client, NULL, "org.mlx.Direct", "Poke", NULL, NULL, G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE, on_signal, "no match rule", subscription_gone);
    guint namespace = g_dbus_connection_signal_subscribe(client, NULL, "org.freedesktop.DBus", "NameOwnerChanged", NULL, "org.mlx.Watched", G_DBUS_SIGNAL_FLAGS_MATCH_ARG0_NAMESPACE, on_owner_changed, "namespace", subscription_gone);

    GDBusProxy *proxy = g_dbus_proxy_new_sync(client, G_DBUS_PROXY_FLAGS_NONE, NULL, "org.mlx.Test", "/org/mlx/Test", "org.mlx.Test", NULL, &error);
    print_error("proxy", error);
    char *owner_name = g_dbus_proxy_get_name_owner(proxy);
    printf("proxy owner %s, name %s, path %s, interface %s, flags %d, timeout %d, connection %d\n", who(owner_name), g_dbus_proxy_get_name(proxy),
        g_dbus_proxy_get_object_path(proxy), g_dbus_proxy_get_interface_name(proxy), g_dbus_proxy_get_flags(proxy), g_dbus_proxy_get_default_timeout(proxy),
        g_dbus_proxy_get_connection(proxy) == client);
    g_free(owner_name);
    print_cache("proxy", proxy);
    g_signal_connect(proxy, "g-signal", G_CALLBACK(proxy_signal), "proxy");
    g_signal_connect(proxy, "g-properties-changed", G_CALLBACK(proxy_properties), "proxy");

    GVariant *v = g_dbus_connection_call_sync(client, "org.mlx.Test", "/org/mlx/Test", "org.mlx.Test", "Emit", NULL, NULL, 0, -1, NULL, &error);
    g_variant_unref(v);
    wait_events(6, "the signals");
    print_cache("proxy after", proxy);

    v = g_dbus_proxy_call_sync(proxy, "Echo", g_variant_new("(s)", "proxied"), 0, -1, NULL, &error);
    print_value("proxy echo", v);
    g_variant_unref(v);
    v = g_dbus_proxy_call_sync(proxy, "org.freedesktop.DBus.Properties.Get", g_variant_new("(ss)", "org.mlx.Test", "Label"), 0, -1, NULL, &error);
    print_value("proxy split name", v);
    g_variant_unref(v);
    g_dbus_proxy_set_interface_info(proxy, iface);
    v = g_dbus_proxy_call_sync(proxy, "Add", g_variant_new("(ii)", 2, 3), 0, -1, NULL, &error);
    print_value("proxy add with info", v);
    g_variant_unref(v);
    g_dbus_proxy_call(proxy, "Echo", g_variant_new("(s)", "proxied async"), 0, -1, NULL, proxy_called, "proxy async");
    wait_events(1, "a proxy call");
    g_dbus_proxy_call(proxy, "Custom", NULL, 0, -1, NULL, proxy_called, "proxy async error");
    wait_events(1, "a proxy call");
    g_dbus_proxy_set_cached_property(proxy, "Count", g_variant_new_uint32(99));
    g_dbus_proxy_set_cached_property(proxy, "Label", NULL);
    print_cache("proxy set", proxy);
    int fds[2];
    if (pipe(fds) == 0) {
        GUnixFDList *list = g_unix_fd_list_new_from_array(&fds[1], 1);
        GUnixFDList *out = NULL;
        v = g_dbus_proxy_call_with_unix_fd_list_sync(proxy, "Pipe", g_variant_new("(h)", 0), 0, -1, list, &out, NULL, &error);
        print_value("proxy pipe", v);
        char buffer[8] = { 0 };
        printf("proxy read %zd '%s', out %d\n", read(fds[0], buffer, 4), buffer, out ? g_unix_fd_list_get_length(out) : -1);
        close(fds[0]);
        g_variant_unref(v);
        g_object_unref(list);
        g_clear_object(&out);
    }

    GDBusProxy *async_proxy = NULL;
    g_dbus_proxy_new(client, G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES, NULL, "org.mlx.Test", "/org/mlx/Test", "org.mlx.Test", NULL, proxy_ready, &async_proxy);
    wait_events(1, "the async proxy");
    print_cache("async proxy", async_proxy);
    GDBusProxy *bus_proxy = NULL;
    g_dbus_proxy_new_for_bus(G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS, NULL, "org.mlx.Test", "/org/mlx/Test", "org.mlx.Test", NULL, bus_proxy_ready, &bus_proxy);
    wait_events(1, "the bus proxy");
    owner_name = g_dbus_proxy_get_name_owner(bus_proxy);
    printf("bus proxy owner %s, same connection %d\n", who(owner_name), g_dbus_proxy_get_connection(bus_proxy) == client);
    g_free(owner_name);
    g_dbus_proxy_call(bus_proxy, "Add", g_variant_new("(ii)", 20, 22), 0, -1, NULL, proxy_called, "bus proxy add");
    wait_events(1, "the bus proxy call");
    GDBusProxy *sync_bus_proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS | G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES, NULL,
        "org.mlx.Test", "/org/mlx/Test", "org.mlx.Test", NULL, &error);
    v = g_dbus_proxy_call_sync(sync_bus_proxy, "Echo", g_variant_new("(s)", "bus"), 0, -1, NULL, &error);
    print_value("sync bus proxy echo", v);
    g_variant_unref(v);

    GDBusProxy *nobody = g_dbus_proxy_new_sync(client, G_DBUS_PROXY_FLAGS_NONE, NULL, "org.mlx.Nobody", "/org/mlx/Nobody", "org.mlx.Nobody", NULL, &error);
    print_error("nobody proxy", error);
    owner_name = g_dbus_proxy_get_name_owner(nobody);
    printf("nobody owner %s\n", who(owner_name));
    print_cache("nobody", nobody);
    v = g_dbus_proxy_call_sync(nobody, "Hello", NULL, 0, -1, NULL, &error);
    print_error("nobody call", error);
    g_clear_error(&error);
    GDBusProxy *nobody_no_start = g_dbus_proxy_new_sync(client, G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START, NULL, "org.mlx.Nobody", "/org/mlx/Nobody", "org.mlx.Nobody", NULL, &error);
    v = g_dbus_proxy_call_sync(nobody_no_start, "Hello", NULL, 0, -1, NULL, &error);
    print_error("nobody no start call", error);
    g_clear_error(&error);
    g_dbus_proxy_call(nobody_no_start, "Hello", NULL, 0, -1, NULL, proxy_called, "nobody no start async");
    wait_events(1, "the nobody call");

    /* Names coming and going, as ATSPI watches its clients. */
    GDBusConnection *watched = g_dbus_connection_new_for_address_sync(address, G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION, NULL, NULL, &error);
    g_signal_connect(watched, "closed", G_CALLBACK(closed), "watched");
    guint watched_owner = g_bus_own_name_on_connection(watched, "org.mlx.Watched.A", G_BUS_NAME_OWNER_FLAGS_NONE, name_acquired, name_lost, "watched", NULL);
    wait_events(2, "the watched name");
    print_owner_log();
    g_bus_unown_name(watched_owner);
    wait_events(1, "the watched name released");
    print_owner_log();
    g_dbus_connection_close_sync(watched, NULL, &error);
    print_error("close", error);
    printf("watched closed %d\n", g_dbus_connection_is_closed(watched));
    wait_events(1, "closed");
    gboolean closed_again = g_dbus_connection_close_sync(watched, NULL, &error);
    print_error("close again", error);
    g_clear_error(&error);
    printf("closed again %d\n", closed_again);
    v = g_dbus_connection_call_sync(watched, "org.mlx.Test", "/org/mlx/Test", "org.mlx.Test", "Echo", g_variant_new("(s)", "x"), NULL, 0, -1, NULL, &error);
    print_error("call on closed", error);
    g_clear_error(&error);
    printf("emit on closed %d\n", g_dbus_connection_emit_signal(watched, NULL, "/a", "org.mlx.X", "Y", NULL, &error));
    print_error("emit on closed", error);
    g_clear_error(&error);
    g_object_unref(watched);

    g_dbus_connection_signal_unsubscribe(client, by_sender);
    g_dbus_connection_signal_unsubscribe(client, by_name);
    g_dbus_connection_signal_unsubscribe(client, arg0);
    g_dbus_connection_signal_unsubscribe(client, direct);
    g_dbus_connection_signal_unsubscribe(client, namespace);
    g_dbus_connection_signal_unsubscribe(client, 999999);
    drain();

    /* Unregistering. */
    gboolean first = g_dbus_connection_unregister_object(service, deep_id);
    gboolean second = g_dbus_connection_unregister_object(service, deep_id);
    gboolean methods = g_dbus_connection_unregister_object(service, methods_id);
    drain();
    printf("unregistered %d %d, methods %d\n", first, second, methods);
    call("after unregistering", "/org/mlx/Test/Child/Deep", "org.mlx.Test", "Echo", g_variant_new("(s)", "x"), NULL);
    g_object_unref(proxy);
    g_object_unref(async_proxy);
    g_object_unref(bus_proxy);
    g_object_unref(sync_bus_proxy);
    g_object_unref(nobody);
    g_object_unref(nobody_no_start);
    g_bus_unown_name(owner);
    drain();
    gboolean test = g_dbus_connection_unregister_object(service, test_id);
    drain();
    printf("unregistered test %d\n", test);
    g_main_loop_quit(service_loop);
    g_thread_join(thread);
    drain();
    g_dbus_node_info_unref(node);
    g_object_unref(made);
    g_object_unref(third);
    g_object_unref(service);
    g_object_unref(client);
    g_free(address);
    printf("done\n");
    return 0;
}
