/* mlx-glib's GIO networking, as JavaScriptCore's remote inspector uses
   it: inet addresses and socket addresses (as GSocketConnectable), a
   GSocketService listening on a GSocketListener's address with its
   "incoming" signal, a GSocketClient connecting to "host:port"
   asynchronously, the connections' sockets and addresses, and the
   errors of an address in use and of a refused connection;
   network.expected is what GLib 2.80 prints for the same program. */
#include <gio/gio.h>
#include <stdio.h>
#include <string.h>

static GMainLoop *loop;
static GSocketConnection *server_side;
static GSocketConnection *client_side;
static int port;
static char *incoming_line;
static char *connected_line;

static void print_address(const char *what, GSocketAddress *address)
{
    char *text = g_socket_connectable_to_string(G_SOCKET_CONNECTABLE(address));
    printf("%s: %s family %d native size %d\n", what, text, g_socket_address_get_family(address), (int)g_socket_address_get_native_size(address));
    g_free(text);
}

static void addresses(void)
{
    const char *texts[] = { "127.0.0.1", "::1", "10.1.2.3", "fe80::1:2", "0.0.0.0", "not an address", "1.2.3", NULL };
    for (int i = 0; texts[i]; i++) {
        GInetAddress *address = g_inet_address_new_from_string(texts[i]);
        if (!address) {
            printf("'%s': no address\n", texts[i]);
            continue;
        }
        char *text = g_inet_address_to_string(address);
        printf("'%s': %s family %d loopback %d any %d size %d\n", texts[i], text, g_inet_address_get_family(address), g_inet_address_get_is_loopback(address),
            g_inet_address_get_is_any(address), (int)g_inet_address_get_native_size(address));
        g_free(text);
        g_object_unref(address);
    }
    GInetAddress *any = g_inet_address_new_any(G_SOCKET_FAMILY_IPV6);
    GInetAddress *loopback = g_inet_address_new_loopback(G_SOCKET_FAMILY_IPV4);
    char *any_text = g_inet_address_to_string(any);
    char *loopback_text = g_inet_address_to_string(loopback);
    printf("any %s loopback %s equal %d\n", any_text, loopback_text, g_inet_address_equal(any, loopback));
    g_free(any_text);
    g_free(loopback_text);

    GSocketAddress *v4 = g_inet_socket_address_new(loopback, 8080);
    print_address("v4", v4);
    GInetAddress *inner = g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(v4));
    printf("v4 port %d same address %d type %s\n", g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(v4)), inner == loopback, G_OBJECT_TYPE_NAME(v4));
    GSocketAddress *v6 = g_inet_socket_address_new_from_string("::1", 2999);
    print_address("v6", v6);
    printf("from bad string %d\n", g_inet_socket_address_new_from_string("nowhere", 1) == NULL);
    printf("is a socket address %d connectable %d\n", G_IS_SOCKET_ADDRESS(v6), G_IS_SOCKET_CONNECTABLE(v6));
    g_object_unref(v4);
    g_object_unref(v6);
    g_object_unref(any);
    g_object_unref(loopback);
}

static void exchange(void)
{
    /* (Which comes first, the service's connection or the client's, is the
       main loop's choice: they are told in one order.) */
    printf("%s%s", incoming_line, connected_line);
    GSocket *client = g_socket_connection_get_socket(client_side);
    GSocket *server = g_socket_connection_get_socket(server_side);
    GError *error = NULL;
    printf("client sent %ld\n", (long)g_socket_send(client, "ping", 4, NULL, &error));
    char buffer[16] = { 0 };
    printf("server received %ld '%s'\n", (long)g_socket_receive(server, buffer, sizeof buffer - 1, NULL, &error), buffer);
    g_socket_send(server, "pong", 4, NULL, &error);
    memset(buffer, 0, sizeof buffer);
    printf("client received %ld '%s'\n", (long)g_socket_receive(client, buffer, sizeof buffer - 1, NULL, &error), buffer);
    GSocketAddress *local = g_socket_connection_get_local_address(client_side, &error);
    GSocketAddress *remote = g_socket_connection_get_remote_address(client_side, &error);
    printf("client remote port is the service's %d, local port differs %d\n", g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(remote)) == port,
        g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(local)) != port);
    g_object_unref(local);
    g_object_unref(remote);
    g_io_stream_close(G_IO_STREAM(client_side), NULL, NULL);
    memset(buffer, 0, sizeof buffer);
    printf("server received after close %ld\n", (long)g_socket_receive(server, buffer, sizeof buffer - 1, NULL, &error));
    g_main_loop_quit(loop);
}

