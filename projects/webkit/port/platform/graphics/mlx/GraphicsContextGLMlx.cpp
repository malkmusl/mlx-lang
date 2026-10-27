// The Mlx port's GraphicsContextGL: WebGL over ANGLE (libANGLE.a,
// libGLESv2.a, projects/webkit/angle.mlx) on its Vulkan backend, whose
// Vulkan is libmlxvulkan.so (compiler/libc/vulkan: std.vulkan's loader,
// the system's driver, lavapipe on a machine without a GPU). As WPE's
// GraphicsContextGLTextureMapperANGLE (not compiled: it hands the
// compositor a GL texture), a GraphicsContextGLANGLE whose drawing buffer
// is the texture m_texture of the framebuffer m_fbo; the EGL display is
// ANGLE's own on Vulkan without a window system (EGL_PLATFORM_SURFACELESS_
// MESA: DisplayVkOffscreen), a context needs no surface.
//
// On the GPU compositor (PORT.md, "Compositing": PlatformDisplay's shared
// display exists), as WPE's: the context shares its textures with the
// display's sharing context, which every compositor's context shares
// with, and each frame WebGL presents swaps the drawing buffer's texture
// with the compositor's (m_compositorTexture), which the canvas' layer
// shows (CoordinatedPlatformLayerBufferRGB, after a fence): no read back.
//
// On the CPU compositor: at each frame
// WebGL presents (prepareForDisplay), the drawing buffer is read back
// into a raster image (BGRA, premultiplied: the compositor's format),
// flipped to the page's orientation, and handed to
// the canvas' layer as its contents (CoordinatedPlatformLayerBufferNative
// Image, which SkiaCompositingLayer draws). The read happens before
// WebGL clears a drawing buffer it does not preserve (preserveDrawingBuffer
// false): the layer keeps the frame presented.
#include "config.h"

#if ENABLE(WEBGL)
#include "ANGLEHeaders.h"
#include "ANGLEUtilities.h"
#include "CoordinatedPlatformLayerBufferNativeImage.h"
#include "CoordinatedPlatformLayerBufferRGB.h"
#include "GLContext.h"
#include "GLFence.h"
#include "GraphicsContextGLANGLE.h"
#include "GraphicsLayerContentsDisplayDelegateCoordinated.h"
#include "NativeImage.h"
#include "PixelBuffer.h"
#include "PlatformDisplay.h"
#include "PlatformDisplayMlx.h"
#include "TextureMapperFlags.h"
#include <wtf/StdLibExtras.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkColorSpace.h>
#include <skia/core/SkData.h>
#include <skia/core/SkImage.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

namespace WebCore {

class GraphicsContextGLMlx final : public GraphicsContextGLANGLE {
public:
    static RefPtr<GraphicsContextGLMlx> create(GraphicsContextGLAttributes&& attributes)
    {
        auto context = adoptRef(*new GraphicsContextGLMlx(WTF::move(attributes)));
        if (!context->initialize())
            return nullptr;
        // (ANGLE's Vulkan backend says what costs it time, `GPU stall due
        // to ReadPixels`, as debug messages of the performance type, which
        // WebGL writes on the console; its GL backend, other ports', says
        // none. Its errors stay.)
        if (context->makeContextCurrent())
            GL_DebugMessageControlKHR(GL_DEBUG_SOURCE_API_KHR, GL_DEBUG_TYPE_PERFORMANCE_KHR, GL_DONT_CARE, 0, nullptr, GL_FALSE);
        return context;
    }

    RefPtr<GraphicsLayerContentsDisplayDelegate> layerContentsDisplayDelegate() final { return m_layerContentsDisplayDelegate; }
    RefPtr<PixelBuffer> readCompositedResults() final { return readRenderingResultsForPainting(); }
    bool reshapeDrawingBuffer() final;
    void prepareForDisplay() final;

private:
    explicit GraphicsContextGLMlx(GraphicsContextGLAttributes&& attributes)
        : GraphicsContextGLANGLE(WTF::move(attributes))
    {
    }

    bool platformInitializeContext() final;
    bool platformInitialize() final;

    ~GraphicsContextGLMlx()
    {
        if (m_compositorTexture && makeContextCurrent())
            GL_DeleteTextures(1, &m_compositorTexture);
    }

    void swapCompositorTexture();

