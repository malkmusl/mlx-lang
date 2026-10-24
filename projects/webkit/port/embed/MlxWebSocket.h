// WebSockets of the Mlx port's embedding (PORT.md, M6): the page's socket
// provider (PageConfiguration::socketProvider), whose channels run the
// Mlx side's WebSocket client (port/mlx/websocket.mlx: the handshake,
// frames, permessage-deflate) with a thread of their own reading, and
// deliver to WebCore's WebSocket on the main thread.
#pragma once

#include <wtf/Ref.h>

namespace WebCore {

class SocketProvider;

class MlxWebSocket {
public:
    WEBCORE_EXPORT static Ref<SocketProvider> socketProvider();
};

} // namespace WebCore