static gboolean on_incoming(GSocketService *service, GSocketConnection *connection, GObject *source, gpointer data)
{
    incoming_line = g_strdup_printf("incoming %s (%s) source object %d connected %d\n", G_OBJECT_TYPE_NAME(connection), (const char *)data, source != NULL,
        g_socket_connection_is_connected(connection));
    server_side = g_object_ref(connection);
    if (client_side)
        exchange();
    return TRUE;
}

static void connected(GObject *object, GAsyncResult *result, gpointer data)
{
    GError *error = NULL;
    GSocketConnection *connection = g_socket_client_connect_to_host_finish(G_SOCKET_CLIENT(object), result, &error);
    if (!connection) {
        printf("connect failed: %s %d: %s\n", g_quark_to_string(error->domain), error->code, error->message);
        g_error_free(error);
        g_main_loop_quit(loop);
        return;
    }
    connected_line = g_strdup_printf("connected %s (%s)\n", G_OBJECT_TYPE_NAME(connection), (const char *)data);
    client_side = connection;
    if (server_side)
        exchange();
}

static void refused(GObject *object, GAsyncResult *result, gpointer data)
{
    GError *error = NULL;
    GSocketConnection *connection = g_socket_client_connect_to_host_finish(G_SOCKET_CLIENT(object), result, &error);
    printf("refused: connection %d, %s %d: %s\n", connection != NULL, g_quark_to_string(error->domain), error->code, error->message);
    g_error_free(error);
    g_main_loop_quit(loop);
}

int main(void)
{
    loop = g_main_loop_new(NULL, FALSE);
    addresses();

    GSocketService *service = g_socket_service_new();
    printf("service %s active %d listener %d\n", G_OBJECT_TYPE_NAME(service), g_socket_service_is_active(service), G_IS_SOCKET_LISTENER(service));
    g_signal_connect(service, "incoming", G_CALLBACK(on_incoming), (gpointer) "service data");
    GInetAddress *loopback = g_inet_address_new_loopback(G_SOCKET_FAMILY_IPV4);
    GSocketAddress *wanted = g_inet_socket_address_new(loopback, 0);
    GSocketAddress *effective = NULL;
    GError *error = NULL;
    gboolean added = g_socket_listener_add_address(G_SOCKET_LISTENER(service), wanted, G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_TCP, NULL, &effective, &error);
    port = g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(effective));
    printf("added %d effective %s port given %d\n", added, G_OBJECT_TYPE_NAME(effective), port != 0);

    GSocketListener *second = g_socket_listener_new();
    gboolean again = g_socket_listener_add_address(second, effective, G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_TCP, NULL, NULL, &error);
    char *effective_text = g_socket_connectable_to_string(G_SOCKET_CONNECTABLE(effective));
    char *expected = g_strdup_printf("Error binding to address %s: Address already in use", effective_text);
    printf("in use: added %d, %s %d: %s\n", again, g_quark_to_string(error->domain), error->code, strcmp(error->message, expected) == 0 ? "(the address's message)" : error->message);
    g_clear_error(&error);
    g_free(expected);
    g_free(effective_text);

    GSocketClient *client = g_socket_client_new();
    char *host = g_strdup_printf("127.0.0.1:%d", port);
    g_socket_client_connect_to_host_async(client, host, 0, NULL, connected, (gpointer) "client data");
    g_main_loop_run(loop);

    /* A port nobody listens on any more. */
    GSocketAddress *closed_address = NULL;
    g_socket_listener_add_address(second, wanted, G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_TCP, NULL, &closed_address, NULL);
    int closed_port = g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(closed_address));
    g_socket_listener_close(second);
    char *closed_host = g_strdup_printf("127.0.0.1:%d", closed_port);
    g_socket_client_connect_to_host_async(client, closed_host, 0, NULL, refused, NULL);
    g_main_loop_run(loop);

    g_socket_service_stop(service);
    printf("stopped: active %d\n", g_socket_service_is_active(service));
    g_object_unref(server_side);
    g_object_unref(client_side);
    g_object_unref(closed_address);
    g_object_unref(second);
    g_object_unref(client);
    g_object_unref(effective);
    g_object_unref(wanted);
    g_object_unref(loopback);
    g_object_unref(service);
    g_free(host);
    g_free(closed_host);
    g_free(incoming_line);
    g_free(connected_line);
    g_main_loop_unref(loop);
    printf("done\n");
    return 0;
}
