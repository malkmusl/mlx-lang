// The network of the Mlx port's embedding (MlxNetwork.h).
#include "config.h"
#include "MlxNetwork.h"

#include "BlobRegistry.h"
#include "BlobRegistryImpl.h"
#include "CookieJar.h"
#include "FormData.h"
#include "HTTPHeaderMap.h"
#include "HTTPParsers.h"
#include "MIMETypeRegistry.h"
#include "NetworkLoadMetrics.h"
#include "NetworkStorageSession.h"
#include "ResourceError.h"
#include "ResourceLoader.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SharedBuffer.h"
#include "StorageSessionProvider.h"
#include "port/MlxWebPort.h"
#include <pal/SessionID.h>
#include <stdio.h>
#include <stdlib.h>
#include <wtf/FileSystem.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/RunLoop.h>
#include <wtf/Threading.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

// The errors of a load that has no response, in a domain of the port's.
static constexpr ASCIILiteral errorDomain = "org.mlx.network"_s;
// The most redirects a load follows (Fetch's limit).
static constexpr unsigned maximumRedirects = 20;

WorkQueue& MlxNetwork::queue()
{
    static NeverDestroyed<Ref<WorkQueue>> queue = WorkQueue::create("org.mlx.network"_s);
    return queue.get();
}

bool MlxNetwork::handles(const URL& url)
{
    return url.protocolIsInHTTPFamily();
}

namespace {

// What a request is, made on the main thread for the network queue.
struct Outgoing {
    CString method;
    CString url;
    CString headers;
    Vector<uint8_t> body;
    bool hasBody { false };
    uint32_t cacheMode { MLXWEB_CACHE_DEFAULT };
    // A request whose long body is read as it comes (the loads of
    // frames, downloads), on a thread of its own.
    bool streamed { false };
};

// What the Mlx side answered, for the main thread.
struct Answer {
    uint32_t error { MLXWEB_HTTP_OTHER };
    unsigned status { 0 };
    Vector<uint8_t> head;
    Vector<uint8_t> body;
    uint32_t source { MLXWEB_SOURCE_NETWORK };
    // The Mlx side's answer when its body is still on its connection
    // (streamed: read by a Transfer), and the length the head gave it
    // (-1: none).
    uintptr_t stream { 0 };
    int64_t streamLength { -1 };
};

}

// The headers WebCore's request has that the client writes itself (the
// connection's, the body's length, the cookies of the jar). WebCore's
// User-Agent is sent: std.http's client then leaves out its own.
static bool clientWritesHeader(const String& name)
{
    return equalLettersIgnoringASCIICase(name, "host"_s)
        || equalLettersIgnoringASCIICase(name, "connection"_s)
        || equalLettersIgnoringASCIICase(name, "content-length"_s)
        || equalLettersIgnoringASCIICase(name, "accept-encoding"_s)
        || equalLettersIgnoringASCIICase(name, "cookie"_s);
}

// The HTTP cache's mode of a request (the profile's cache on disk,
// port/mlx/httpcache.mlx), from WebCore's cache policy.
static uint32_t cacheModeFor(const ResourceRequest& request)
{
    switch (request.cachePolicy()) {
    case ResourceRequestCachePolicy::UseProtocolCachePolicy:
        return MLXWEB_CACHE_DEFAULT;
    case ResourceRequestCachePolicy::ReloadIgnoringCacheData:
        return MLXWEB_CACHE_RELOAD;
    case ResourceRequestCachePolicy::ReturnCacheDataElseLoad:
        return MLXWEB_CACHE_PREFER;
    case ResourceRequestCachePolicy::ReturnCacheDataDontLoad:
        return MLXWEB_CACHE_ONLY;
    case ResourceRequestCachePolicy::DoNotUseAnyCache:
        return MLXWEB_CACHE_BYPASS;
    case ResourceRequestCachePolicy::RefreshAnyCacheData:
        return MLXWEB_CACHE_REVALIDATE;
    }
    return MLXWEB_CACHE_DEFAULT;
}