    RefPtr<GraphicsLayerContentsDisplayDelegate> m_layerContentsDisplayDelegate;
    // (The GPU compositor's: the texture it shows, in the shared group.)
    GCGLuint m_compositorTexture { 0 };
    bool m_sharesWithCompositor { false };
};

// ANGLE's display on Vulkan, initialized once for the process (each
// context its own of it; the GPU compositor's contexts too:
// PlatformDisplayMlx.h). ANGLE's device asked for
// VK_EXT_external_memory_host (its feature supportsExternalMemoryHost,
// which ANGLE turns on by itself for OpenCL only), so the GPU compositor
// can copy its frames into the window's buffer on the device
// (port/angle/vulkan_present.cpp); where the driver has no such
// extension, ANGLE's device cannot be made with it, and the display is
// made without.
void* mlxANGLEDisplay()
{
    static EGLDisplay display = [] {
        const char* hostMemory[] = { "supportsExternalMemoryHost", nullptr };
        for (bool withHostMemory : { true, false }) {
            const EGLAttrib attributes[] = {
                EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE,
                EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE, EGL_PLATFORM_SURFACELESS_MESA,
                withHostMemory ? EGL_FEATURE_OVERRIDES_ENABLED_ANGLE : EGL_NONE, reinterpret_cast<EGLAttrib>(hostMemory),
                EGL_NONE
            };
            EGLDisplay made = EGL_GetPlatformDisplay(EGL_PLATFORM_ANGLE_ANGLE, nullptr, attributes);
            if (made == EGL_NO_DISPLAY)
                continue;
            EGLint major = 0;
            EGLint minor = 0;
            if (EGL_Initialize(made, &major, &minor))
                return made;
            if (!withHostMemory)
                WTFLogAlways("ANGLE: no Vulkan device (EGL error 0x%x)", EGL_GetError());
        }
        return EGL_NO_DISPLAY;
    }();
    return display;
}

// (port/angle/vulkan_device_type.cpp: compiled with vulkan/vulkan.h.)
extern "C" bool mlxVulkanDeviceIsCPU(void* physicalDevice, char* name, size_t);

bool mlxANGLEDeviceIsCPU(const char** name)
{
    static char deviceName[256];
    static bool isCPU = [] {
        auto display = static_cast<EGLDisplay>(mlxANGLEDisplay());
        EGLAttrib device = 0;
        EGLAttrib physicalDevice = 0;
        if (!display || !EGL_QueryDisplayAttribEXT(display, EGL_DEVICE_EXT, &device) || !device
            || !EGL_QueryDeviceAttribEXT(reinterpret_cast<EGLDeviceEXT>(device), EGL_VULKAN_PHYSICAL_DEVICE_ANGLE, &physicalDevice) || !physicalDevice)
            return false;
        return mlxVulkanDeviceIsCPU(reinterpret_cast<void*>(physicalDevice), deviceName, sizeof(deviceName));
    }();
    if (name)
        *name = deviceName;
    return isCPU;
}

GraphicsContextGLANGLE::~GraphicsContextGLANGLE()
{
    if (!makeContextCurrent())
        return;
    GL_Disable(DEBUG_OUTPUT);
    if (m_texture)
        GL_DeleteTextures(1, &m_texture);
    auto attributes = contextAttributes();
    if (attributes.antialias) {
        GL_DeleteRenderbuffers(1, &m_multisampleColorBuffer);
        if (attributes.stencil || attributes.depth)
            GL_DeleteRenderbuffers(1, &m_multisampleDepthStencilBuffer);
        GL_DeleteFramebuffers(1, &m_multisampleFBO);
    } else {
        if (m_depthStencilBuffer)
            GL_DeleteRenderbuffers(1, &m_depthStencilBuffer);
        if (m_preserveDrawingBufferTexture)
            GL_DeleteTextures(1, &m_preserveDrawingBufferTexture);
        if (m_preserveDrawingBufferFBO)
            GL_DeleteFramebuffers(1, &m_preserveDrawingBufferFBO);
    }
    GL_DeleteFramebuffers(1, &m_fbo);
    EGL_MakeCurrent(m_displayObj, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (m_contextObj)
        EGL_DestroyContext(m_displayObj, m_contextObj);
}

bool GraphicsContextGLANGLE::makeContextCurrent()
{
    // WebKit's GLContexts (Skia's, the compositor's, the painting
    // threads') keep which of them is current on the thread
    // (GLContextWrapper) and skip eglMakeCurrent when it is theirs: one
    // made current before WebGL's would be taken for current still, and
    // Skia's calls (a WebGL canvas drawn into a 2D one with the GPU's
    // compositor on) would go to WebGL's context. (Upstream's
    // GraphicsContextGLTextureMapperANGLE is a GLContextWrapper itself.)
    if (auto* current = GLContextWrapper::currentContext())
        current->didUnmakeContextCurrent();
    if (EGL_GetCurrentContext() == m_contextObj)
        return true;
    return EGL_MakeCurrent(m_displayObj, EGL_NO_SURFACE, EGL_NO_SURFACE, m_contextObj);
}

void GraphicsContextGLANGLE::checkGPUStatus()
{
}

void GraphicsContextGLANGLE::platformReleaseThreadResources()
{
}

RefPtr<GraphicsContextGL> createWebProcessGraphicsContextGL(const GraphicsContextGLAttributes& attributes)
{
    return GraphicsContextGLMlx::create(GraphicsContextGLAttributes { attributes });
}

bool GraphicsContextGLMlx::platformInitializeContext()
{
    m_isForWebGL2 = contextAttributes().isWebGL2;
    m_displayObj = static_cast<EGLDisplay>(mlxANGLEDisplay());
    if (m_displayObj == EGL_NO_DISPLAY)
        return false;

    const EGLint configAttributes[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0,
        EGL_STENCIL_SIZE, 0,
        EGL_NONE
    };
    EGLint count = 0;
    EGL_ChooseConfig(m_displayObj, configAttributes, &m_configObj, 1, &count);
    if (count != 1)
        return false;
    EGL_BindAPI(EGL_OPENGL_ES_API);

    Vector<EGLint> contextAttributes;
    contextAttributes.append(EGL_CONTEXT_CLIENT_VERSION);
    contextAttributes.append(m_isForWebGL2 ? 3 : 2);
    if (!m_isForWebGL2) {
        // (ANGLE makes an ES 3 context of an ES 2 one unless told not to.)
        contextAttributes.append(EGL_CONTEXT_OPENGL_BACKWARDS_COMPATIBLE_ANGLE);
        contextAttributes.append(EGL_FALSE);
    }
    contextAttributes.append(EGL_CONTEXT_WEBGL_COMPATIBILITY_ANGLE);
    contextAttributes.append(EGL_TRUE);
    // (WebGL's resources are cleared when made; no client arrays; no
    // objects made by binding a name.)
    contextAttributes.append(EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE);
    contextAttributes.append(EGL_TRUE);
    contextAttributes.append(EGL_CONTEXT_CLIENT_ARRAYS_ENABLED_ANGLE);
    contextAttributes.append(EGL_FALSE);
    contextAttributes.append(EGL_CONTEXT_BIND_GENERATES_RESOURCE_CHROMIUM);
    contextAttributes.append(EGL_FALSE);
    contextAttributes.append(EGL_NONE);
    // (Sharing the compositor's textures when it composes on the GPU.)
    EGLContext sharing = EGL_NO_CONTEXT;
    if (auto* display = PlatformDisplay::sharedDisplayIfExists()) {
        if (auto* context = display->sharingGLContext())
            sharing = context->platformContext();
    }
    m_contextObj = EGL_CreateContext(m_displayObj, m_configObj, sharing, contextAttributes.span().data());
    if (m_contextObj == EGL_NO_CONTEXT && sharing != EGL_NO_CONTEXT) {
        WTFLogAlways("WebGL: no context sharing the compositor's (EGL error 0x%x): read back", EGL_GetError());
        m_contextObj = EGL_CreateContext(m_displayObj, m_configObj, EGL_NO_CONTEXT, contextAttributes.span().data());
    } else
        m_sharesWithCompositor = sharing != EGL_NO_CONTEXT;
    if (m_contextObj == EGL_NO_CONTEXT)
        return false;
    return makeContextCurrent();
}

bool GraphicsContextGLMlx::platformInitialize()
{
    m_layerContentsDisplayDelegate = GraphicsLayerContentsDisplayDelegateCoordinated::create();
    if (m_sharesWithCompositor) {
        ScopedRestoreTextureBinding restoreBinding(TEXTURE_BINDING_2D, TEXTURE_2D);
        GL_GenTextures(1, &m_compositorTexture);
        GL_BindTexture(GL_TEXTURE_2D, m_compositorTexture);
        GL_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        GL_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        GL_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        GL_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    return true;
}

bool GraphicsContextGLMlx::reshapeDrawingBuffer()
{
    auto attributes = contextAttributes();
    auto size = getInternalFramebufferSize();
    GLenum format = attributes.alpha ? GL_RGBA : GL_RGB;
    ScopedRestoreTextureBinding restoreBinding(TEXTURE_BINDING_2D, TEXTURE_2D);
    GL_BindTexture(GL_TEXTURE_2D, m_texture);
    GL_TexImage2D(GL_TEXTURE_2D, 0, format, size.width(), size.height(), 0, format, GL_UNSIGNED_BYTE, nullptr);
    if (m_compositorTexture) {
        GL_BindTexture(GL_TEXTURE_2D, m_compositorTexture);
        GL_TexImage2D(GL_TEXTURE_2D, 0, format, size.width(), size.height(), 0, format, GL_UNSIGNED_BYTE, nullptr);
    }
    return true;
}

// The fence after a frame WebGL presents, which the compositor waits for
// on the GPU before it draws the frame's texture (an EGL fence sync of
// ANGLE's: made, it submits what the context recorded, the open render
// pass too, which glFlush leaves open). WebKit's GLFence::create gives
// none here: its fences are HAVE(GL_FENCE)'s, and a WebGL 1 context has
// no GL sync objects.
class MlxEGLFence final : public GLFence {
public:
    static std::unique_ptr<GLFence> create(EGLDisplay display)
    {
        EGLSync sync = EGL_CreateSync(display, EGL_SYNC_FENCE_KHR, nullptr);
        if (sync == EGL_NO_SYNC)
            return nullptr;
        return std::unique_ptr<GLFence>(new MlxEGLFence(display, sync));
    }

    ~MlxEGLFence() final
    {
        EGL_DestroySync(m_display, m_sync);
    }

private:
    MlxEGLFence(EGLDisplay display, EGLSync sync)
        : m_display(display)
        , m_sync(sync)
    {
    }

    void clientWait() final
    {
        EGL_ClientWaitSync(m_display, m_sync, 0, EGL_FOREVER);
    }

    void serverWait() final
    {
        EGL_WaitSync(m_display, m_sync, 0);
    }

    EGLDisplay m_display;
    EGLSync m_sync;
};

// The drawing buffer's texture and the compositor's swapped (WPE's
// GraphicsContextGLTextureMapperANGLE::swapCompositorTexture): the frame
// presented goes to the compositor, WebGL draws into the other.
void GraphicsContextGLMlx::swapCompositorTexture()
{
    std::swap(m_texture, m_compositorTexture);
    if (m_preserveDrawingBufferTexture) {
        GL_FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_preserveDrawingBufferTexture, 0);
        GL_BindFramebuffer(GL_FRAMEBUFFER, m_preserveDrawingBufferFBO);
        GL_FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0);
        GL_BindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    } else {
        GL_BindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        GL_FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0);
    }
    GL_Flush();
    if (m_state.boundDrawFBO != m_fbo)
        GL_BindFramebuffer(GraphicsContextGL::FRAMEBUFFER, m_state.boundDrawFBO);
}

