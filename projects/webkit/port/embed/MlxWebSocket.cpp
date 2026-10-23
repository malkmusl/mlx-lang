// WebSockets of the Mlx port's embedding (MlxWebSocket.h).
#include "config.h"
#include "MlxWebSocket.h"

#include "Blob.h"
#include "Document.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SocketProvider.h"
#include "ThreadableWebSocketChannel.h"
#include "WebSocketChannelClient.h"
#include "WebTransportSession.h"
#include "port/MlxWebPort.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <wtf/RunLoop.h>
#include <wtf/Threading.h>
#include <wtf/text/CString.h>

namespace WebCore {

namespace {

// A WebSocket's channel: the Mlx side's socket, connected and then read
// on a thread of the channel's own; what it reads goes to the client on
// the main thread. Sending and closing are the main thread's.
class MlxWebSocketChannel final : public ThreadableWebSocketChannel, public RefCounted<MlxWebSocketChannel> {
public:
    static Ref<MlxWebSocketChannel> create(WebSocketChannelClient& client) { return adoptRef(*new MlxWebSocketChannel(client)); }

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

private:
    explicit MlxWebSocketChannel(WebSocketChannelClient& client)
        : m_client(&client)
        , m_progressIdentifier(WebSocketChannelIdentifier::generate())
    {
    }

    ConnectStatus connect(const URL& url, const String& protocol) final
    {
        if (!url.protocolIs("ws"_s) && !url.protocolIs("wss"_s))
            return ConnectStatus::KO;
        m_url = url;
        m_handshakeCreated = true;
        Thread::create("org.mlx.websocket"_s, [protectedThis = Ref { *this }, url = url.string().utf8(), protocol = protocol.utf8()] {
            size_t socket = mlxweb_ws_connect(url.data(), protocol.data());
            RunLoop::mainSingleton().dispatch([protectedThis, socket] {
                protectedThis->didOpen(socket);
            });
            if (!socket)
                return;
            while (true) {
                size_t message = mlxweb_ws_receive(socket);
                uint32_t kind = mlxweb_ws_message_kind(message);
                RunLoop::mainSingleton().dispatch([protectedThis, socket, message] {
                    protectedThis->didReceive(socket, message);
                });
                if (kind != MLXWEB_WS_TEXT && kind != MLXWEB_WS_BINARY)
                    return;
            }
        })->detach();
        return ConnectStatus::OK;
    }

    void didOpen(size_t socket)
    {
        if (!socket) {
            if (m_client) {
                m_client->didReceiveMessageError("WebSocket connection failed"_s);
                m_client->didClose(0, WebSocketChannelClient::ClosingHandshakeIncomplete, CloseEventCodeAbnormalClosure, { });
            }
            return;
        }
        m_socket = socket;
        m_subprotocol = String::fromUTF8(mlxweb_ws_protocol(socket));
        m_extensions = String::fromUTF8(mlxweb_ws_extensions(socket));
        m_connected = true;
        if (m_disconnected) {
            mlxweb_ws_shutdown(socket);
            return;
        }
        if (m_client)
            m_client->didConnect();
    }

    // A message read (the last one when it is no data: then the reading
    // thread has ended and the socket goes).
    void didReceive(size_t socket, size_t message)
    {
        uint32_t kind = mlxweb_ws_message_kind(message);
        std::span<const uint8_t> bytes { mlxweb_ws_message_bytes(message), mlxweb_ws_message_length(message) };
        auto code = static_cast<unsigned short>(mlxweb_ws_message_code(message));
        if (kind == MLXWEB_WS_TEXT && m_client)
            m_client->didReceiveMessage(String::fromUTF8(bytes));
        else if (kind == MLXWEB_WS_BINARY && m_client)
            m_client->didReceiveBinaryData(Vector<uint8_t>(bytes));
        else if (kind == MLXWEB_WS_CLOSE || kind == MLXWEB_WS_ERROR) {
            m_connected = false;
            m_socket = 0;
            mlxweb_ws_release(socket);
            if (m_client) {
                if (kind == MLXWEB_WS_CLOSE)
                    m_client->didStartClosingHandshake();
                m_client->didClose(0, kind == MLXWEB_WS_CLOSE ? WebSocketChannelClient::ClosingHandshakeComplete : WebSocketChannelClient::ClosingHandshakeIncomplete, kind == MLXWEB_WS_CLOSE ? code : CloseEventCodeAbnormalClosure, String::fromUTF8(bytes));
            }
        }
        mlxweb_ws_message_release(message);
    }

    String subprotocol() final { return m_subprotocol; }
    String extensions() final { return m_extensions; }

    void send(CString&& text) final
    {
        if (m_socket)
            mlxweb_ws_send(m_socket, 0, text.data(), text.length());
    }

    void send(const JSC::ArrayBuffer& buffer, unsigned byteOffset, unsigned byteLength) final
    {
        if (m_socket)
            mlxweb_ws_send(m_socket, 1, static_cast<const uint8_t*>(buffer.data()) + byteOffset, byteLength);
    }

    // STUB (later: a Blob's bytes read first): a Blob is not sent.
    void send(Blob&) final
    {
        if (m_client)
            m_client->didReceiveMessageError("Sending a Blob over a WebSocket is not supported yet"_s);
    }

    void close(int code, const String& reason) final
    {
        if (m_socket)
            mlxweb_ws_close(m_socket, code == CloseEventCodeNotSpecified ? 0 : code, reason.utf8().data());
    }

    void fail(String&& reason) final
    {
        if (m_client)
            m_client->didReceiveMessageError(WTF::move(reason));
        if (m_socket)
            mlxweb_ws_shutdown(m_socket);
    }

    void disconnect() final
    {
        m_client = nullptr;
        m_disconnected = true;
        if (m_socket)
            mlxweb_ws_shutdown(m_socket);
    }

    void suspend() final { }
    void resume() final { }

    WebSocketChannelIdentifier progressIdentifier() const final { return m_progressIdentifier; }
    bool hasCreatedHandshake() const final { return m_handshakeCreated; }
    bool isConnected() const final { return m_connected; }
    ResourceRequest clientHandshakeRequest(const CookieGetter&) const final { return ResourceRequest { URL { m_url } }; }
    const ResourceResponse& serverHandshakeResponse() const final { return m_response; }

    WebSocketChannelClient* m_client;
    WebSocketChannelIdentifier m_progressIdentifier;
    URL m_url;
    size_t m_socket { 0 };
    String m_subprotocol;
    String m_extensions;
    ResourceResponse m_response;
    bool m_handshakeCreated { false };
    bool m_connected { false };
    bool m_disconnected { false };
};

class MlxSocketProvider final : public SocketProvider {
public:
    RefPtr<ThreadableWebSocketChannel> createWebSocketChannel(Document&, WebSocketChannelClient& client, IsInitiatedByDedicatedWorker) final
    {
        return MlxWebSocketChannel::create(client);
    }

    std::pair<RefPtr<WebTransportSession>, Ref<WebTransportSessionPromise>> initializeWebTransportSession(ScriptExecutionContext&, WebTransportSessionClient&, const URL&, const WebTransportOptions&) final
    {
        return { nullptr, WebTransportSessionPromise::createAndReject() };
    }
};

}

Ref<SocketProvider> MlxWebSocket::socketProvider()
{
    return adoptRef(*new MlxSocketProvider);
}

} // namespace WebCore