// A request's body as it is sent: its bytes, the files' contents (a
// form's <input type=file>, a File in a FormData) read in, blobs resolved
// to theirs (BlobRegistryImpl).
static Vector<uint8_t> bodyBytes(FormData& formData)
{
    Ref resolved = formData.resolveBlobReferences(blobRegistry()->blobRegistryImpl());
    Vector<uint8_t> bytes;
    for (auto& element : resolved->elements()) {
        WTF::switchOn(element.data,
            [&](const Vector<uint8_t>& data) {
                bytes.append(data.span());
            },
            [&](const FormDataElement::EncodedFileData& file) {
                auto contents = FileSystem::readEntireFile(file.filename);
                if (!contents)
                    return;
                size_t start = std::min<size_t>(std::max<int64_t>(file.fileStart, 0), contents->size());
                size_t length = file.fileLength < 0 ? contents->size() - start : std::min<size_t>(file.fileLength, contents->size() - start);
                bytes.append(contents->subspan(start, length));
            },
            [&](const FormDataElement::EncodedBlobData&) { });
    }
    return bytes;
}

static Outgoing outgoingFor(const ResourceRequest& request)
{
    Outgoing outgoing;
    outgoing.cacheMode = cacheModeFor(request);
    String method = request.httpMethod();
    outgoing.method = (method.isEmpty() ? "GET"_s : method).utf8();
    outgoing.url = request.url().string().utf8();
    StringBuilder headers;
    for (auto& field : request.httpHeaderFields()) {
        if (clientWritesHeader(field.key))
            continue;
        headers.append(field.key, ": "_s, field.value, "\r\n"_s);
    }
    outgoing.headers = headers.toString().utf8();
    if (RefPtr body = request.httpBody()) {
        outgoing.body = bodyBytes(*body);
        outgoing.hasBody = true;
    } else
        outgoing.hasBody = method == "POST"_s || method == "PUT"_s;
    return outgoing;
}

// Runs a request on the calling thread (the network queue). A streamed
// one's long body stays with the Mlx side's answer (answer.stream).
static Answer run(const Outgoing& outgoing)
{
    Answer answer;
    auto request = outgoing.streamed ? mlxweb_http_request_streamed : mlxweb_http_request;
    size_t handle = request(outgoing.method.data(), outgoing.url.data(), outgoing.headers.data(), outgoing.body.span().data(), outgoing.body.size(), outgoing.hasBody ? 1 : 0, outgoing.cacheMode);
    if (!handle)
        return answer;
    answer.error = mlxweb_http_error(handle);
    answer.status = mlxweb_http_status(handle);
    answer.source = mlxweb_http_source(handle);
    if (auto* head = mlxweb_http_head(handle))
        answer.head.append(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(head), mlxweb_http_head_length(handle)));
    if (auto* body = mlxweb_http_body(handle))
        answer.body.append(std::span<const uint8_t>(body, mlxweb_http_body_length(handle)));
    if (!answer.error && mlxweb_http_has_stream(handle)) {
        answer.stream = handle;
        answer.streamLength = mlxweb_http_stream_length(handle);
        return answer;
    }
    mlxweb_http_release(handle);
    return answer;
}

static ResourceError errorFor(const URL& url, uint32_t error)
{
    switch (error) {
    case MLXWEB_HTTP_BAD_URL:
        return ResourceError(String(errorDomain), static_cast<int>(error), url, "The URL is not one the network loads"_s);
    case MLXWEB_HTTP_CONNECT:
        return ResourceError(String(errorDomain), static_cast<int>(error), url, "Could not connect to the server"_s);
    case MLXWEB_HTTP_TLS:
        return ResourceError(String(errorDomain), static_cast<int>(error), url, "The secure connection failed (the server's certificate did not verify)"_s);
    case MLXWEB_HTTP_RECEIVE:
        return ResourceError(String(errorDomain), static_cast<int>(error), url, "The connection ended before the response was whole"_s);
    case MLXWEB_HTTP_NOT_CACHED:
        return ResourceError(String(errorDomain), static_cast<int>(error), url, "The response is not in the cache"_s);
    default:
        return ResourceError(String(errorDomain), static_cast<int>(error), url, "The load failed"_s);
    }
}

// MLX_NETWORK_TRACE in the environment: each request and what came back,
// on standard error.
static bool networkTracing()
{
    static bool trace = getenv("MLX_NETWORK_TRACE");
    return trace;
}

