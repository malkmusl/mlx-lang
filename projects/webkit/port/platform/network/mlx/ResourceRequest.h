// The Mlx port's ResourceRequest (PORT.md, "Network"): WebCore's generic
// request (ResourceRequestBase) and nothing of a network library's. The
// port's loader strategy turns it into an HTTP request of std.http's on
// the Mlx side; there is no platform request object to keep in step, so
// the ResourceRequestBase hooks for one do nothing.
#pragma once

#include <WebCore/ResourceRequestBase.h>

namespace WebCore {

class ResourceRequest : public ResourceRequestBase {
public:
    explicit ResourceRequest(String&& url)
        : ResourceRequestBase(URL({ }, WTF::move(url)), ResourceRequestCachePolicy::UseProtocolCachePolicy)
    {
    }

    ResourceRequest(URL&& url)
        : ResourceRequestBase(WTF::move(url), ResourceRequestCachePolicy::UseProtocolCachePolicy)
    {
    }

    ResourceRequest(URL&& url, const String& referrer, ResourceRequestCachePolicy policy = ResourceRequestCachePolicy::UseProtocolCachePolicy)
        : ResourceRequestBase(WTF::move(url), policy)
    {
        setHTTPReferrer(referrer);
    }

    ResourceRequest()
        : ResourceRequestBase(URL(), ResourceRequestCachePolicy::UseProtocolCachePolicy)
    {
    }

    ResourceRequest(ResourceRequestBase&& base)
        : ResourceRequestBase(WTF::move(base))
    {
    }

    ResourceRequest(RequestData&& requestData)
        : ResourceRequestBase(WTF::move(requestData))
    {
    }

    // How many redirects the request followed (the loader strategy stops
    // at twenty, as the network ports do).
    void incrementRedirectCount() { m_redirectCount++; }
    uint16_t redirectCount() const { return m_redirectCount; }

    bool encodingRequiresPlatformData() const { return false; }

    // The data a request is made of again in another thread or process.
    static ResourceRequest fromResourceRequestData(RequestData&& data) { return ResourceRequest(WTF::move(data)); }
    RequestData getRequestDataToSerialize() const { return m_requestData; }

private:
    friend class ResourceRequestBase;

    void doUpdatePlatformRequest() { }
    void doUpdateResourceRequest() { }
    void doUpdatePlatformHTTPBody() { }
    void doUpdateResourceHTTPBody() { }

    void doPlatformSetAsIsolatedCopy(const ResourceRequest& other) { m_redirectCount = other.m_redirectCount; }

    uint16_t m_redirectCount { 0 };
};

} // namespace WebCore

namespace WTF {

template<> struct MarkableTraits<WebCore::ResourceRequest> {
    static bool isEmptyValue(const WebCore::ResourceRequest& request) { return request.isNull(); };
    static WebCore::ResourceRequest emptyValue() { return WebCore::ResourceRequest { }; };
};

} // namespace WTF
