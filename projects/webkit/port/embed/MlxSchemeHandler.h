// The embedder's URL schemes (PORT.md, "Internal pages"): a scheme the
// embedder registers with a callback that answers each request of it
// (Voyager's voyager:, webcore-dump's --scheme), as WebKit2's
// WebURLSchemeHandler lets an application serve its own scheme. WebCore
// knows only about:blank and about:srcdoc; a browser's internal pages are
// its own.
//
// A registered scheme is, for WebCore's security checks
// (LegacySchemeRegistry):
//   - display-isolated: only a document of the scheme itself can display
//     a URL of it (SecurityOrigin::canDisplay): an http:, https:, file:
//     or data: page can neither navigate to it (a link, location, a
//     form), frame it, load it as an image, a script or a style sheet,
//     nor fetch it or ask it with XMLHttpRequest; the redirect of a load
//     to it is refused too. A load the embedder starts itself (the
//     address bar: FrameLoader::load) is no page's and is not checked;
//   - secure: its pages are a secure context (no mixed content warnings
//     for what they load of their own);
//   - handled by a scheme handler: its URLs have an origin of their own
//     (scheme, host, port) rather than an opaque one, so a page of it
//     loads and fetches its own URLs as same-origin;
//   - not local: its pages get no access to file: URLs.
// The loader strategy (MlxPlatformStrategies.cpp) gives every load of it
// to the callback instead of the network.
#pragma once

#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ResourceError;
class ResourceLoader;
class ResourceRequest;
class ResourceResponse;

// One request of a registered scheme: its URL and method, answered once
// with respond(), at once (in the callback) or later on the main thread.
// A request that is answered after its load ended (cancelled, the page
// gone) is dropped.
class MlxSchemeTask final : public RefCounted<MlxSchemeTask> {
public:
    const URL& url() const { return m_url; }
    const String& method() const { return m_method; }

    // The answer: STATUS (200, 404, ...), the MIME type of the body
    // ("text/html"), its charset ("utf-8"; empty for none) and its bytes.
    WEBCORE_EXPORT void respond(unsigned status, const String& mimeType, const String& charset, Vector<uint8_t>&& body);
    // A failure without a response (the load fails as a network error).
    WEBCORE_EXPORT void fail(const String& description);

    bool answered() const { return m_answered; }

private:
    friend class MlxSchemeHandler;
    MlxSchemeTask(const URL&, const String& method, RefPtr<ResourceLoader>&&);

    URL m_url;
    String m_method;
    RefPtr<ResourceLoader> m_loader;
    bool m_answered { false };
    // A synchronous load's answer (no loader).
    unsigned m_status { 0 };
    String m_mimeType;
    String m_charset;
    Vector<uint8_t> m_body;
    String m_failure;
};

class MlxSchemeHandler {
public:
    using Callback = Function<void(MlxSchemeTask&)>;

    // SCHEME (lower case, no colon) served by CALLBACK from now on, every
    // page's; registering it again replaces the callback.
    WEBCORE_EXPORT static void registerScheme(const String& scheme, Callback&&);

    // Whether the URL's scheme is a registered one.
    static bool handles(const URL&);

    // A load of the loader, its callback asked on the main thread's next
    // turn.
    static void load(ResourceLoader&);

    // A synchronous load (synchronous XHR): the callback must answer in
    // the call, else the load fails.
    static void loadSynchronously(const ResourceRequest&, ResourceError&, ResourceResponse&, Vector<uint8_t>& data);
};

} // namespace WebCore
