// The network of the Mlx port's embedding (PORT.md, M6): http: and https:
// loads for the loader strategy (MlxPlatformStrategies.cpp), run by the
// Mlx side (port/mlx/http.mlx: std.http's client, std.tls, the cookie
// jar) on one thread of their own, the network queue, and delivered to
// WebCore's ResourceLoader on the main thread as WebKit2's
// WebResourceLoader delivers the network process's: redirects through
// ResourceLoader::willSendRequest (WebCore's policy, then the next
// request), then the response, the data and the end.
//
// A body is read as it comes: once the head is in, the response goes to
// the loader, and a long body (or one of no known length) is read on a
// thread of its own, each piece given to the loader on the main thread
// (the network queue goes on meanwhile), or, once the load is diverted
// (a frame's load turned into a download), to the download's receiver on
// that thread: the bytes are those of the one request WebCore made, POSTs
// and URLs good once included.
//
// The cookie jar is the Mlx side's: the client sends from it and takes
// Set-Cookie into it; document.cookie reaches it through the port's
// NetworkStorageSession, which the page's CookieJar gets from
// MlxNetwork::cookieJar() (PageConfiguration::cookieJar).
#pragma once

#include <span>
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/Ref.h>
#include <wtf/ThreadSafeRefCounted.h>

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

    // Where a body goes once a load is diverted, or a fetch's response
    // taken: each piece on the thread that reads it (false: stop, the
    // connection closed), then its end on the main thread (all of it
    // came: a null error; STOPPED: receive asked to stop).
    class BodyReceiver : public ThreadSafeRefCounted<BodyReceiver> {
    public:
        virtual ~BodyReceiver() = default;
        virtual bool receive(std::span<const uint8_t>) = 0;
        virtual void finished(const ResourceError&, bool stopped) = 0;
    };

    // The body of the loader's response from now on goes to the receiver
    // (WebCore's convertMainResourceLoadToDownload: the frame's load turned
    // into a download, its bytes not asked for again); false when the
    // loader's body is no network one or was taken already.
    static bool divert(ResourceLoader&, Ref<BodyReceiver>&&);
    // The loader is gone (the loader strategy's remove): its body, unless
    // diverted, is read no further.
    static void loaderGone(ResourceLoader&);

    // A request run with no loader (a download, a download resumed):
    // redirects followed, the response (or the error) given to the handler
    // on the main thread, then, when it answers true, the body to the
    // receiver.
    using ResponseHandler = Function<bool(const ResourceError&, const ResourceResponse&)>;
    static void fetch(const ResourceRequest&, ResponseHandler&&, Ref<BodyReceiver>&&);

    // The page's cookie jar over the Mlx side's (document.cookie).
    WEBCORE_EXPORT static Ref<CookieJar> cookieJar();
};

} // namespace WebCore
