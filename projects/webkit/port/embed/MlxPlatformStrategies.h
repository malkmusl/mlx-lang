// The platform strategies of the Mlx port's single-process embedding
// (PORT.md, "One process first"): what WebKitLegacy's
// WebPlatformStrategies were to a page in the application's process. The
// loader strategy makes a SubresourceLoader of each load, which WebCore
// runs (data: URLs itself, the others through the port's ResourceHandle);
// blobs are kept in the process (BlobRegistryImpl); media has nothing to
// strategize (video and web audio are off); the pasteboard is WebCore's
// Pasteboard of the port (no strategy until the Wayland data device, M5).
//
//     WebCore::MlxPlatformStrategies::initialize();   before the first Page
#pragma once

#include "PlatformStrategies.h"

namespace WebCore {

class MlxPlatformStrategies final : public PlatformStrategies {
public:
    WEBCORE_EXPORT static void initialize();

private:
    MlxPlatformStrategies() = default;

    LoaderStrategy* createLoaderStrategy() override;
    PasteboardStrategy* createPasteboardStrategy() override;
    MediaStrategy* createMediaStrategy() override;
    BlobRegistry* createBlobRegistry() override;
};

} // namespace WebCore
