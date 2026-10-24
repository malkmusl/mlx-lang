// The Mlx port's PlatformDisplay: ANGLE's surfaceless EGL display on Vulkan
// (PlatformDisplayMlx.h), what WebKit's TextureMapper, BitmapTexturePool and
// GLContext (platform/graphics/egl, texmap: compiled as they are) are given
// when the compositor composes on the GPU (MlxLayerTreeHost). WebKit's
// PlatformDisplay.cpp keeps the shared display; it exists only once the
// GPU compositor asked for it (sharedDisplayIfExists() is null otherwise,
// and nothing is accelerated: ProcessCapabilities says no accelerated
// buffers).
//
// What PlatformDisplaySkia.cpp gives (Ganesh on the display: a GL
// context and a GrDirectContext for each thread that paints) is the
// port's here, since WebKit's makes it only for GTK and WPE: with the GPU
// compositor and MLX_PAINTING=gpu, a layer's tiles are painted by Ganesh
// over ANGLE's GLES into textures (SkiaPaintingEngineMlx.cpp); otherwise
// on the CPU and uploaded.
#include "config.h"
#include "PlatformDisplayMlx.h"

#include "GLContext.h"
#include "GLDisplay.h"
#include "PlatformDisplay.h"
#include <EGL/egl.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/gpu/ganesh/GrDirectContext.h>
#include <skia/gpu/ganesh/gl/GrGLAssembleInterface.h>
#include <skia/gpu/ganesh/gl/GrGLDirectContext.h>
#include <skia/gpu/ganesh/gl/GrGLInterface.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

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

// Ganesh's GL functions: ANGLE's GLES (GLES's assembly: the generic one
// would also take in WebGL's, which Skia is built without), found
// through EGL.
static sk_sp<const GrGLInterface> skiaGLInterface()
{
    static NeverDestroyed<sk_sp<const GrGLInterface>> interface { GrGLMakeAssembledGLESInterface(nullptr, [](void*, const char* name) -> GrGLFuncPtr {
        return reinterpret_cast<GrGLFuncPtr>(eglGetProcAddress(name));
    }) };
    return interface.get();
}

// A thread's GL context for Ganesh (offscreen, sharing the display's
// textures) and its GrDirectContext.
struct MlxSkiaGLContext {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(MlxSkiaGLContext);
public:
    std::unique_ptr<GLContext> glContext;
    sk_sp<GrDirectContext> grContext;
};
static thread_local std::unique_ptr<MlxSkiaGLContext> s_skiaGLContext;

GLContext* PlatformDisplay::skiaGLContext()
{
    if (!s_skiaGLContext) {
        s_skiaGLContext = makeUnique<MlxSkiaGLContext>();
        auto glContext = GLContext::createOffscreen(*this);
        if (glContext && glContext->makeContextCurrent()) {
            GrContextOptions options;
            options.fSharpenMipmappedTextures = false;
            if (auto grContext = GrDirectContexts::MakeGL(skiaGLInterface(), options)) {
                s_skiaGLContext->glContext = WTF::move(glContext);
                s_skiaGLContext->grContext = WTF::move(grContext);
            }
        }
    }
    return s_skiaGLContext->glContext.get();
}

void PlatformDisplay::clearSkiaGLContext()
{
    if (s_skiaGLContext && s_skiaGLContext->glContext && s_skiaGLContext->glContext->makeContextCurrent())
        s_skiaGLContext->grContext = nullptr;
    s_skiaGLContext = nullptr;
}

GrDirectContext* PlatformDisplay::skiaGrContext() const
{
    return s_skiaGLContext ? s_skiaGLContext->grContext.get() : nullptr;
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
