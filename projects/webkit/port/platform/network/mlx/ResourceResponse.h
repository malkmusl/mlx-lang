// The Mlx port's ResourceResponse (PORT.md, "Network"): WebCore's generic
// response (ResourceResponseBase). The loader strategy makes it from what
// std.http gives back (status, headers, the MIME type sniffed) through the
// generic setters; it keeps nothing of its own but the suggested file name
// of a download (Content-Disposition, which the generic headers hold).
#pragma once

#include <WebCore/ResourceResponseBase.h>

namespace WebCore {

class ResourceResponse : public ResourceResponseBase {
public:
    ResourceResponse() = default;

    ResourceResponse(URL&& url, String&& mimeType, long long expectedLength, String&& textEncodingName)
        : ResourceResponseBase(WTF::move(url), WTF::move(mimeType), expectedLength, WTF::move(textEncodingName))
    {
    }

    ResourceResponse(ResourceResponseBase&& base)
        : ResourceResponseBase(WTF::move(base))
    {
    }

private:
    friend class ResourceResponseBase;

    void doUpdateResourceResponse() { }
    // ResourceResponseBase calls this name (its own default is spelled
    // platformSuggestedFileName): every port defines it.
    String platformSuggestedFilename() const;
};

} // namespace WebCore