void GraphicsContextGLMlx::prepareForDisplay()
{
    if (!makeContextCurrent())
        return;
    prepareTexture();
    if (m_compositorTexture) {
        swapCompositorTexture();
        OptionSet<TextureMapperFlags> flags = TextureMapperFlags::ShouldFlipTexture;
        if (contextAttributes().alpha)
            flags.add(TextureMapperFlags::ShouldBlend);
        auto fence = MlxEGLFence::create(m_displayObj);
        m_layerContentsDisplayDelegate->setDisplayBuffer(CoordinatedPlatformLayerBufferRGB::create(m_compositorTexture, getInternalFramebufferSize(), flags, WTF::move(fence)));
        return;
    }
    RefPtr pixels = readRenderingResultsForPainting();
    if (!pixels || pixels->size().isEmpty())
        return;
    // The layer's image in the compositor's own format, BGRA premultiplied
    // (opaque without alpha), so that SkiaCompositingLayer draws it with
    // the port's row blends (SkiaRoutinesMlx.cpp) and not Skia's portable
    // pipeline a pixel at a time (swapping, unpremultiplying: a 300x300
    // canvas took the compositor 90 ms a frame); its rows the page's way
    // up (GL's go up from the bottom).
    auto attributes = contextAttributes();
    int width = pixels->size().width();
    int height = pixels->size().height();
    size_t rowBytes = static_cast<size_t>(width) * 4;
    auto data = SkData::MakeUninitialized(rowBytes * height);
    auto source = pixels->bytes();
    auto* target = static_cast<uint8_t*>(data->writable_data());
    bool premultiply = attributes.alpha && !attributes.premultipliedAlpha;
    for (int row = 0; row < height; ++row) {
        const uint8_t* from = source.data() + (height - 1 - row) * rowBytes;
        uint8_t* to = target + row * rowBytes;
        for (int column = 0; column < width; ++column, from += 4, to += 4) {
            uint8_t alpha = attributes.alpha ? from[3] : 255;
            if (premultiply) {
                to[0] = (from[2] * alpha + 127) / 255;
                to[1] = (from[1] * alpha + 127) / 255;
                to[2] = (from[0] * alpha + 127) / 255;
            } else {
                to[0] = from[2];
                to[1] = from[1];
                to[2] = from[0];
            }
            to[3] = alpha;
        }
    }
    auto info = SkImageInfo::Make(width, height, kBGRA_8888_SkColorType, attributes.alpha ? kPremul_SkAlphaType : kOpaque_SkAlphaType, SkColorSpace::MakeSRGB());
    auto image = SkImages::RasterFromData(info, WTF::move(data), rowBytes);
    if (!image)
        return;
    m_layerContentsDisplayDelegate->setDisplayBuffer(CoordinatedPlatformLayerBufferNativeImage::create(NativeImage::create(WTF::move(image)).releaseNonNull(), nullptr));
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
