// The Mlx port's platform strategies (MlxPlatformStrategies.h).
#include "config.h"
#include "MlxPlatformStrategies.h"

#include "BlobRegistry.h"
#include "BlobRegistryImpl.h"
#include "CachedResource.h"
#include "FrameLoader.h"
#include "LoaderStrategy.h"
#include "LocalFrame.h"
#include "MIMETypeRegistry.h"
#include "MediaStrategy.h"
#include "NetworkLoadMetrics.h"
#include "ResourceError.h"
#include "ResourceHandle.h"
#include "ResourceLoader.h"
#include "ResourceResponse.h"
#include "SharedBuffer.h"
#include "SubresourceLoader.h"
#include "port/MlxWebPort.h"
#include <wtf/RunLoop.h>
#include <wtf/text/CString.h>

namespace WebCore {

// The errors WebCore asks the strategy to make, in WebKit's internal domain
// with NSURLError's codes, which WebCore's own checks compare with.
static constexpr int cancelledCode = -999;
static constexpr int blockedCode = 103;
static constexpr int cannotShowURLCode = 101;
static constexpr int interruptedCode = 102;
static constexpr int cannotShowMIMETypeCode = 100;
static constexpr int fileDoesNotExistCode = -1100;
static constexpr int pluginWillHandleCode = 204;
static constexpr int httpsOnlyCode = 106;

static ResourceError errorFor(const URL& url, int code, ASCIILiteral description, ResourceError::Type type = ResourceError::Type::General)
{
    return ResourceError(errorDomainWebKitInternal, code, url, description, type);
}

// A file: URL's file, read whole by the Mlx side (std.fs) and given to the
// loader as WebKit2's WebResourceLoader gives the network process's: the
// response (typed by the file's extension, MIMETypeRegistryMlx.cpp), once
// WebCore has decided on it the data, then the end.
static void loadFile(ResourceLoader& loader)
{
    if (loader.reachedTerminalState())
        return;
    URL url = loader.request().url();
    String path = url.fileSystemPath();
    uint32_t reason = 0;
    size_t contents = mlxweb_file_read(path.utf8().data(), &reason);
    if (!contents) {
        loader.didFail(reason == MLXWEB_FILE_NOT_FOUND
            ? errorFor(url, fileDoesNotExistCode, "The file does not exist"_s)
            : errorFor(url, cannotShowURLCode, "The file cannot be read"_s));
        return;
    }
    size_t length = mlxweb_file_length(contents);
    Ref data = SharedBuffer::create(std::span<const uint8_t>(mlxweb_file_bytes(contents), length));
    mlxweb_file_release(contents);

    String mimeType = MIMETypeRegistry::mimeTypeForPath(path);
    if (mimeType.isEmpty())
        mimeType = "application/octet-stream"_s;
    ResourceResponse response(WTF::move(url), WTF::move(mimeType), static_cast<long long>(length), String());
    loader.didReceiveResponse(WTF::move(response), [loader = Ref { loader }, data = WTF::move(data)] {
        if (!loader->reachedTerminalState() && data->size() && loader->request().httpMethod() != "HEAD"_s)
            loader->didReceiveBuffer(data, data->size(), DataPayloadWholeResource);
        if (!loader->reachedTerminalState())
            loader->didFinishLoading(NetworkLoadMetrics { });
    });
}

// Each load a SubresourceLoader (the main resource's too), run as WebKit2's
// WebLoaderStrategy runs them, from the main loop: data: URLs by WebCore
// itself (ResourceLoader::start, DataURLDecoder), file: URLs by the port
// (loadFile, std.fs). No scheduling by priority yet.
//
// STUB (M6: http and https over std.http and std.tls): other schemes fail
// as URLs that cannot be shown.
class MlxLoaderStrategy final : public LoaderStrategy {
public:
    void loadResource(LocalFrame& frame, CachedResource& resource, ResourceRequest&& request, const ResourceLoaderOptions& options, CompletionHandler<void(RefPtr<SubresourceLoader>&&)>&& completionHandler) override
    {
        SubresourceLoader::create(frame, resource, WTF::move(request), options, [completionHandler = WTF::move(completionHandler)](RefPtr<SubresourceLoader>&& loader) mutable {
            if (loader)
                schedule(*loader);
            completionHandler(WTF::move(loader));
        });
    }