// Bytes of a head as a string (a header's bytes are Latin-1).
static String latin1(std::span<const uint8_t> bytes)
{
    return String(std::span<const char>(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
}

// The response of the answer's head, from URL.
static ResourceResponse responseFor(const URL& url, const Answer& answer)
{
    // The head's lines, "\r\n" apart: the status line ("HTTP/1.1 200 OK":
    // the phrase after the code), then "Name: value".
    auto bytes = answer.head.span();
    String statusText;
    String contentType;
    Vector<std::pair<String, String>> fields;
    size_t start = 0;
    bool first = true;
    while (start < bytes.size()) {
        size_t end = start;
        while (end < bytes.size() && bytes[end] != '\n')
            ++end;
        size_t lineEnd = end > start && bytes[end - 1] == '\r' ? end - 1 : end;
        auto line = bytes.subspan(start, lineEnd - start);
        start = end + 1;
        if (first) {
            first = false;
            size_t spaces = 0;
            for (size_t i = 0; i < line.size(); ++i) {
                if (line[i] == ' ' && ++spaces == 2) {
                    statusText = latin1(line.subspan(i + 1));
                    break;
                }
            }
            continue;
        }
        size_t colon = 0;
        while (colon < line.size() && line[colon] != ':')
            ++colon;
        if (colon == line.size())
            continue;
        size_t valueStart = colon + 1;
        while (valueStart < line.size() && (line[valueStart] == ' ' || line[valueStart] == '\t'))
            ++valueStart;
        size_t valueEnd = line.size();
        while (valueEnd > valueStart && (line[valueEnd - 1] == ' ' || line[valueEnd - 1] == '\t'))
            --valueEnd;
        String name = latin1(line.first(colon));
        String value = latin1(line.subspan(valueStart, valueEnd - valueStart));
        if (equalLettersIgnoringASCIICase(name, "content-type"_s))
            contentType = value;
        fields.append({ WTF::move(name), WTF::move(value) });
    }
    String mimeType = extractMIMETypeFromMediaType(contentType);
    if (mimeType.isEmpty())
        mimeType = MIMETypeRegistry::mimeTypeForPath(url.path().toString());
    if (mimeType.isEmpty())
        mimeType = "text/plain"_s;
    long long length = answer.stream ? static_cast<long long>(answer.streamLength) : static_cast<long long>(answer.body.size());
    ResourceResponse response(URL { url }, WTF::move(mimeType), length, extractCharsetFromMediaType(contentType).toString());
    response.setHTTPStatusCode(answer.status);
    response.setHTTPStatusText(AtomString { statusText });
    response.setHTTPVersion("HTTP/1.1"_s);
    for (auto& field : fields)
        response.addHTTPHeaderField(field.first, field.second);
    if (networkTracing())
        fprintf(stderr, "network: %zu header fields, type %s, status text %s\n", fields.size(), contentType.utf8().data(), statusText.utf8().data());
    return response;
}

// The request a redirect leads to, or a null one when the response is no
// redirect.
static ResourceRequest redirected(const ResourceRequest& request, const ResourceResponse& response)
{
    auto status = response.httpStatusCode();
    if (status != 301 && status != 302 && status != 303 && status != 307 && status != 308)
        return { };
    String location = response.httpHeaderField(HTTPHeaderName::Location);
    if (location.isEmpty())
        return { };
    URL next(request.url(), location);
    if (!next.isValid())
        return { };
    ResourceRequest nextRequest = request;
    nextRequest.setURL(WTF::move(next));
    // 303 (and 301 and 302 after a POST) are followed by a GET without
    // the body.
    if ((status == 303 && request.httpMethod() != "HEAD"_s) || ((status == 301 || status == 302) && request.httpMethod() == "POST"_s)) {
        nextRequest.setHTTPMethod("GET"_s);
        nextRequest.setHTTPBody(nullptr);
        nextRequest.clearHTTPContentType();
    }
    return nextRequest;
}

// A response's body on its way: the bytes that came (a whole body, or
// the pieces a streamed one's thread has read) go to the loader once its
// response is taken (didReceiveResponse's completion), or to a receiver
// once the load is diverted (a download: convertMainResourceLoadToDownload),
// and the bytes that came before either wait. The thread reading a
// streamed body (readBody) gives each piece to take(); the end comes to
// the main thread (ended).
class Transfer : public ThreadSafeRefCounted<Transfer> {
public:
    static Ref<Transfer> create(ResourceLoader* loader) { return adoptRef(*new Transfer(loader)); }

    // The reading thread's (or a whole body's): false stops the read.
    bool take(std::span<const uint8_t> bytes)
    {
        RefPtr<MlxNetwork::BodyReceiver> receiver;
        {
            Locker locker { m_lock };
            if (m_stopped)
                return false;
            if (!m_receiver) {
                if (!m_toLoader) {
                    m_pending.append(bytes);
                    return true;
                }
                RunLoop::mainSingleton().dispatch([transfer = Ref { *this }, piece = Vector<uint8_t>(bytes)]() mutable {
                    transfer->deliverToLoader(WTF::move(piece));
                });
                return true;
            }
            receiver = m_receiver;
        }
        if (!receiver->receive(bytes)) {
            Locker locker { m_lock };
            m_stopped = true;
            return false;
        }
        return true;
    }

    // Main thread: the loader takes the bytes from now on.
    void startLoader()
    {
        Vector<uint8_t> pending;
        {
            Locker locker { m_lock };
            if (m_receiver || m_stopped)
                return;
            m_toLoader = true;
            pending = std::exchange(m_pending, { });
        }
        if (!pending.isEmpty())
            deliverToLoader(WTF::move(pending));
        if (m_ended)
            finishLoader();
    }

    // Main thread: the receiver takes the bytes from now on (those that
    // came before first); false when the loader took some already.
    bool divert(Ref<MlxNetwork::BodyReceiver>&& receiver)
    {
        Vector<uint8_t> pending;
        {
            Locker locker { m_lock };
            if (m_toLoader || m_receiver || m_stopped)
                return false;
            m_receiver = receiver.copyRef();
            pending = std::exchange(m_pending, { });
        }
        m_loader = nullptr;
        if (!pending.isEmpty() && !receiver->receive(pending.span())) {
            Locker locker { m_lock };
            m_stopped = true;
        }
        if (m_ended)
            finishReceiver();
        return true;
    }

    // Main thread: the body's end (READ: what streamRead answered, 0 all
    // came, 1 stopped, 2 failed).
    void ended(uint32_t read)
    {
        m_ended = true;
        m_read = read;
        RefPtr<MlxNetwork::BodyReceiver> receiver;
        bool toLoader;
        {
            Locker locker { m_lock };
            receiver = m_receiver;
            toLoader = m_toLoader;
        }
        if (receiver)
            finishReceiver();
        else if (toLoader)
            finishLoader();
        // (Neither yet: whichever comes finishes.)
    }

    // Main thread: the loader is gone (cancelled, finished): a body not
    // diverted is read no further.
    void loaderGone()
    {
        m_loader = nullptr;
        Locker locker { m_lock };
        if (!m_receiver)
            m_stopped = true;
    }

    bool isEnded() const { return m_ended; }
    const URL& url() const { return m_url; }
    void setURL(const URL& url) { m_url = url; }

private:
    explicit Transfer(ResourceLoader* loader)
        : m_loader(loader)
    {
    }

    void deliverToLoader(Vector<uint8_t>&& bytes)
    {
        RefPtr loader = m_loader;
        if (!loader || loader->reachedTerminalState()) {
            loaderGone();
            return;
        }
        if (loader->request().httpMethod() == "HEAD"_s)
            return;
        Ref data = SharedBuffer::create(WTF::move(bytes));
        loader->didReceiveBuffer(data, data->size(), DataPayloadBytes);
    }

    void finishLoader()
    {
        RefPtr loader = std::exchange(m_loader, nullptr);
        if (!loader || loader->reachedTerminalState())
            return;
        if (m_read == 2)
            loader->didFail(ResourceError(String(errorDomain), MLXWEB_HTTP_RECEIVE, m_url, "The connection ended before the response was whole"_s));
        else if (m_read == 0)
            loader->didFinishLoading(NetworkLoadMetrics { });
    }

    void finishReceiver()
    {
        RefPtr<MlxNetwork::BodyReceiver> receiver;
        bool stopped;
        {
            Locker locker { m_lock };
            receiver = std::exchange(m_receiver, nullptr);
            stopped = m_stopped;
            m_stopped = true;
        }
        if (!receiver)
            return;
        if (m_read == 2)
            receiver->finished(ResourceError(String(errorDomain), MLXWEB_HTTP_RECEIVE, m_url, "The connection ended before the download was whole"_s), false);
        else
            receiver->finished({ }, stopped || m_read == 1);
    }

    // The main thread's.
    RefPtr<ResourceLoader> m_loader;
    URL m_url;
    bool m_ended { false };
    uint32_t m_read { 0 };

    Lock m_lock;
    RefPtr<MlxNetwork::BodyReceiver> m_receiver WTF_GUARDED_BY_LOCK(m_lock);
    Vector<uint8_t> m_pending WTF_GUARDED_BY_LOCK(m_lock);
    bool m_toLoader WTF_GUARDED_BY_LOCK(m_lock) { false };
    bool m_stopped WTF_GUARDED_BY_LOCK(m_lock) { false };
};

// The loads whose bodies are on their way, by loader (the main thread's).
static HashMap<ResourceLoader*, Ref<Transfer>>& transfers()
{
    static NeverDestroyed<HashMap<ResourceLoader*, Ref<Transfer>>> transfers;
    return transfers;
}

static uint32_t takePiece(uintptr_t context, const uint8_t* bytes, size_t length)
{
    return reinterpret_cast<Transfer*>(context)->take(std::span<const uint8_t>(bytes, length)) ? 1 : 0;
}

// The body of the answer into the transfer: a whole one at once (its end
// noted now), a streamed one read on a thread of its own.
static void feed(Transfer& transfer, Answer&& answer)
{
    if (!answer.stream) {
        if (!answer.body.isEmpty())
            transfer.take(answer.body.span());
        transfer.ended(0);
        return;
    }
    Thread::create("org.mlx.network.body"_s, [transfer = Ref { transfer }, stream = answer.stream]() mutable {
        uint32_t read = mlxweb_http_stream_read(stream, reinterpret_cast<uintptr_t>(&takePiece), reinterpret_cast<uintptr_t>(transfer.ptr()));
        mlxweb_http_release(stream);
        RunLoop::mainSingleton().dispatch([transfer = WTF::move(transfer), read] {
            transfer->ended(read);
        });
    })->detach();
}

static void releaseStream(Answer& answer)
{
    if (answer.stream)
        mlxweb_http_release(std::exchange(answer.stream, 0));
}

static void start(Ref<ResourceLoader>&&, const ResourceRequest&, unsigned redirects);

static void deliver(Ref<ResourceLoader>&& loader, const ResourceRequest& request, Answer&& answer, unsigned redirects)
{
    if (networkTracing())
        fprintf(stderr, "network: %s %s: %s %u (%zu bytes%s)%s%s\n", request.httpMethod().utf8().data(), request.url().string().utf8().data(), answer.error ? "error" : "status", answer.error ? answer.error : answer.status, answer.body.size(), answer.stream ? ", then streamed" : "", answer.source == MLXWEB_SOURCE_CACHE ? ", from the cache" : answer.source == MLXWEB_SOURCE_REVALIDATED ? ", from the cache after a 304" : "", loader->reachedTerminalState() ? ", the load ended already" : "");
    if (loader->reachedTerminalState()) {
        releaseStream(answer);
        return;
    }
    if (answer.error) {
        loader->didFail(errorFor(request.url(), answer.error));
        return;
    }
    auto response = responseFor(request.url(), answer);
    auto next = redirected(request, response);
    if (!next.isNull()) {
        releaseStream(answer);
        if (redirects >= maximumRedirects) {
            loader->didFail(ResourceError(String(errorDomain), 6, request.url(), "Too many redirects"_s));
            return;
        }
        loader->willSendRequest(WTF::move(next), response, [loader, redirects](ResourceRequest&& request) mutable {
            if (request.isNull() || loader->reachedTerminalState())
                return;
            start(WTF::move(loader), request, redirects + 1);
        });
        return;
    }
    // The body comes as the loader takes it, or a download diverts it
    // (MlxNetwork::divert) while the response's policy is decided.
    Ref transfer = Transfer::create(loader.ptr());
    transfer->setURL(request.url());
    transfers().set(loader.ptr(), transfer.copyRef());
    feed(transfer, WTF::move(answer));
    loader->didReceiveResponse(WTF::move(response), [loader, transfer]() mutable {
        if (!loader->reachedTerminalState())
            transfer->startLoader();
    });
}

static void start(Ref<ResourceLoader>&& loader, const ResourceRequest& request, unsigned redirects)
{
    auto outgoing = outgoingFor(request);
    outgoing.streamed = true;
    MlxNetwork::queue().dispatch([loader = WTF::move(loader), request = request.isolatedCopy(), outgoing = WTF::move(outgoing), redirects]() mutable {
        auto answer = run(outgoing);
        RunLoop::mainSingleton().dispatch([loader = WTF::move(loader), request = WTF::move(request), answer = WTF::move(answer), redirects]() mutable {
            deliver(WTF::move(loader), request, WTF::move(answer), redirects);
        });
    });
}

void MlxNetwork::load(ResourceLoader& loader)
{
    start(Ref { loader }, loader.request(), 0);
}

bool MlxNetwork::divert(ResourceLoader& loader, Ref<BodyReceiver>&& receiver)
{
    auto transfer = transfers().take(&loader);
    if (!transfer)
        return false;
    return transfer->divert(WTF::move(receiver));
}

void MlxNetwork::loaderGone(ResourceLoader& loader)
{
    if (auto transfer = transfers().take(&loader))
        transfer->loaderGone();
}

void MlxNetwork::loadSynchronously(const ResourceRequest& original, ResourceError& error, ResourceResponse& response, Vector<uint8_t>& data)
{
    ResourceRequest request = original;
    for (unsigned redirects = 0; ; ++redirects) {
        auto outgoing = outgoingFor(request);
        Answer answer;
        queue().dispatchSync([&] {
            answer = run(outgoing);
        });
        if (answer.error) {
            error = errorFor(request.url(), answer.error);
            return;
        }
        response = responseFor(request.url(), answer);
        auto next = redirected(request, response);
        if (next.isNull()) {
            data = WTF::move(answer.body);
            return;
        }
        if (redirects >= maximumRedirects) {
            error = ResourceError(String(errorDomain), 6, request.url(), "Too many redirects"_s);
            return;
        }
        request = WTF::move(next);
    }
}

static void fetchFrom(ResourceRequest&& request, unsigned redirects, MlxNetwork::ResponseHandler&& handler, Ref<MlxNetwork::BodyReceiver>&& receiver)
{
    auto outgoing = outgoingFor(request);
    outgoing.streamed = true;
    MlxNetwork::queue().dispatch([request = request.isolatedCopy(), outgoing = WTF::move(outgoing), redirects, handler = WTF::move(handler), receiver = WTF::move(receiver)]() mutable {
        auto answer = run(outgoing);
        RunLoop::mainSingleton().dispatch([request = WTF::move(request), answer = WTF::move(answer), redirects, handler = WTF::move(handler), receiver = WTF::move(receiver)]() mutable {
            if (answer.error) {
                handler(errorFor(request.url(), answer.error), { });
                return;
            }
            auto response = responseFor(request.url(), answer);
            auto next = redirected(request, response);
            if (next.isNull()) {
                if (!handler({ }, response)) {
                    releaseStream(answer);
                    return;
                }
                Ref transfer = Transfer::create(nullptr);
                transfer->setURL(request.url());
                transfer->divert(WTF::move(receiver));
                feed(transfer, WTF::move(answer));
                return;
            }
            releaseStream(answer);
            if (redirects >= maximumRedirects) {
                handler(ResourceError(String(errorDomain), 6, request.url(), "Too many redirects"_s), { });
                return;
            }
            fetchFrom(WTF::move(next), redirects + 1, WTF::move(handler), WTF::move(receiver));
        });
    });
}

void MlxNetwork::fetch(const ResourceRequest& request, ResponseHandler&& handler, Ref<BodyReceiver>&& receiver)
{
    fetchFrom(ResourceRequest { request }, 0, WTF::move(handler), WTF::move(receiver));
}

namespace {

// The page's storage session: the port's NetworkStorageSession (cookies
// over the Mlx side's jar, NetworkStorageSessionMlx.cpp).
class MlxStorageSessionProvider final : public StorageSessionProvider {
public:
    NetworkStorageSession* storageSession() const final
    {
        static NeverDestroyed<std::unique_ptr<NetworkStorageSession>> session = makeUnique<NetworkStorageSession>(PAL::SessionID::defaultSessionID(), nullptr);
        return session.get().get();
    }
};

}

Ref<CookieJar> MlxNetwork::cookieJar()
{
    return CookieJar::create(adoptRef(*new MlxStorageSessionProvider));
}

} // namespace WebCore
