// The embedder's URL schemes (MlxSchemeHandler.h).
#include "config.h"
#include "MlxSchemeHandler.h"

#include "LegacySchemeRegistry.h"
#include "NetworkLoadMetrics.h"
#include "ResourceError.h"
#include "ResourceLoader.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SharedBuffer.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

// The errors of a request the callback failed, in a domain of the port's.
static constexpr ASCIILiteral errorDomain = "org.mlx.scheme"_s;

static HashMap<String, MlxSchemeHandler::Callback>& callbacks()
{
    static NeverDestroyed<HashMap<String, MlxSchemeHandler::Callback>> callbacks;
    return callbacks;
}

void MlxSchemeHandler::registerScheme(const String& scheme, Callback&& callback)
{
    ASSERT(isMainThread());
    String name = scheme.convertToASCIILowercase();
    LegacySchemeRegistry::registerURLSchemeAsDisplayIsolated(name);
    LegacySchemeRegistry::registerURLSchemeAsSecure(name);
    // (A scheme handler's, as WebKit2's are: its URLs have an origin of
    // their own, scheme, host and port, not an opaque one, so a page of it
    // loads and fetches its own URLs as same-origin.)
    LegacySchemeRegistry::registerURLSchemeAsHandledBySchemeHandler(name);
    callbacks().set(name, WTF::move(callback));
}

bool MlxSchemeHandler::handles(const URL& url)
{
    auto& map = callbacks();
    return !map.isEmpty() && map.contains(url.protocol().convertToASCIILowercase());
}

MlxSchemeTask::MlxSchemeTask(const URL& url, const String& method, RefPtr<ResourceLoader>&& loader)
    : m_url(url)
    , m_method(method.isEmpty() ? "GET"_s : method)
    , m_loader(WTF::move(loader))
{
}

static ResourceResponse responseFor(const URL& url, unsigned status, const String& mimeType, const String& charset, size_t length)
{
    String type = mimeType;
    if (type.isEmpty())
        type = "application/octet-stream"_s;
    ResourceResponse response(URL { url }, WTF::move(type), static_cast<long long>(length), String { charset });
    response.setHTTPStatusCode(status);
    response.setHTTPStatusText(status == 200 ? "OK"_s : status == 404 ? "Not Found"_s : ""_s);
    return response;
}

void MlxSchemeTask::respond(unsigned status, const String& mimeType, const String& charset, Vector<uint8_t>&& body)
{
    if (m_answered)
        return;
    m_answered = true;
    RefPtr loader = std::exchange(m_loader, nullptr);
    if (!loader) {
        m_status = status;
        m_mimeType = mimeType;
        m_charset = charset;
        m_body = WTF::move(body);
        return;
    }
    if (loader->reachedTerminalState())
        return;
    auto response = responseFor(m_url, status, mimeType, charset, body.size());
    loader->didReceiveResponse(WTF::move(response), [loader, body = WTF::move(body)]() mutable {
        if (!loader->reachedTerminalState() && !body.isEmpty() && loader->request().httpMethod() != "HEAD"_s) {
            Ref data = SharedBuffer::create(WTF::move(body));
            loader->didReceiveBuffer(data, data->size(), DataPayloadWholeResource);
        }
        if (!loader->reachedTerminalState())
            loader->didFinishLoading(NetworkLoadMetrics { });
    });
}

void MlxSchemeTask::fail(const String& description)
{
    if (m_answered)
        return;
    m_answered = true;
    RefPtr loader = std::exchange(m_loader, nullptr);
    if (!loader) {
        m_failure = description.isEmpty() ? "The load failed"_s : description;
        return;
    }
    if (!loader->reachedTerminalState())
        loader->didFail(ResourceError(String(errorDomain), 1, m_url, description));
}

void MlxSchemeHandler::load(ResourceLoader& loader)
{
    RunLoop::mainSingleton().dispatch([loader = Ref { loader }]() mutable {
        if (loader->reachedTerminalState())
            return;
        const URL& url = loader->request().url();
        auto entry = callbacks().find(url.protocol().convertToASCIILowercase());
        if (entry == callbacks().end()) {
            loader->didFail(ResourceError(String(errorDomain), 2, url, "No handler for the URL's scheme"_s));
            return;
        }
        Ref task = adoptRef(*new MlxSchemeTask(url, loader->request().httpMethod(), loader.copyRef()));
        entry->value(task);
    });
}

void MlxSchemeHandler::loadSynchronously(const ResourceRequest& request, ResourceError& error, ResourceResponse& response, Vector<uint8_t>& data)
{
    const URL& url = request.url();
    auto entry = callbacks().find(url.protocol().convertToASCIILowercase());
    if (entry == callbacks().end()) {
        error = ResourceError(String(errorDomain), 2, url, "No handler for the URL's scheme"_s);
        return;
    }
    Ref task = adoptRef(*new MlxSchemeTask(url, request.httpMethod(), nullptr));
    entry->value(task);
    if (!task->answered()) {
        // (Answered later: too late for a synchronous load.)
        task->fail(String());
        error = ResourceError(String(errorDomain), 3, url, "The scheme's handler did not answer at once"_s);
        return;
    }
    if (!task->m_failure.isNull()) {
        error = ResourceError(String(errorDomain), 1, url, task->m_failure);
        return;
    }
    response = responseFor(url, task->m_status, task->m_mimeType, task->m_charset, task->m_body.size());
    data = WTF::move(task->m_body);
}

} // namespace WebCore
