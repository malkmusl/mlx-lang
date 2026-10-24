// The Mlx port's PlatformDisplay: ANGLE's surfaceless EGL display on Vulkan
// (PlatformDisplayMlx.h), what WebKit's TextureMapper, BitmapTexturePool and
// GLContext (platform/graphics/egl, texmap: compiled as they are) are given
// when the compositor composes on the GPU (MlxLayerTreeHost). WebKit's
// PlatformDisplay.cpp keeps the shared display; it exists only once the
// GPU compositor asked for it (sharedDisplayIfExists() is null otherwise,
// and nothing is accelerated: ProcessCapabilities says no accelerated
// buffers).
//
// What PlatformDisplaySkia.cpp gives (Ganesh on the display: GPU painting
// of images, canvases and tiles) is not taken: the layers' tiles are
// painted on the CPU (SkiaPaintingEngineMlx.cpp) and uploaded into
// textures.
// STUB (Ganesh over ANGLE: tiles and canvases painted on the GPU): no
// Skia GL context, no GrDirectContext.

#include "config.h"
#include "PlatformDisplayMlx.h"

#include "GLContext.h"
#include "GLDisplay.h"
#include "PlatformDisplay.h"
#include <wtf/MainThread.h>

namespace WebCore {

class PlatformDisplayMlx final : public PlatformDisplay {
public:
    explicit PlatformDisplayMlx(Ref<GLDisplay>&& display)
        : PlatformDisplay(WTF::move(display))
    {
    }

private:
    Type type() const final { return Type::Surfaceless; }
};

bool mlxEnsureSharedDisplay()
{
    static bool usable = [] {
        RELEASE_ASSERT(isMainThread());
        if (PlatformDisplay::sharedDisplayIfExists())
            return true;
        auto* angle = mlxANGLEDisplay();
        if (!angle)
            return false;
        RefPtr display = GLDisplay::create(angle);
        if (!display)
            return false;
        PlatformDisplay::setSharedDisplay(makeUnique<PlatformDisplayMlx>(display.releaseNonNull()));
        // (The sharing context: every compositor's context shares its
        // textures, as BitmapTexturePool's are, with it.)
        return !!PlatformDisplay::sharedDisplay().sharingGLContext();
    }();
    return usable;
}

GLContext* PlatformDisplay::skiaGLContext()
{
    return nullptr;
}

void PlatformDisplay::clearSkiaGLContext()
{
}

GrDirectContext* PlatformDisplay::skiaGrContext() const
{
    return nullptr;
}

unsigned PlatformDisplay::msaaSampleCount() const
{
    return 0;
}

size_t PlatformDisplay::maxSkiaResourceCacheBytes() const
{
    return 0;
}

} // namespace WebCore