    static void schedule(ResourceLoader& loader)
    {
        const URL& url = loader.request().url();
        if (url.protocolIsData()) {
            loader.start();
            return;
        }
        if (url.protocolIsFile()) {
            RunLoop::mainSingleton().dispatch([loader = Ref { loader }] {
                loadFile(loader);
            });
            return;
        }
        RunLoop::mainSingleton().dispatch([loader = Ref { loader }] {
            if (!loader->reachedTerminalState())
                loader->didFail(errorFor(loader->request().url(), cannotShowURLCode, "The port does not load this URL yet"_s));
        });
    }

    void loadResourceSynchronously(FrameLoader& frameLoader, ResourceLoaderIdentifier, const ResourceRequest& request, ClientCredentialPolicy, const FetchOptions&, const HTTPHeaderMap&, ResourceError& error, ResourceResponse& response, Vector<uint8_t>& data) override
    {
        ResourceHandle::loadResourceSynchronously(frameLoader.networkingContext(), request, StoredCredentialsPolicy::Use, nullptr, error, response, data);
    }

    void pageLoadCompleted(Page&) override { }
    void browsingContextRemoved(LocalFrame&) override { }
    void remove(ResourceLoader*) override { }
    void setDefersLoading(ResourceLoader&, bool) override { }
    void crossOriginRedirectReceived(ResourceLoader*, const URL&) override { }
    void servePendingRequests(ResourceLoadPriority) override { }
    void suspendPendingRequests() override { }
    void resumePendingRequests() override { }

    // Pings (beacons, hyperlink auditing) go out with the network (M6).
    void startPingLoad(LocalFrame&, ResourceRequest& request, const HTTPHeaderMap&, const FetchOptions&, ContentSecurityPolicyImposition, PingLoadCompletionHandler&& completionHandler) override
    {
        if (completionHandler)
            completionHandler(errorFor(request.url(), cannotShowURLCode, "Ping loads are not supported yet"_s), { });
    }

    void preconnectTo(FrameLoader&, ResourceRequest&& request, StoredCredentialsPolicy, ShouldPreconnectAsFirstParty, PreconnectCompletionHandler&& completionHandler) override
    {
        if (completionHandler)
            completionHandler(errorFor(request.url(), cannotShowURLCode, "Preconnecting is not supported"_s));
    }

    void setCaptureExtraNetworkLoadMetricsEnabled(bool) override { }

    bool isOnLine() const override { return true; }
    void addOnlineStateChangeListener(Function<void(bool)>&&) override { }

    void isResourceLoadFinished(CachedResource& resource, CompletionHandler<void(bool)>&& callback) override
    {
        callback(resource.isLoaded());
    }

