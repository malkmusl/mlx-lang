// The network of the Mlx port's embedding (PORT.md, M6): http: and https:
// loads for the loader strategy (MlxPlatformStrategies.cpp), run by the
// Mlx side (port/mlx/http.mlx: std.http's client, std.tls, the cookie
// jar) on one thread of their own, the network queue, and delivered to
// WebCore's ResourceLoader on the main thread as WebKit2's
// WebResourceLoader delivers the network process's: redirects through
// ResourceLoader::willSendRequest (WebCore's policy, then the next
// request), then the response, the data and the end.
//
// The cookie jar is the Mlx side's: the client sends from it and takes
// Set-Cookie into it; document.cookie reaches it through the port's
// NetworkStorageSession, which the page's CookieJar gets from
// MlxNetwork::cookieJar() (PageConfiguration::cookieJar).
#pragma once

#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/Ref.h>

namespace WTF {
class WorkQueue;
}

namespace WebCore {

class CookieJar;
class ResourceError;
class ResourceLoader;
class ResourceRequest;
class ResourceResponse;

class MlxNetwork {
public:
    // The thread every call of the Mlx side's network functions runs on.
    WEBCORE_EXPORT static WTF::WorkQueue& queue();

    // Whether the loader strategy loads the URL's scheme here (http:, https:).
    static bool handles(const URL&);

    // An http: or https: load of the loader, started now.
    static void load(ResourceLoader&);

    // A load run to its end on the calling thread (synchronous XHR):
    // redirects followed, the response and the data, or the error.
    static void loadSynchronously(const ResourceRequest&, ResourceError&, ResourceResponse&, Vector<uint8_t>& data);

    // A request run on the network queue with no loader (a download):
    // redirects followed, the response and its whole body, or the error,
    // given to the handler on the main thread.
    using FetchHandler = CompletionHandler<void(const ResourceError&, ResourceResponse&&, Vector<uint8_t>&&)>;
    static void fetch(const ResourceRequest&, FetchHandler&&);

    // The page's cookie jar over the Mlx side's (document.cookie).
    WEBCORE_EXPORT static Ref<CookieJar> cookieJar();
};

} // namespace WebCore