    ResourceError cancelledError(const ResourceRequest& request) const override { return errorFor(request.url(), cancelledCode, "Load cancelled"_s, ResourceError::Type::Cancellation); }
    ResourceError blockedError(const ResourceRequest& request) const override { return errorFor(request.url(), blockedCode, "Not allowed to use restricted network port"_s); }
    bool isBlockedError(const ResourceError& error) const override { return error.errorCode() == blockedCode && error.domain() == errorDomainWebKitInternal; }
    ResourceError blockedByContentBlockerError(const ResourceRequest& request) const override { return errorFor(request.url(), blockedCode, "The URL was blocked by a content blocker"_s); }
    ResourceError cannotShowURLError(const ResourceRequest& request) const override { return errorFor(request.url(), cannotShowURLCode, "The URL can't be shown"_s); }
    ResourceError interruptedForPolicyChangeError(const ResourceRequest& request) const override { return errorFor(request.url(), interruptedCode, "Frame load was interrupted"_s); }
    ResourceError blockedByContentFilterError(const ResourceRequest& request) const override { return errorFor(request.url(), blockedCode, "The URL was blocked by a content filter"_s); }
    ResourceError cannotShowMIMETypeError(const ResourceResponse& response) const override { return errorFor(response.url(), cannotShowMIMETypeCode, "The content of the URL can't be shown"_s); }
    ResourceError fileDoesNotExistError(const ResourceResponse& response) const override { return errorFor(response.url(), fileDoesNotExistCode, "The file does not exist"_s); }
    ResourceError httpsUpgradeRedirectLoopError(const ResourceRequest& request) const override { return errorFor(request.url(), httpsOnlyCode, "HTTPS upgrade redirect loop detected"_s); }
    ResourceError httpNavigationWithHTTPSOnlyError(const ResourceRequest& request) const override { return errorFor(request.url(), httpsOnlyCode, "Navigation failed because the request was for an HTTP URL with HTTPS-Only enabled"_s); }
    bool isHttpNavigationWithHTTPSOnlyError(const ResourceError& error) const override { return error.errorCode() == httpsOnlyCode && error.domain() == errorDomainWebKitInternal; }
    ResourceError pluginWillHandleLoadError(const ResourceResponse& response) const override { return errorFor(response.url(), pluginWillHandleCode, "Plug-in handled load"_s); }
};

// Media: nothing to make (video and web audio are off in the port).
class MlxMediaStrategy final : public MediaStrategy {
};

// Blobs in the process, as WebKitLegacy's WebBlobRegistry kept them.
class MlxBlobRegistry final : public BlobRegistry {
private:
    void registerInternalFileBlobURL(const URL& url, Ref<BlobDataFileReference>&& reference, const String&, const String& contentType) override { m_registry.registerInternalFileBlobURL(url, WTF::move(reference), contentType); }
    void registerInternalBlobURL(const URL& url, Vector<BlobPart>&& parts, const String& contentType) override { m_registry.registerInternalBlobURL(url, WTF::move(parts), contentType); }
    void registerBlobURL(const URL& url, const URL& sourceURL, const PolicyContainer& policyContainer, const std::optional<SecurityOriginData>& topOrigin) override { m_registry.registerBlobURL(url, sourceURL, policyContainer, topOrigin); }
    void registerInternalBlobURLOptionallyFileBacked(const URL& url, const URL& sourceURL, RefPtr<BlobDataFileReference>&& reference, const String& contentType) override { m_registry.registerInternalBlobURLOptionallyFileBacked(url, sourceURL, WTF::move(reference), contentType, { }); }
    void registerInternalBlobURLForSlice(const URL& url, const URL& sourceURL, long long start, long long end, const String& contentType) override { m_registry.registerInternalBlobURLForSlice(url, sourceURL, start, end, contentType); }
    void unregisterBlobURL(const URL& url, const std::optional<SecurityOriginData>& topOrigin) override { m_registry.unregisterBlobURL(url, topOrigin); }
    void registerBlobURLHandle(const URL& url, const std::optional<SecurityOriginData>& topOrigin) override { m_registry.registerBlobURLHandle(url, topOrigin); }
    void unregisterBlobURLHandle(const URL& url, const std::optional<SecurityOriginData>& topOrigin) override { m_registry.unregisterBlobURLHandle(url, topOrigin); }
    String blobType(const URL& url) override { return m_registry.blobType(url); }
    unsigned long long blobSize(const URL& url) override { return m_registry.blobSize(url); }
    void writeBlobsToTemporaryFilesForIndexedDB(const Vector<String>& blobURLs, CompletionHandler<void(Vector<String>&& filePaths)>&& completionHandler) override { m_registry.writeBlobsToTemporaryFilesForIndexedDB(blobURLs, WTF::move(completionHandler)); }
    BlobRegistryImpl* blobRegistryImpl() override { return &m_registry; }

    BlobRegistryImpl m_registry;
};

void MlxPlatformStrategies::initialize()
{
    static MlxPlatformStrategies* strategies = new MlxPlatformStrategies;
    setPlatformStrategies(strategies);
}

LoaderStrategy* MlxPlatformStrategies::createLoaderStrategy()
{
    return new MlxLoaderStrategy;
}

PasteboardStrategy* MlxPlatformStrategies::createPasteboardStrategy()
{
    return nullptr;
}

MediaStrategy* MlxPlatformStrategies::createMediaStrategy()
{
    return new MlxMediaStrategy;
}

BlobRegistry* MlxPlatformStrategies::createBlobRegistry()
{
    return new MlxBlobRegistry;
}

} // namespace WebCore
