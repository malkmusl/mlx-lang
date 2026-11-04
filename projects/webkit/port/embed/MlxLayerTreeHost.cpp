// A page's composited layers, on the GPU or the CPU: MlxLayerTreeHost.h says what.
// Beside the host: MlxDisplayLink, the process's frame clock (a thread
// ticking at the display's rate while anything wants frames: the hosts'
// compositors, the scrolling trees' animated scrolls, the pages' rendering
// updates through MlxDisplayRefreshMonitor), and MlxScrollingCoordinator,
// the pages' asynchronous scrolling (WebCore's ThreadedScrollingCoordinator
// over ScrollingTreeCoordinated, as WebKit2's ScrollingCoordinatorCoordinated
// makes it for WPE).
#include "config.h"
#include "MlxLayerTreeHost.h"

#if USE(COORDINATED_GRAPHICS) && USE(SKIA)
#include "AsyncScrollingCoordinator.h"
#include "Chrome.h"
#include "CoordinatedImageBackingStore.h"
#include "Damage.h"
#include "DisplayRefreshMonitor.h"
#include "EventHandler.h"
#include "GLContext.h"
#include "GraphicsLayerCoordinated.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "NativeImage.h"
#include "Page.h"
#include "PlatformDisplay.h"
#include "ScrollableArea.h"
#include "Scrollbar.h"
#include "ScrollbarTheme.h"
#include "ScrollbarsController.h"
#include "ScrollingThread.h"
#include "ScrollingTreeCoordinated.h"
#include "Settings.h"
#include "SkiaCompositingLayer.h"
#include "SkiaPaintingEngine.h"
#include "TextureMapper.h"
#include "TextureMapperGLHeaders.h"
#include <GLES3/gl3.h>
#include "TextureMapperLayer.h"
#include "ThreadedScrollingCoordinator.h"
#include "ThreadedScrollingTree.h"
#include "port/platform/graphics/mlx/MlxDarkMode.h"
#include "port/platform/graphics/mlx/PlatformDisplayMlx.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <skia/core/SkCanvas.h>
#include <skia/core/SkImage.h>
#include <skia/core/SkSurface.h>
#include <skia/gpu/ganesh/GrDirectContext.h>
#include <skia/utils/SkNWayCanvas.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <stdlib.h>
#include <wtf/Condition.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/SetForScope.h>
#include <wtf/text/MakeString.h>
#include <wtf/Threading.h>

// ANGLE's EGL and GL entry points for the direct presentation
// (EGL_ANGLE_device_vulkan, EGL_ANGLE_vulkan_image, GL_ANGLE_vulkan_image)
// and its Vulkan side (port/angle/vulkan_present.cpp).
extern "C" {
EGLBoolean EGLAPIENTRY eglQueryDisplayAttribEXT(EGLDisplay, EGLint, EGLAttrib*);
EGLBoolean EGLAPIENTRY eglQueryDeviceAttribEXT(EGLDeviceEXT, EGLint, EGLAttrib*);
EGLImageKHR EGLAPIENTRY eglCreateImageKHR(EGLDisplay, EGLContext, EGLenum, EGLClientBuffer, const EGLint*);
EGLBoolean EGLAPIENTRY eglDestroyImageKHR(EGLDisplay, EGLImageKHR);
void EGLAPIENTRY eglLockVulkanQueueANGLE(EGLDisplay);
void EGLAPIENTRY eglUnlockVulkanQueueANGLE(EGLDisplay);
void GL_APIENTRY glEGLImageTargetTexture2DOES(GLenum, GLeglImageOES);
void GL_APIENTRY glAcquireTexturesANGLE(GLuint, const GLuint*, const GLenum*);
void GL_APIENTRY glReleaseTexturesANGLE(GLuint, const GLuint*, GLenum*);
void* mlxVulkanPresenterCreate(void* physicalDevice, void* device, void* queue, uint32_t family);
void mlxVulkanPresenterDestroy(void*);
bool mlxVulkanPresenterMakeImage(void*, int width, int height, void** image, const void** info);
bool mlxVulkanPresenterCopy(void*, uint32_t glLayout, uint32_t* layoutAfter, uint32_t* pixels, int width, int height, int stride, void (*lock)(void*), void (*unlock)(void*), void*);
}

namespace WebCore {

// ---------------------------------------------------------------------------
// The frame clock

// Who ticks with the display: FIRED, on the display link's thread,
// answers whether it still wants ticks.
class MlxDisplayLinkObserver {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(MlxDisplayLinkObserver);
public:
    explicit MlxDisplayLinkObserver(Function<bool(MonotonicTime)>&& fired)
        : m_fired(WTF::move(fired))
    {
    }
    bool fire(MonotonicTime now) { return m_fired(now); }

private:
    Function<bool(MonotonicTime)> m_fired;
};

class MlxDisplayLink {
public:
    static MlxDisplayLink& singleton()
    {
        static NeverDestroyed<MlxDisplayLink> link;
        return link;
    }

    // The display's rate: 60 frames a second (MLX_REFRESH_RATE, 1 to 240,
    // sets it) until std.ui.host reports the output's.
    static unsigned framesPerSecond()
    {
        static unsigned rate = [] {
            if (const char* variable = getenv("MLX_REFRESH_RATE")) {
                int value = atoi(variable);
                if (value >= 1 && value <= 240)
                    return static_cast<unsigned>(value);
            }
            return 60u;
        }();
        return rate;
    }

    void add(MlxDisplayLinkObserver& observer)
    {
        Locker locker { m_lock };
        m_observers.add(&observer, 0);
    }

    // (Waits for a tick that is firing OBSERVER to end.)
    void remove(MlxDisplayLinkObserver& observer)
    {
        Locker firing { m_firingLock };
        Locker locker { m_lock };
        m_observers.remove(&observer);
    }

    // OBSERVER wants ticks again.
    void wake(MlxDisplayLinkObserver& observer)
    {
        Locker locker { m_lock };
        auto found = m_observers.find(&observer);
        if (found == m_observers.end())
            return;
        found->value++;
        if (!m_thread) {
            m_thread = Thread::create("MlxDisplayLink"_s, [this] { run(); });
            return;
        }
        m_condition.notifyAll();
    }

private:
    friend class NeverDestroyed<MlxDisplayLink>;
    MlxDisplayLink() = default;

    void run()
    {
        Seconds interval = 1_s / framesPerSecond();
        MonotonicTime next = MonotonicTime::now();
        while (true) {
            Vector<std::pair<MlxDisplayLinkObserver*, unsigned>> due;
            {
                Locker locker { m_lock };
                while (true) {
                    for (auto& entry : m_observers) {
                        if (entry.value)
                            due.append({ entry.key, entry.value });
                    }
                    if (!due.isEmpty())
                        break;
                    m_condition.wait(m_lock);
                    // (Woken from idle: the next tick a frame from now.)
                    next = MonotonicTime::now() + interval;
                }
            }
            // The tick: at the display's cadence, again in step after a
            // tick missed by more than a frame.
            auto now = MonotonicTime::now();
            if (now < next) {
                sleep(next - now);
                now = MonotonicTime::now();
            }
            next = next + interval;
            if (next < now)
                next = now + interval;
            Locker firing { m_firingLock };
            for (auto& [observer, generation] : due) {
                {
                    Locker locker { m_lock };
                    if (!m_observers.contains(observer))
                        continue;
                }
                bool wanted = observer->fire(now);
                if (wanted)
                    continue;
                // (Idle, unless woken while it fired.)
                Locker locker { m_lock };
                auto found = m_observers.find(observer);
                if (found != m_observers.end() && found->value == generation)
                    found->value = 0;
            }
        }
    }

    Lock m_firingLock;
    Lock m_lock;
    Condition m_condition;
    // Each observer and its wake count (0: no ticks wanted).
    HashMap<MlxDisplayLinkObserver*, unsigned> m_observers WTF_GUARDED_BY_LOCK(m_lock);
    RefPtr<Thread> m_thread;
};

// requestAnimationFrame and the rendering update on the frame clock:
// WebCore's RenderingUpdateScheduler asks this monitor for the next
// refresh, which comes on the display link's tick and is delivered on
// the main thread.
class MlxDisplayRefreshMonitor final : public DisplayRefreshMonitor {
public:
    static Ref<MlxDisplayRefreshMonitor> create(PlatformDisplayID displayID)
    {
        return adoptRef(*new MlxDisplayRefreshMonitor(displayID));
    }

    ~MlxDisplayRefreshMonitor()
    {
        MlxDisplayLink::singleton().remove(m_observer);
    }

private:
    explicit MlxDisplayRefreshMonitor(PlatformDisplayID displayID)
        : DisplayRefreshMonitor(displayID)
        , m_observer([this](MonotonicTime) { return tick(); })
    {
        MlxDisplayLink::singleton().add(m_observer);
    }

    bool tick()
    {
        if (!m_running.load())
            return false;
        m_updateIndex = (m_updateIndex + 1) % MlxDisplayLink::framesPerSecond();
        displayLinkFired({ m_updateIndex, MlxDisplayLink::framesPerSecond() });
        return m_running.load();
    }

    bool startNotificationMechanism() final
    {
        m_running = true;
        MlxDisplayLink::singleton().wake(m_observer);
        return true;
    }

    void stopNotificationMechanism() final { m_running = false; }

    std::optional<FramesPerSecond> displayNominalFramesPerSecond() final { return MlxDisplayLink::framesPerSecond(); }

    void dispatchDisplayDidRefresh(const DisplayUpdate& update) final
    {
        RunLoop::mainSingleton().dispatch([protectedThis = Ref { *this }, update] {
            protectedThis->displayDidRefresh(update);
        });
    }

    MlxDisplayLinkObserver m_observer;
    std::atomic<bool> m_running { false };
    unsigned m_updateIndex { 0 };
};

// ---------------------------------------------------------------------------
// Asynchronous scrolling

class MlxScrollingCoordinator final : public ThreadedScrollingCoordinator {
public:
    static Ref<MlxScrollingCoordinator> create(Page& page)
    {
        return adoptRef(*new MlxScrollingCoordinator(page));
    }

private:
    explicit MlxScrollingCoordinator(Page& page)
        : ThreadedScrollingCoordinator(&page)
    {
        setScrollingTree(ScrollingTreeCoordinated::create(*this));
    }

    void didCompletePlatformRenderingUpdate() final
    {
        if (auto* tree = scrollingTree())
            downcast<ScrollingTreeCoordinated>(*tree).didCompletePlatformRenderingUpdate();
    }
};

// The scrollbars of an asynchronously scrolled area (WebKit2's
// ScrollbarsControllerCoordinated, for the port's single process): drawn
// by the scrolling thread (ScrollerCoordinated, the port's
// ScrollerCoordinatedMlx.cpp) instead of into their layers on the main
// thread, so that a scroll paints nothing there; their state (enabled,
// hovered and pressed parts, opacity, colour, direction, width) handed to
// the scrolling tree. Custom and mock scrollbars are still painted. (On
// WebCore's own ScrollbarsController, as the port's other scrollable
// areas: ScrollbarsControllerGeneric, GTK's and WPE's, defines
// ScrollbarsController::create where ScrollbarsController.cpp does for a
// port without PLATFORM().)
class MlxScrollbarsController final : public ScrollbarsController {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(MlxScrollbarsController);
public:
    MlxScrollbarsController(ScrollableArea& area, ScrollingCoordinator* coordinator)
        : ScrollbarsController(area)
        , m_coordinator(ThreadSafeWeakPtr<ScrollingCoordinator>(coordinator))
    {
        if (RefPtr scrollingCoordinator = m_coordinator.get())
            scrollingCoordinator->setScrollbarWidth(area, area.scrollbarWidthStyle());
    }

private:
    bool isScrollbarsControllerCoordinated() const final { return true; }
    bool shouldDrawIntoScrollbarLayer(Scrollbar& scrollbar) const final { return scrollbar.isCustomScrollbar() || scrollbar.isMockScrollbar(); }

    void updateScrollbarEnabledState(Scrollbar& scrollbar) final
    {
        if (RefPtr coordinator = m_coordinator.get())
            coordinator->setScrollbarEnabled(scrollbar);
    }
    void scrollbarLayoutDirectionChanged(UserInterfaceLayoutDirection direction) final
    {
        if (RefPtr coordinator = m_coordinator.get())
            coordinator->setScrollbarLayoutDirection(protect(scrollableArea()), direction);
    }
    void updateScrollbarStyle() final
    {
        auto& theme = ScrollbarTheme::theme();
        if (theme.isMockTheme())
            return;
        // (The styles' thicknesses differ: the frames set again, laid out
        // again.)
        updateScrollbarsThickness();
        protect(scrollableArea())->scrollbarStyleChanged(theme.usesOverlayScrollbars() ? ScrollbarStyle::Overlay : ScrollbarStyle::AlwaysVisible, true);
    }
    void scrollbarOpacityChanged() final
    {
        if (RefPtr coordinator = m_coordinator.get())
            coordinator->setScrollbarOpacity(protect(scrollableArea()));
    }
    void hoveredPartChanged(Scrollbar&) final
    {
        if (RefPtr coordinator = m_coordinator.get())
            coordinator->setHoveredAndPressedScrollbarParts(protect(scrollableArea()));
    }
    void pressedPartChanged(Scrollbar&) final
    {
        if (RefPtr coordinator = m_coordinator.get())
            coordinator->setHoveredAndPressedScrollbarParts(protect(scrollableArea()));
    }
    void scrollbarColorChanged(std::optional<ScrollbarColor> color) final
    {
        if (RefPtr coordinator = m_coordinator.get())
            coordinator->setScrollbarColor(protect(scrollableArea()), color);
    }

    ThreadSafeWeakPtr<ScrollingCoordinator> m_coordinator;
};

void MlxLayerTreeHost::ensureScrollbarsController(Page& page, ScrollableArea& area)
{
    auto* current = area.existingScrollbarsController();
    if (area.usesCompositedScrolling()) {
        if (!current || !current->isScrollbarsControllerCoordinated())
            area.setScrollbarsController(makeUnique<MlxScrollbarsController>(area, page.scrollingCoordinator()));
    } else if (!current || current->isScrollbarsControllerCoordinated())
        area.setScrollbarsController(ScrollbarsController::create(area));
}

// ---------------------------------------------------------------------------
// The GPU's compositor (ThreadedCompositor's TextureMapper path, WPE's)

static std::atomic<bool> s_gpuCompositingAllowed { true };

void MlxLayerTreeHost::setGPUCompositingAllowed(bool allowed)
{
    s_gpuCompositingAllowed = allowed;
}

// Whether a host is to compose on the GPU, and why (compositingReason):
// MLX_COMPOSITING=cpu, or the embedder's setGPUCompositingAllowed(false),
// the CPU; without a Vulkan device (no ANGLE display), the CPU; otherwise
// the GPU, on any Vulkan device, software ones (lavapipe, llvmpipe,
// SwiftShader) included: the reason names the device and its type.
static bool gpuCompositingWanted(String& reason)
{
    const char* mode = getenv("MLX_COMPOSITING");
    if (mode && !strcmp(mode, "cpu")) {
        reason = "MLX_COMPOSITING=cpu"_s;
        return false;
    }
    if (!s_gpuCompositingAllowed.load()) {
        reason = "the embedder's choice"_s;
        return false;
    }
    if (!mlxANGLEDisplay()) {
        reason = "no Vulkan device"_s;
        return false;
    }
    const char* device = "";
    bool software = mlxANGLEDeviceIsCPU(&device);
    reason = makeString("Vulkan device "_s, String::fromLatin1(device), software ? " (a CPU's)"_s : ""_s);
    return true;
}

// The compositor thread's GL context (ANGLE's, surfaceless, sharing its
// textures with the display's sharing context as every compositor's
// does: BitmapTexturePool's are the process's), WebKit's TextureMapper on
// it, and the framebuffer the frames are composed in: a colour texture of
// the view's size, with depth and stencil (TextureMapper's clips and 3D).
// Frames are composed with their Y axis flipped, so the framebuffer's rows
// read back top down, as the raster frame's.
struct MlxLayerTreeHost::GPUCompositor {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(GPUCompositor);
public:
    std::unique_ptr<GLContext> context;
    std::unique_ptr<TextureMapper> textureMapper;
    GLuint framebuffer { 0 };
    GLuint color { 0 };
    GLuint depthStencil { 0 };
    IntSize size;
    Vector<uint32_t> rows;
    // The direct presentation (port/angle/vulkan_present.cpp): the frame a
    // VkImage of the port's on ANGLE's device, copied on ANGLE's queue into
    // the window's buffer imported as host memory. Null when the device
    // cannot (or MLX_COMPOSITOR_PRESENT=read).
    void* presenter { nullptr };
    EGLDisplay eglDisplay { EGL_NO_DISPLAY };
    EGLImageKHR presentImage { EGL_NO_IMAGE_KHR };

    static std::unique_ptr<GPUCompositor> create(bool present)
    {
        auto& display = PlatformDisplay::sharedDisplay();
        auto context = GLContext::create(display.glDisplay(), GLContext::Target::Surfaceless, display.sharingGLContext());
        if (!context || !context->makeContextCurrent())
            return nullptr;
        auto compositor = makeUnique<GPUCompositor>();
        compositor->context = WTF::move(context);
        compositor->textureMapper = TextureMapper::create();
        // (Reads asynchronous on GLES 3, as ANGLE's contexts are, unless
        // MLX_COMPOSITOR_READ=sync.)
        auto* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        const char* readMode = getenv("MLX_COMPOSITOR_READ");
        compositor->asynchronous = version && strstr(version, "OpenGL ES 3") && !(readMode && !strcmp(readMode, "sync"));
        // (Presented directly on a GPU, unless MLX_COMPOSITOR_PRESENT=read;
        // on a CPU's device only with MLX_COMPOSITOR_PRESENT=gpu: there
        // the copy is the processor's too, and the take waits for the
        // frame's drawing, which the asynchronous read leaves to the
        // device's threads: on lavapipe a frame of the animation page took
        // 5.6 ms of the main thread presented, 2.4 ms read back.)
        const char* presentMode = getenv("MLX_COMPOSITOR_PRESENT");
        bool presentWanted = presentMode ? !strcmp(presentMode, "gpu") : !mlxANGLEDeviceIsCPU();
        if (present && presentWanted)
            compositor->startPresenter(display.eglDisplay());
        else if (present && getenv("MLX_TRACE_COMPOSITOR"))
            fprintf(stderr, "compositor: frames read back (%s)\n", presentMode ? "MLX_COMPOSITOR_PRESENT" : "a CPU's Vulkan device: MLX_COMPOSITOR_PRESENT=gpu presents directly");
        if (getenv("MLX_TRACE_COMPOSITOR"))
            fprintf(stderr, "compositor on the GPU: %s (%s, %s)\n", reinterpret_cast<const char*>(glGetString(GL_RENDERER)), version ? version : "", compositor->presenter ? "frames copied into the window's buffer on the GPU" : compositor->asynchronous ? "asynchronous reads" : "synchronous reads");
        return compositor;
    }

    // The presenter on ANGLE's VkDevice (its handles from EGL's device),
    // when ANGLE has EGL_ANGLE_vulkan_image and GL_ANGLE_vulkan_image and
    // the device imports host memory.
    void startPresenter(EGLDisplay display)
    {
        const char* reason = startPresenterOn(display);
        if (getenv("MLX_TRACE_COMPOSITOR") && reason)
            fprintf(stderr, "compositor: frames read back (%s)\n", reason);
    }

    // Why there is no presenter, or null.
    const char* startPresenterOn(EGLDisplay display)
    {
        const char* eglExtensions = eglQueryString(display, EGL_EXTENSIONS);
        const char* glExtensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
        if (!eglExtensions || !strstr(eglExtensions, "EGL_ANGLE_vulkan_image"))
            return "no EGL_ANGLE_vulkan_image";
        if (!glExtensions || !strstr(glExtensions, "GL_ANGLE_vulkan_image"))
            return "no GL_ANGLE_vulkan_image";
        EGLAttrib device = 0;
        EGLAttrib physicalDevice = 0;
        EGLAttrib vulkanDevice = 0;
        EGLAttrib queue = 0;
        EGLAttrib family = 0;
        if (!eglQueryDisplayAttribEXT(display, EGL_DEVICE_EXT, &device) || !device)
            return "no EGL device";
        auto eglDevice = reinterpret_cast<EGLDeviceEXT>(device);
        if (!eglQueryDeviceAttribEXT(eglDevice, EGL_VULKAN_PHYSICAL_DEVICE_ANGLE, &physicalDevice) || !eglQueryDeviceAttribEXT(eglDevice, EGL_VULKAN_DEVICE_ANGLE, &vulkanDevice)
            || !eglQueryDeviceAttribEXT(eglDevice, EGL_VULKAN_QUEUE_ANGLE, &queue) || !eglQueryDeviceAttribEXT(eglDevice, EGL_VULKAN_QUEUE_FAMILIY_INDEX_ANGLE, &family))
            return "no Vulkan handles of ANGLE's EGL device";
        presenter = mlxVulkanPresenterCreate(reinterpret_cast<void*>(physicalDevice), reinterpret_cast<void*>(vulkanDevice), reinterpret_cast<void*>(queue), static_cast<uint32_t>(family));
        if (!presenter)
            return "the Vulkan device imports no host memory";
        eglDisplay = display;
        return nullptr;
    }

    void stopPresenter()
    {
        if (presentImage != EGL_NO_IMAGE_KHR)
            eglDestroyImageKHR(eglDisplay, presentImage);
        presentImage = EGL_NO_IMAGE_KHR;
        mlxVulkanPresenterDestroy(presenter);
        presenter = nullptr;
    }

    // The frame's WIDTH x HEIGHT top left into PIXELS (STRIDE words a row:
    // the window's buffer) by the GPU; false when it cannot (the caller
    // reads it instead).
    bool present(uint32_t* pixels, int width, int height, int stride)
    {
        if (!presenter || presentImage == EGL_NO_IMAGE_KHR || !color)
            return false;
        static bool trace = getenv("MLX_TRACE_COMPOSITOR");
        auto t0 = MonotonicTime::now();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        GLenum layout = GL_NONE;
        glReleaseTexturesANGLE(1, &color, &layout);
        auto t1 = MonotonicTime::now();
        uint32_t after = layout;
        auto lock = [](void* display) { eglLockVulkanQueueANGLE(static_cast<EGLDisplay>(display)); };
        auto unlock = [](void* display) { eglUnlockVulkanQueueANGLE(static_cast<EGLDisplay>(display)); };
        bool copied = mlxVulkanPresenterCopy(presenter, layout, &after, pixels, std::min(width, size.width()), std::min(height, size.height()), stride, lock, unlock, eglDisplay);
        auto t2 = MonotonicTime::now();
        GLenum acquired = after;
        glAcquireTexturesANGLE(1, &color, &acquired);
        if (trace)
            fprintf(stderr, "compositor present: release %.1f copy %.1f acquire %.1f ms\n", (t1 - t0).milliseconds(), (t2 - t1).milliseconds(), (MonotonicTime::now() - t2).milliseconds());
        return copied;
    }

    ~GPUCompositor()
    {
        if (!context->makeContextCurrent())
            return;
        textureMapper = nullptr;
        deleteTarget();
        if (presenter)
            stopPresenter();
        for (auto& read : reads) {
            if (read.fence)
                glDeleteSync(read.fence);
            if (read.buffer)
                glDeleteBuffers(1, &read.buffer);
        }
    }

    void deleteTarget()
    {
        if (framebuffer)
            glDeleteFramebuffers(1, &framebuffer);
        if (color)
            glDeleteTextures(1, &color);
        if (depthStencil)
            glDeleteRenderbuffers(1, &depthStencil);
        if (presentImage != EGL_NO_IMAGE_KHR) {
            eglDestroyImageKHR(eglDisplay, presentImage);
            // (ANGLE's work on the image done before the presenter frees
            // it: a tab closed freed it under lavapipe's draws of it.)
            glFinish();
        }
        presentImage = EGL_NO_IMAGE_KHR;
        framebuffer = color = depthStencil = 0;
        size = { };
    }

    // The framebuffer of WANTED's size, bound; true when made anew (what
    // it holds is not the last frame).
    bool ensureTarget(const IntSize& wanted)
    {
        if (framebuffer && wanted == size) {
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
            return false;
        }
        deleteTarget();
        size = wanted;
        glGenTextures(1, &color);
        glBindTexture(GL_TEXTURE_2D, color);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        if (!makePresentImage())
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.width(), size.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);
        glGenRenderbuffers(1, &depthStencil);
        glBindRenderbuffer(GL_RENDERBUFFER, depthStencil);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, size.width(), size.height());
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthStencil);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencil);
        return true;
    }

    // The bound texture's storage the presenter's image (BGRA, the window's
    // words), when there is a presenter; without one, or when ANGLE does
    // not take the image, false (and no presenter from then on).
    bool makePresentImage()
    {
        if (!presenter)
            return false;
        void* image = nullptr;
        const void* info = nullptr;
        if (mlxVulkanPresenterMakeImage(presenter, size.width(), size.height(), &image, &info)) {
            auto address = reinterpret_cast<uint64_t>(info);
            const EGLint attributes[] = {
                EGL_VULKAN_IMAGE_CREATE_INFO_HI_ANGLE, static_cast<EGLint>(address >> 32),
                EGL_VULKAN_IMAGE_CREATE_INFO_LO_ANGLE, static_cast<EGLint>(address & 0xFFFFFFFF),
                EGL_NONE
            };
            presentImage = eglCreateImageKHR(eglDisplay, EGL_NO_CONTEXT, EGL_VULKAN_IMAGE_ANGLE, reinterpret_cast<EGLClientBuffer>(&image), attributes);
            if (presentImage != EGL_NO_IMAGE_KHR) {
                glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, presentImage);
                if (glGetError() == GL_NO_ERROR)
                    return true;
                eglDestroyImageKHR(eglDisplay, presentImage);
                presentImage = EGL_NO_IMAGE_KHR;
            }
        }
        if (getenv("MLX_TRACE_COMPOSITOR"))
            fprintf(stderr, "compositor: ANGLE takes no image of the presenter's: frames read back\n");
        stopPresenter();
        return false;
    }

    // Asynchronous reads (GLES 3's pixel pack buffers and fences: ANGLE's
    // contexts are 3.x, its Vulkan backend copying the framebuffer into a
    // buffer on the GPU's queue): a frame's changed rectangle read into a
    // buffer, a fence after it, and the pixels taken once the fence has
    // signalled, while the compositor thread goes on (the next frame, or
    // nothing); in the order they were made, at most three at once.
    struct Read {
        GLuint buffer { 0 };
        size_t capacity { 0 };
        GLsync fence { nullptr };
        IntRect rect;
        IntSize frameSize;
        bool full { false };
        Vector<IntRect> damage;
    };
    static constexpr unsigned maximumReads = 3;
    std::array<Read, maximumReads> reads;
    unsigned firstRead { 0 };
    unsigned readCount { 0 };
    bool asynchronous { false };

    bool readsPending() const { return readCount; }

    // Starts reading RECT of the frame (FRAME_SIZE) composed last;
    // false when a read is still to be taken first (all buffers busy).
    bool startRead(const IntRect& wanted, const IntSize& frameSize, bool full, Vector<IntRect>&& damage)
    {
        if (readCount == maximumReads)
            return false;
        auto& read = reads[(firstRead + readCount) % maximumReads];
        read.rect = intersection(wanted, IntRect({ }, size));
        read.frameSize = frameSize;
        read.full = full;
        read.damage = WTF::move(damage);
        size_t bytes = static_cast<size_t>(read.rect.width()) * read.rect.height() * 4;
        if (!read.buffer)
            glGenBuffers(1, &read.buffer);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, read.buffer);
        if (bytes > read.capacity) {
            read.capacity = bytes;
            glBufferData(GL_PIXEL_PACK_BUFFER, bytes, nullptr, GL_STREAM_READ);
        }
        if (bytes) {
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
            glPixelStorei(GL_PACK_ALIGNMENT, 4);
            glReadPixels(read.rect.x(), read.rect.y(), read.rect.width(), read.rect.height(), GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        read.fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        glFlush();
        ++readCount;
        return true;
    }

    // The oldest read, when its fence has signalled (WAIT: waiting for it),
    // handed to TAKE (the read, its pixels: RGBA rows of its width) and
    // its buffer free again; false when there is none or it is not done.
    template<typename Take>
    bool finishRead(bool wait, const Take& take)
    {
        if (!readCount)
            return false;
        auto& read = reads[firstRead];
        GLenum status = glClientWaitSync(read.fence, wait ? GL_SYNC_FLUSH_COMMANDS_BIT : 0, wait ? 1000000000 : 0);
        if (status != GL_ALREADY_SIGNALED && status != GL_CONDITION_SATISFIED && !(wait && status == GL_WAIT_FAILED))
            return false;
        glDeleteSync(read.fence);
        read.fence = nullptr;
        size_t bytes = static_cast<size_t>(read.rect.width()) * read.rect.height() * 4;
        const uint32_t* pixels = nullptr;
        if (bytes) {
            glBindBuffer(GL_PIXEL_PACK_BUFFER, read.buffer);
            pixels = static_cast<const uint32_t*>(glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, bytes, GL_MAP_READ_BIT));
        }
        take(read, pixels);
        if (bytes) {
            if (pixels)
                glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        }
        firstRead = (firstRead + 1) % maximumReads;
        --readCount;
        return true;
    }

    // RGBA rows (SOURCE, WIDTH words a row) into PIXELS (STRIDE words a
    // row, BGRA as Skia's N32) at RECT.
    static void copySwapped(const uint32_t* source, const IntRect& rect, uint32_t* pixels, int stride)
    {
        size_t width = rect.width();
        for (int y = 0; y < rect.height(); ++y) {
            uint32_t* row = pixels + static_cast<size_t>(rect.y() + y) * stride + rect.x();
            const uint32_t* from = source + y * width;
            for (size_t x = 0; x < width; ++x) {
                uint32_t pixel = from[x];
                row[x] = (pixel & 0xFF00FF00) | ((pixel & 0xFF) << 16) | ((pixel >> 16) & 0xFF);
            }
        }
    }

    // RECT of the framebuffer into PIXELS (STRIDE words a row, BGRA as
    // Skia's N32), one read for the rectangle. (Read as RGBA, the
    // framebuffer's own order, and swapped here: ANGLE's Vulkan backend
    // converts a BGRA read on the GPU, which on lavapipe took 10 ms of a
    // 300x300 read, the swap a fraction of one.)
    void read(const IntRect& wanted, uint32_t* pixels, int stride)
    {
        IntRect rect = intersection(wanted, IntRect({ }, size));
        if (rect.isEmpty() || !framebuffer)
            return;
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        size_t width = rect.width();
        rows.resize(width * rect.height());
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        glReadPixels(rect.x(), rect.y(), rect.width(), rect.height(), GL_RGBA, GL_UNSIGNED_BYTE, rows.mutableSpan().data());
        copySwapped(rows.span().data(), rect, pixels, stride);
    }
};

// ---------------------------------------------------------------------------
// The host

MlxLayerTreeHost::MlxLayerTreeHost(Page& page, Mode mode)
    : m_page(page)
    , m_mode(mode)
    , m_rootLayer(CoordinatedPlatformLayer::create())
    , m_paintingEngine(SkiaPaintingEngine::create(nullptr))
    , m_alive(std::make_shared<std::atomic<bool>>(true))
{
    // (Damage propagated: each frame composes only what changed.)
    m_rootLayer->setDamagePropagationEnabled(true);
    {
        Locker locker { m_rootLayer->lock() };
        m_rootLayer->setAnchorPoint(FloatPoint3D(0, 0, 0));
    }
    // On the GPU: the compositor's thread (in both modes) owns its GL
    // context; without one, the CPU's compositor.
    bool gpu = gpuCompositingWanted(m_compositingReason);
    if (gpu && !mlxEnsureSharedDisplay()) {
        gpu = false;
        m_compositingReason = "no GL context on ANGLE's display"_s;
    }
    if (m_mode == Mode::Threaded || gpu)
        m_compositorQueue = WorkQueue::create("MlxCompositor"_s);
    if (gpu) {
        m_compositorQueue->dispatchSync([this] {
            m_gpu = GPUCompositor::create(m_mode == Mode::Threaded);
        });
        m_onGPU = !!m_gpu;
        m_presentsDirectly = m_onGPU && m_mode == Mode::Threaded && m_gpu->presenter;
        m_asynchronousReads = m_onGPU && m_mode == Mode::Threaded && m_gpu->asynchronous && !m_presentsDirectly;
        if (!m_onGPU)
            m_compositingReason = "no GL context on ANGLE's display"_s;
        if (!m_onGPU && m_mode == Mode::Synchronous)
            m_compositorQueue = nullptr;
    }
    // (On the GPU the tiles are painted there too, by Ganesh over ANGLE's
    // GLES, unless MLX_PAINTING=cpu: the painting engine has the main
    // thread's Skia context's proxy.)
    if (m_onGPU) {
        auto& display = PlatformDisplay::sharedDisplay();
        bool onGPU = mlxGPUPaintingWanted() && display.skiaGLContext() && display.skiaGrContext();
        if (onGPU)
            m_paintingEngine = SkiaPaintingEngine::create(display.skiaGrContext()->threadSafeProxy());
        if (getenv("MLX_TRACE_COMPOSITOR"))
            fprintf(stderr, "compositor's tiles painted on the %s\n", onGPU ? "GPU" : "CPU");
    }
    if (getenv("MLX_TRACE_COMPOSITOR"))
        fprintf(stderr, "compositor on the %s: %s\n", m_onGPU ? "GPU" : "CPU", m_compositingReason.utf8().data());
    if (m_onGPU) {
        m_globalDamage = std::make_shared<Damage>(IntSize(1, 1));
        m_rootLayer->setDamageInGlobalCoordinateSpace(m_globalDamage);
    }
    if (m_mode == Mode::Threaded) {
        m_displayLinkObserver = makeUnique<MlxDisplayLinkObserver>([this](MonotonicTime now) { return displayLinkFired(now); });
        MlxDisplayLink::singleton().add(*m_displayLinkObserver);
    }
}

MlxLayerTreeHost::~MlxLayerTreeHost()
{
    invalidate();
}

void MlxLayerTreeHost::configure(Settings& settings)
{
    settings.setAcceleratedCompositingEnabled(true);
    settings.setForceCompositingMode(true);
    settings.setAcceleratedCompositingForFixedPositionEnabled(true);
    settings.setAsyncFrameScrollingEnabled(true);
    settings.setAsyncOverflowScrollingEnabled(true);
    settings.setPropagateDamagingInformation(true);
    settings.setUseDamagingInformationForCompositing(true);
}

MlxLayerTreeHost& MlxLayerTreeHost::of(const GraphicsLayerCoordinated& layer)
{
    return static_cast<MlxLayerTreeHost&>(layer.coordinatedPlatformLayer().client());
}

void MlxLayerTreeHost::invalidate()
{
    if (m_invalidated)
        return;
    m_invalidated = true;
    *m_alive = false;
    if (m_displayLinkObserver)
        MlxDisplayLink::singleton().remove(*m_displayLinkObserver);
    {
        Locker locker { m_stateLock };
        m_reasons = { };
        m_scrollingTree = nullptr;
    }
    auto letGo = [this] {
        // (A target's textures go with its GL context current.)
        if (m_gpu)
            m_gpu->context->makeContextCurrent();
        commitScene();
        m_rootLayer->invalidateTarget();
        for (auto& layer : std::exchange(m_committedLayers, { }))
            layer->invalidateTarget();
        m_gpu = nullptr;
    };
    if (m_compositorQueue)
        m_compositorQueue->dispatchSync(WTF::move(letGo));
    else
        letGo();
    HashSet<Ref<CoordinatedPlatformLayer>> layers;
    {
        Locker locker { m_layersLock };
        layers = WTF::move(m_layers);
    }
    for (auto& layer : layers)
        layer->invalidateClient();
    {
        Locker locker { m_pendingLayersLock };
        m_pendingLayers = { };
        m_pendingLayersToRemove = { };
    }
    m_imageBackingStores.clear();
}

Ref<GraphicsLayer> MlxLayerTreeHost::createGraphicsLayer(GraphicsLayer::Type type, GraphicsLayerClient& client)
{
    return adoptRef(*new GraphicsLayerCoordinated(type, client, CoordinatedPlatformLayer::create(*this)));
}

void MlxLayerTreeHost::attachLayer(CoordinatedPlatformLayer& layer)
{
    layer.setDamagePropagationEnabled(true);
    if (m_globalDamage)
        layer.setDamageInGlobalCoordinateSpace(m_globalDamage);
    {
        Locker locker { m_layersLock };
        m_layers.add(layer);
    }
    m_didChangeLayers = true;
}

void MlxLayerTreeHost::detachLayer(CoordinatedPlatformLayer& layer)
{
    {
        Locker locker { m_layersLock };
        m_layers.remove(layer);
    }
    m_layersToRemove.add(layer);
    m_didChangeLayers = true;
}

Ref<CoordinatedImageBackingStore> MlxLayerTreeHost::imageBackingStore(Ref<NativeImage>&& image)
{
    auto identifier = image->uniqueID();
    return m_imageBackingStores.ensure(identifier, [&] {
        return CoordinatedImageBackingStore::create(WTF::move(image));
    }).iterator->value;
}

void MlxLayerTreeHost::notifyCompositionRequired()
{
    if (ScrollingThread::isCurrentThread()) {
        m_compositionRequiredInScrollingThread = true;
        return;
    }
    m_compositionRequired = true;
}

bool MlxLayerTreeHost::isCompositionRequiredOrOngoing() const
{
    if (m_compositionRequired)
        return true;
    Locker locker { m_stateLock };
    return !m_reasons.isEmpty() || m_frameScheduled || m_frameInProgress;
}

void MlxLayerTreeHost::requestComposition(CompositionReason reason)
{
    if (ScrollingThread::isCurrentThread()) {
        // (The scrolling thread moved layers: their positions handed over
        // now, without the main thread.)
        flushPendingState();
        if (!std::exchange(m_compositionRequiredInScrollingThread, false))
            return;
    }
    Locker locker { m_stateLock };
    m_reasons.add(reason);
    scheduleFrameLocked();
}

RunLoop* MlxLayerTreeHost::compositingRunLoop() const
{
    return m_compositorQueue ? &m_compositorQueue->runLoop() : &RunLoop::mainSingleton();
}

void MlxLayerTreeHost::didPaintTile()
{
    if (--m_pendingTiles)
        return;
    Locker locker { m_stateLock };
    if (!std::exchange(m_waitingForTiles, false))
        return;
    scheduleFrameLocked();
}

RefPtr<DisplayRefreshMonitor> MlxLayerTreeHost::createDisplayRefreshMonitor(PlatformDisplayID displayID)
{
    return MlxDisplayRefreshMonitor::create(displayID);
}

RefPtr<ScrollingCoordinator> MlxLayerTreeHost::createScrollingCoordinator(Page& page)
{
    return MlxScrollingCoordinator::create(page);
}

// The page's scrolling tree, kept for the frame clock's thread.
RefPtr<ScrollingTree> MlxLayerTreeHost::mainThreadScrollingTree()
{
    Locker locker { m_stateLock };
    if (!m_scrollingTree && !m_invalidated) {
        RefPtr page = m_page.get();
        RefPtr coordinator = page ? dynamicDowncast<AsyncScrollingCoordinator>(page->scrollingCoordinator()) : nullptr;
        if (coordinator)
            m_scrollingTree = coordinator->scrollingTree();
    }
    return m_scrollingTree;
}

void MlxLayerTreeHost::attachRootGraphicsLayer(GraphicsLayer* layer)
{
    if (m_rootCompositingLayer == layer)
        return;
    m_rootCompositingLayer = layer;
    updateRootChildren();
}

void MlxLayerTreeHost::attachViewOverlayGraphicsLayer(GraphicsLayer* layer)
{
    if (m_overlayCompositingLayer == layer)
        return;
    m_overlayCompositingLayer = layer;
    updateRootChildren();
}

void MlxLayerTreeHost::updateRootChildren()
{
    Vector<Ref<CoordinatedPlatformLayer>> children;
    if (m_rootCompositingLayer) {
        children.append(downcast<GraphicsLayerCoordinated>(*m_rootCompositingLayer).coordinatedPlatformLayer());
        if (m_overlayCompositingLayer)
            children.append(downcast<GraphicsLayerCoordinated>(*m_overlayCompositingLayer).coordinatedPlatformLayer());
    }
    {
        Locker locker { m_rootLayer->lock() };
        m_rootLayer->setChildren(WTF::move(children));
    }
    m_didChangeLayers = true;
    if (RefPtr page = m_page.get())
        page->scheduleRenderingUpdate({ });
}

void MlxLayerTreeHost::setSize(const IntSize& size)
{
    if (size == m_size)
        return;
    {
        Locker locker { m_frameLock };
        m_size = size;
    }
    m_pendingResize = true;
    if (RefPtr page = m_page.get())
        page->scheduleRenderingUpdate({ });
}

void MlxLayerTreeHost::setVisible(bool visible)
{
    if (m_visible.exchange(visible) == visible || !visible)
        return;
    // (Shown again: the whole frame composed afresh.)
    {
        Locker locker { m_frameLock };
        m_frameValid = false;
    }
    Locker locker { m_stateLock };
    m_reasons.add(CompositionReason::RenderingUpdate);
    scheduleFrameLocked();
}

void MlxLayerTreeHost::setForcedDark(bool dark)
{
    if (m_forcedDark.exchange(dark) == dark)
        return;
    // (Every layer's contents painted again, in the new colours.)
    if (RefPtr page = m_page.get()) {
        page->forceRepaintAllFrames();
        page->scheduleRenderingUpdate({ });
    }
    Locker locker { m_frameLock };
    m_frameValid = false;
}

// ---------------------------------------------------------------------------
// The rendering update (main thread)

void MlxLayerTreeHost::triggerRenderingUpdate()
{
    if (m_isWaitingForRenderer) {
        m_scheduledWhileWaitingForRenderer = true;
        return;
    }
    updateRendering();
}

void MlxLayerTreeHost::updateRendering()
{
    RefPtr page = m_page.get();
    if (!page || m_invalidated || m_isUpdatingRendering)
        return;
    SetForScope updating(m_isUpdatingRendering, true);
    mainThreadScrollingTree();
    page->updateRendering();
    if (m_overlayCompositingLayer) {
        FloatRect visible { { }, FloatSize(m_size) };
        m_overlayCompositingLayer->flushCompositingState(visible);
    }
    page->finalizeRenderingUpdate({ FinalizeRenderingUpdateFlags::ApplyScrollingTreeLayerPositions });
    if (m_pendingResize) {
        Locker locker { m_rootLayer->lock() };
        m_rootLayer->setSize(FloatSize(m_size));
    }
    bool changed = flushScene();
    if (m_compositionRequired || m_pendingResize || changed)
        requestCompositionForRenderingUpdate();
    m_compositionRequired = false;
    m_pendingResize = false;
    page->didUpdateRendering();
    // (Images no layer shows any more let go.)
    m_imageBackingStores.removeIf([](auto& entry) {
        return entry.value->hasOneRef();
    });
}

// The scene handed to the compositor (CoordinatedSceneState::flush): the
// layers made and gone since, every layer's state.
bool MlxLayerTreeHost::flushScene()
{
    bool changed = m_didChangeLayers.exchange(false);
    if (changed) {
        Locker pending { m_pendingLayersLock };
        {
            Locker locker { m_layersLock };
            m_pendingLayers = m_layers;
        }
        m_pendingLayersToRemove.addAll(std::exchange(m_layersToRemove, { }));
    }
    flushPendingState();
    return changed;
}

void MlxLayerTreeHost::flushPendingState()
{
    Locker scene { m_sceneStateLock };
    Locker locker { m_layersLock };
    for (auto& layer : m_layers)
        Ref { layer }->flushPendingState();
}

void MlxLayerTreeHost::requestCompositionForRenderingUpdate()
{
    if (m_mode == Mode::Synchronous) {
        Locker locker { m_stateLock };
        m_reasons.add(CompositionReason::RenderingUpdate);
        return;
    }
    m_isWaitingForRenderer = true;
    Locker locker { m_stateLock };
    m_reasons.add(CompositionReason::RenderingUpdate);
    m_notifyRenderingUpdate = true;
    if (m_pendingTiles.load())
        m_waitingForTiles = true;
    // (Composed at once, as WebKit2's ThreadedCompositor composes a
    // rendering update when it is committed: left to the clock's next tick,
    // an update and its frame took two ticks at least, and a page changing
    // each frame, a WebGL canvas drawn from requestAnimationFrame, ran at
    // a quarter of the display's rate.)
    if (!m_waitingForTiles && m_visible.load() && !m_frameScheduled && !m_frameInProgress) {
        m_frameScheduled = true;
        m_compositorQueue->dispatch([this] { renderFrame(); });
    }
    scheduleFrameLocked();
}

// The frame of a rendering update is shown: the next update may run.
void MlxLayerTreeHost::didCompositeRenderingUpdate()
{
    RefPtr page = m_page.get();
    if (!page || m_invalidated)
        return;
    m_isWaitingForRenderer = false;
    page->didCompleteRenderingUpdateDisplay();
    if (std::exchange(m_scheduledWhileWaitingForRenderer, false))
        page->scheduleRenderingUpdate({ });
}

// ---------------------------------------------------------------------------
// The compositor

void MlxLayerTreeHost::scheduleFrameLocked()
{
    if (m_displayLinkObserver)
        MlxDisplayLink::singleton().wake(*m_displayLinkObserver);
}

bool MlxLayerTreeHost::displayLinkFired(MonotonicTime)
{
    RefPtr<ScrollingTree> tree;
    bool wanted = false;
    {
        Locker locker { m_stateLock };
        tree = m_scrollingTree;
        bool onlyWaitingForTiles = m_waitingForTiles && m_reasons.containsOnly({ CompositionReason::RenderingUpdate });
        if (!m_reasons.isEmpty() && m_visible.load() && !m_frameScheduled && !m_frameInProgress && !onlyWaitingForTiles) {
            m_frameScheduled = true;
            m_compositorQueue->dispatch([this] { renderFrame(); });
        }
        wanted = !m_reasons.isEmpty() || m_frameScheduled || m_frameInProgress;
    }
    // (The scrolling thread's animated scrolls and its keeping in step
    // with the main thread, on the same clock.)
    if (tree && (wanted || tree->hasNodeWithActiveScrollAnimations())) {
        tree->displayDidRefresh(1);
        wanted = true;
    }
    return wanted;
}

void MlxLayerTreeHost::commitScene()
{
    Locker pending { m_pendingLayersLock };
    while (!m_pendingLayersToRemove.isEmpty())
        m_pendingLayersToRemove.takeAny()->invalidateTarget();
    if (!m_pendingLayers.isEmpty())
        m_committedLayers = WTF::move(m_pendingLayers);
}

void MlxLayerTreeHost::flushCompositingState(const OptionSet<CompositionReason>& reasons)
{
    if (reasons.hasExactlyOneBitSet() && reasons.contains(CompositionReason::Animation))
        return;
    // (The layers' targets: SkiaCompositingLayers on the CPU,
    // TextureMapperLayers on the GPU, whose tiles' updates are uploads
    // into textures: the context current.)
    bool skia = !m_onGPU;
    if (m_gpu)
        m_gpu->context->makeContextCurrent();
    commitScene();
    {
        Locker scene { m_sceneStateLock };
        m_rootLayer->flushPositionChanges(reasons, skia);
        for (auto& layer : m_committedLayers)
            Ref { layer }->flushPositionChanges(reasons, skia);
    }
    Vector<Ref<CoordinatedPlatformLayer>, 16> withTiles;
    m_rootLayer->flushCompositingState(reasons, skia);
    for (auto& layer : m_committedLayers) {
        Ref platformLayer = layer;
        platformLayer->flushCompositingState(reasons, skia);
        if (platformLayer->hasPendingBackingStoreTileUpdates())
            withTiles.append(WTF::move(platformLayer));
    }
    for (auto& layer : withTiles)
        layer->processPendingBackingStoreTileUpdates();
    if (m_onGPU)
        darkenSolidColors();
}

// CoordinatedPlatformLayer's contents colour (m_contentsColor, which WebKit
// keeps to itself and hands only to the layer's targets), read through a
// pointer to the member taken where C++ lets a private member be named:
// an explicit instantiation's arguments.
template<typename Tag, typename Tag::Type member>
struct MlxPrivateMember {
    friend typename Tag::Type mlxPrivateMember(Tag) { return member; }
};
struct MlxContentsColor {
    using Type = Color CoordinatedPlatformLayer::*;
    friend Type mlxPrivateMember(MlxContentsColor);
};
template struct MlxPrivateMember<MlxContentsColor, &CoordinatedPlatformLayer::m_contentsColor>;

// Forced dark on the GPU: a layer's solid colour (RenderLayerBacking's
// directly composited background), which the CPU's compositor draws through
// MlxDarkModeCanvas, is given to its TextureMapperLayer darkened (after
// each flush, which gives it the page's colour when that changes). Its
// tiles and images were darkened when they were recorded.
void MlxLayerTreeHost::darkenSolidColors()
{
    bool dark = m_forcedDark.load();
    auto darken = [dark](CoordinatedPlatformLayer& layer) {
        Color color;
        {
            Locker locker { layer.lock() };
            color = layer.*mlxPrivateMember(MlxContentsColor { });
        }
        if (!color.isValid())
            return;
        if (dark) {
            auto [r, g, b, a] = color.toColorTypeLossy<SRGBA<float>>().resolved();
            auto darkened = mlxForcedDarkColor(SkColor4f { r, g, b, a });
            color = Color(SRGBA<float> { darkened.fR, darkened.fG, darkened.fB, darkened.fA });
        }
        layer.ensureTarget().setSolidColor(color);
    };
    darken(m_rootLayer);
    for (auto& layer : m_committedLayers)
        darken(layer);
}

// A frame on the GPU (ThreadedCompositor::paintToTextureMapper): the
// animations applied, the damage collected, and only what it covers
// cleared (the page's white) and composed when the framebuffer holds the
// last frame.
bool MlxLayerTreeHost::paintFrameOnGPU(const IntSize& size, std::optional<Damage>& frameDamage, bool targetValid, bool& running)
{
    auto& gpu = *m_gpu;
    if (!gpu.context->makeContextCurrent())
        return false;
    if (gpu.ensureTarget(size))
        targetValid = false;
    glViewport(0, 0, size.width(), size.height());
    auto& root = m_rootLayer->ensureTarget();
    if (!root.transform().isIdentity())
        root.setTransform({ });
    running = root.applyAnimationsRecursively(MonotonicTime::now());

    auto& textureMapper = *gpu.textureMapper;
    FloatRect whole({ }, size);
    textureMapper.beginPainting(TextureMapper::FlipY::Yes);
    textureMapper.beginClip(TransformationMatrix(), FloatRoundedRect(whole));
    root.prepareForPainting(textureMapper);
    Damage damage(size);
    root.collectDamage(textureMapper, damage);
    std::optional<FloatRoundedRect> changed;
    IntRect cleared({ }, size);
    if (targetValid) {
        textureMapper.setDamage(damage);
        cleared = damage.bounds();
        if (!FloatRect(cleared).contains(whole))
            changed = FloatRoundedRect(FloatRect(cleared));
    } else
        textureMapper.setDamage(std::nullopt);
    if (changed)
        textureMapper.beginClip(TransformationMatrix(), *changed);
    if (!cleared.isEmpty()) {
        auto background = m_forcedDark.load() ? mlxForcedDarkColor(SkColors::kWhite) : SkColors::kWhite;
        glScissor(cleared.x(), cleared.y(), cleared.width(), cleared.height());
        glClearColor(background.fR, background.fG, background.fB, 1);
        glClearStencil(0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        root.paint(textureMapper);
    }
    if (changed)
        textureMapper.endClip();
    textureMapper.endClip();
    textureMapper.endPainting();
    frameDamage = WTF::move(damage);
    return true;
}

// The compositor's canvas in front of the frame's: a tile or an image
// drawn a part of it (SkiaCompositingLayer splits what it draws by the
// frame's damage: the damaged pieces of each tile) at whole pixels and its
// own size is drawn whole under a clip of that part. Skia draws a part of
// an image (SkBitmapDevice::drawImageRect, a source rectangle not at the
// image's origin) through an image shader and its raster pipeline, each
// pixel through its portable stages (projects/webkit/skia.mlx: no vector
// registers): a whole image at whole pixels is a sprite, rows copied or
// blended (SkSpriteBlitter, the port's row blends: SkiaRoutinesMlx.cpp).
// What shows is the same.
class MlxCompositingCanvas final : public SkNWayCanvas {
public:
    explicit MlxCompositingCanvas(SkCanvas& target)
        : SkNWayCanvas(target.imageInfo().width(), target.imageInfo().height())
    {
        addCanvas(&target);
        setMatrix(target.getLocalToDevice());
    }

private:
    static bool whole(const SkRect& rect)
    {
        return rect.fLeft == std::floor(rect.fLeft) && rect.fTop == std::floor(rect.fTop) && rect.fRight == std::floor(rect.fRight) && rect.fBottom == std::floor(rect.fBottom);
    }

    // Whether SOURCE's pixels go to DESTINATION one for one, at whole
    // pixels, under MATRIX (this canvas's and the entry's own).
    bool asSprite(const SkMatrix& matrix, const SkRect& source, const SkRect& destination) const
    {
        if (!matrix.isTranslate() || matrix.getTranslateX() != std::floor(matrix.getTranslateX()) || matrix.getTranslateY() != std::floor(matrix.getTranslateY()))
            return false;
        return source.width() == destination.width() && source.height() == destination.height() && whole(source) && whole(destination);
    }

    void drawPart(const SkImage* image, const SkMatrix* preView, const SkRect& source, const SkRect& destination, const SkPaint& paint)
    {
        for (int i = 0; i < fList.size(); ++i) {
            SkCanvas* target = fList[i];
            SkAutoCanvasRestore restore(target, true);
            if (preView)
                target->concat(*preView);
            target->clipRect(destination);
            target->drawImage(image, destination.fLeft - source.fLeft, destination.fTop - source.fTop, SkSamplingOptions(), &paint);
        }
    }

    void onDrawImageRect2(const SkImage* image, const SkRect& source, const SkRect& destination, const SkSamplingOptions& sampling, const SkPaint* paint, SrcRectConstraint constraint) override
    {
        if (!image || !asSprite(getTotalMatrix(), source, destination) || (paint && (paint->getMaskFilter() || paint->getShader()))) {
            SkNWayCanvas::onDrawImageRect2(image, source, destination, sampling, paint, constraint);
            return;
        }
        SkPaint plain = paint ? *paint : SkPaint();
        plain.setAntiAlias(false);
        drawPart(image, nullptr, source, destination, plain);
    }

    void onDrawEdgeAAImageSet2(const ImageSetEntry set[], int count, const SkPoint clips[], const SkMatrix preViewMatrices[], const SkSamplingOptions& sampling, const SkPaint* paint, SrcRectConstraint constraint) override
    {
        bool clipped = false;
        for (int i = 0; i < count; ++i)
            clipped |= set[i].fHasClip;
        if (clipped || (paint && (paint->getMaskFilter() || paint->getShader()))) {
            SkNWayCanvas::onDrawEdgeAAImageSet2(set, count, clips, preViewMatrices, sampling, paint, constraint);
            return;
        }
        for (int i = 0; i < count; ++i) {
            const auto& entry = set[i];
            const SkMatrix* preView = entry.fMatrixIndex >= 0 ? &preViewMatrices[entry.fMatrixIndex] : nullptr;
            SkMatrix matrix = getTotalMatrix();
            if (preView)
                matrix.preConcat(*preView);
            if (!entry.fImage || !asSprite(matrix, entry.fSrcRect, entry.fDstRect)) {
                SkNWayCanvas::onDrawEdgeAAImageSet2(&entry, 1, nullptr, preViewMatrices, sampling, paint, constraint);
                continue;
            }
            SkPaint plain = paint ? *paint : SkPaint();
            plain.setAntiAlias(false);
            plain.setAlphaf(plain.getAlphaf() * entry.fAlpha);
            drawPart(entry.fImage.get(), preView, entry.fSrcRect, entry.fDstRect, plain);
        }
    }
};

bool MlxLayerTreeHost::paintFrame(SkCanvas& frameCanvas, std::optional<Damage>& frameDamage, bool targetValid)
{
    MlxCompositingCanvas canvas(frameCanvas);
    auto& root = m_rootLayer->ensureSkiaTarget();
    root.setTransform({ });
    std::optional<Damage> prior;
    if (targetValid)
        prior = Damage(IntSize(canvas.getBaseLayerSize().width(), canvas.getBaseLayerSize().height()));
    if (m_forcedDark.load()) {
        // Forced dark: the layers' contents were painted dark as they were
        // recorded (SkiaPaintingEngineMlx.cpp); what the compositor fills
        // itself, the canvas and a layer's solid colour (RenderLayerBacking's
        // directly composited background), goes through the same filter
        // here. Tiles and images are drawn as images: as they are.
        MlxDarkModeCanvas dark(canvas);
        return root.paint(dark, frameDamage, prior, SK_ColorWHITE);
    }
    canvas.save();
    bool running = root.paint(canvas, frameDamage, prior, SK_ColorWHITE);
    canvas.restore();
    return running;
}

void MlxLayerTreeHost::renderFrame()
{
    OptionSet<CompositionReason> reasons;
    bool notify = false;
    {
        Locker locker { m_stateLock };
        m_frameScheduled = false;
        if (!m_visible.load() || m_invalidated)
            return;
        reasons = std::exchange(m_reasons, { });
        if (reasons.contains(CompositionReason::RenderingUpdate)) {
            if (m_pendingTiles.load()) {
                // (Its tiles still painting: the update's frame waits.)
                m_waitingForTiles = true;
                reasons.remove(CompositionReason::RenderingUpdate);
                m_reasons.add(CompositionReason::RenderingUpdate);
            } else
                notify = std::exchange(m_notifyRenderingUpdate, false);
        }
        m_frameInProgress = true;
    }
    if (notify) {
        RunLoop::mainSingleton().dispatch([this, alive = m_alive] {
            if (!alive->load())
                return;
            if (RefPtr page = m_page.get())
                page->willStartRenderingUpdateDisplay();
        });
    }
    auto started = MonotonicTime::now();
    flushCompositingState(reasons);
    auto flushed = MonotonicTime::now();

    bool running = false;
    MonotonicTime locked;
    MonotonicTime composed;
    if (m_asynchronousReads)
        renderFrameOnGPU(running, locked, composed);
    else {
        Locker locker { m_frameLock };
        locked = MonotonicTime::now();
        IntSize size = m_size;
        if (size.isEmpty()) {
            Locker root { m_rootLayer->lock() };
            size = IntSize(m_rootLayer->size());
        }
        if (size != m_frameSize) {
            m_frameSize = size;
            m_framePixels = Vector<uint32_t>(FillWith { }, static_cast<size_t>(size.width()) * size.height(), 0u);
            m_frameValid = false;
            m_frameReadable = false;
        }
        if (!size.isEmpty()) {
            std::optional<Damage> damage = Damage(size);
            bool valid = m_frameValid;
            if (m_onGPU) {
                // (What changed read back into the frame, all of it when it
                // was not valid: in one read, the damage's bounds, since
                // each read waits for the GPU.)
                if (!paintFrameOnGPU(size, damage, valid, running))
                    valid = false;
                IntRect changed = valid && damage ? damage->bounds() : IntRect({ }, size);
                composed = MonotonicTime::now();
                // (Presented directly: the embedder's take of the frame
                // copies it on the GPU into the window's buffer.)
                if (!m_presentsDirectly)
                    m_gpu->read(changed, m_framePixels.mutableSpan().data(), size.width());
            } else {
                auto info = SkImageInfo::MakeN32Premul(size.width(), size.height());
                auto surface = SkSurfaces::WrapPixels(info, m_framePixels.mutableSpan().data(), static_cast<size_t>(size.width()) * 4);
                running = paintFrame(*surface->getCanvas(), damage, valid);
            }
            m_frameValid = true;
            m_frameReadable = true;
            if (!valid)
                m_frameDamageFull = true;
            else if (damage) {
                for (const auto& rect : *damage) {
                    if (m_frameDamage.size() >= 32) {
                        m_frameDamageFull = true;
                        break;
                    }
                    if (!rect.isEmpty())
                        m_frameDamage.append(rect);
                }
            }
            m_hasFrame = true;
        }
    }
    m_animationsRunning = running;
    if (m_onGPU && m_countsTextures.load() && MonotonicTime::now() - m_texturesCounted >= 500_ms) {
        // (Texture names are handed out lowest first and reused: the
        // count ends after a run of names that are none.)
        m_texturesCounted = MonotonicTime::now();
        unsigned count = 0;
        for (GLuint name = 1, misses = 0; misses < 1024; ++name) {
            if (glIsTexture(name)) {
                ++count;
                misses = 0;
            } else
                ++misses;
        }
        m_textures = count;
    }
    static bool traceFrames = getenv("MLX_TRACE_COMPOSITOR");
    if (traceFrames) {
        auto painted = MonotonicTime::now();
        if (m_onGPU)
            fprintf(stderr, "compositor frame (gpu) flush %.1f wait %.1f paint %.1f ms (read back %.1f)\n", (flushed - started).milliseconds(), (locked - flushed).milliseconds(), (painted - locked).milliseconds(), (painted - composed).milliseconds());
        else
            fprintf(stderr, "compositor frame (cpu) flush %.1f wait %.1f paint %.1f ms\n", (flushed - started).milliseconds(), (locked - flushed).milliseconds(), (painted - locked).milliseconds());
    }
    countFrame();
    if (!m_asynchronousReads) {
        m_newFrame = true;
        if (m_frameReady)
            m_frameReady();
    }
    {
        Locker locker { m_stateLock };
        m_frameInProgress = false;
        if (running)
            m_reasons.add(CompositionReason::Animation);
        if (!m_reasons.isEmpty())
            scheduleFrameLocked();
    }
    if (notify) {
        RunLoop::mainSingleton().dispatch([this, alive = m_alive] {
            if (alive->load())
                didCompositeRenderingUpdate();
        });
    }
}

// A frame on the GPU with asynchronous reads: composed without the
// frame's lock (the embedder's take of the last frame does not wait for
// it), what changed read into a buffer (the whole when the framebuffer did
// not hold the last frame), and the reads the GPU is done with taken into
// the frame, each a frame ready; those still going taken the next frame,
// or by a poll of the compositor thread every millisecond until they are.
void MlxLayerTreeHost::renderFrameOnGPU(bool& running, MonotonicTime& locked, MonotonicTime& composed)
{
    takeReads(false);
    IntSize size;
    bool valid;
    {
        Locker locker { m_frameLock };
        size = m_size;
        if (size.isEmpty()) {
            Locker root { m_rootLayer->lock() };
            size = IntSize(m_rootLayer->size());
        }
        if (size != m_frameSize) {
            m_frameSize = size;
            m_framePixels = Vector<uint32_t>(FillWith { }, static_cast<size_t>(size.width()) * size.height(), 0u);
            m_frameValid = false;
            m_frameReadable = false;
        }
        valid = m_frameValid;
        m_frameValid = true;
    }
    locked = MonotonicTime::now();
    composed = locked;
    if (size.isEmpty() || !m_gpu)
        return;
    std::optional<Damage> damage = Damage(size);
    if (!paintFrameOnGPU(size, damage, valid, running))
        valid = false;
    composed = MonotonicTime::now();
    bool full = !valid || !damage;
    Vector<IntRect> rects;
    if (!full) {
        for (const auto& rect : *damage) {
            if (rects.size() >= 32) {
                full = true;
                break;
            }
            if (!rect.isEmpty())
                rects.append(rect);
        }
    }
    IntRect changed = full ? IntRect({ }, size) : damage->bounds();
    // (Every buffer busy: the oldest read waited for.)
    if (m_gpu->readCount == GPUCompositor::maximumReads)
        takeReads(true);
    m_gpu->startRead(changed, size, full, WTF::move(rects));
    takeReads(false);
    if (m_gpu->readsPending())
        scheduleReadPoll();
}

// The reads done taken into the frame, oldest first (WAIT: the oldest
// waited for).
void MlxLayerTreeHost::takeReads(bool wait)
{
    if (!m_gpu)
        return;
    auto take = [this](const GPUCompositor::Read& read, const uint32_t* pixels) {
        Locker locker { m_frameLock };
        if (read.frameSize != m_frameSize)
            return;
        if (pixels)
            GPUCompositor::copySwapped(pixels, read.rect, m_framePixels.mutableSpan().data(), m_frameSize.width());
        if (read.full)
            m_frameDamageFull = true;
        else {
            for (const auto& rect : read.damage) {
                if (m_frameDamage.size() >= 32) {
                    m_frameDamageFull = true;
                    break;
                }
                m_frameDamage.append(rect);
            }
        }
        m_frameReadable = true;
        m_hasFrame = true;
    };
    while (m_gpu->finishRead(std::exchange(wait, false), take)) {
        m_newFrame = true;
        if (m_frameReady)
            m_frameReady();
    }
}

void MlxLayerTreeHost::scheduleReadPoll()
{
    if (std::exchange(m_readPollScheduled, true))
        return;
    m_compositorQueue->dispatchAfter(1_ms, [this, alive = m_alive] {
        if (!alive->load())
            return;
        m_readPollScheduled = false;
        if (!m_gpu || !m_gpu->context->makeContextCurrent())
            return;
        takeReads(false);
        if (m_gpu->readsPending())
            scheduleReadPoll();
    });
}

void MlxLayerTreeHost::setFrameReadyCallback(Function<void()>&& callback)
{
    m_frameReady = WTF::move(callback);
}

bool MlxLayerTreeHost::takeFrame(uint32_t* pixels, int width, int height, int stride, Vector<IntRect>& damage)
{
    if (m_presentsDirectly)
        return presentFrame(pixels, width, height, stride, damage);
    static bool trace = getenv("MLX_TRACE_COMPOSITOR");
    auto started = MonotonicTime::now();
    Locker locker { m_frameLock };
    m_newFrame = false;
    if (!(m_asynchronousReads ? m_frameReadable : m_frameValid) || m_frameSize.isEmpty())
        return false;
    int copyWidth = std::min(width, m_frameSize.width());
    int copyHeight = std::min(height, m_frameSize.height());
    for (int y = 0; y < height; ++y) {
        uint32_t* row = pixels + static_cast<size_t>(y) * stride;
        if (y < copyHeight) {
            memcpy(row, m_framePixels.span().data() + static_cast<size_t>(y) * m_frameSize.width(), static_cast<size_t>(copyWidth) * 4);
            for (int x = copyWidth; x < width; ++x)
                row[x] = 0xFFFFFFFF;
        } else {
            for (int x = 0; x < width; ++x)
                row[x] = 0xFFFFFFFF;
        }
    }
    if (m_frameDamageFull || m_frameSize != IntSize(width, height))
        damage = { IntRect(0, 0, width, height) };
    else
        damage = WTF::move(m_frameDamage);
    m_frameDamage = { };
    m_frameDamageFull = false;
    if (trace)
        fprintf(stderr, "compositor take %.1f ms (copied)\n", (MonotonicTime::now() - started).milliseconds());
    return true;
}

// The frame taken directly (m_presentsDirectly): on the compositor's
// thread, between its frames, the last frame copied by the GPU into PIXELS
// (the window's buffer, imported into ANGLE's device), what is past the
// frame white; read back there if the copy fails.
bool MlxLayerTreeHost::presentFrame(uint32_t* pixels, int width, int height, int stride, Vector<IntRect>& damage)
{
    bool taken = false;
    auto asked = MonotonicTime::now();
    double presentTime = 0;
    m_compositorQueue->dispatchSync([&] {
        Locker locker { m_frameLock };
        m_newFrame = false;
        if (!m_frameValid || m_frameSize.isEmpty() || !m_gpu || !m_gpu->context->makeContextCurrent())
            return;
        int copyWidth = std::min(width, m_frameSize.width());
        int copyHeight = std::min(height, m_frameSize.height());
        auto started = MonotonicTime::now();
        if (!m_gpu->present(pixels, copyWidth, copyHeight, stride)) {
            m_gpu->read(IntRect(0, 0, copyWidth, copyHeight), pixels, stride);
            // (A window buffer the device cannot import, wl_shm's memfd
            // pages on amdgpu: this frame read synchronously, the next
            // ones read asynchronously as without a presenter, instead of
            // a synchronous read of every frame, 60 ms each on an RX 580.)
            m_presentsDirectly = false;
            m_asynchronousReads = m_gpu->asynchronous;
            if (getenv("MLX_TRACE_COMPOSITOR"))
                fprintf(stderr, "compositor: the window's buffer could not be presented on the GPU: frames %s\n", m_asynchronousReads ? "read back asynchronously" : "read back");
        }
        presentTime = (MonotonicTime::now() - started).milliseconds();
        for (int y = 0; y < height; ++y) {
            uint32_t* row = pixels + static_cast<size_t>(y) * stride;
            for (int x = y < copyHeight ? copyWidth : 0; x < width; ++x)
                row[x] = 0xFFFFFFFF;
        }
        if (m_frameDamageFull || m_frameSize != IntSize(width, height))
            damage = { IntRect(0, 0, width, height) };
        else
            damage = WTF::move(m_frameDamage);
        m_frameDamage = { };
        m_frameDamageFull = false;
        taken = true;
    });
    static bool trace = getenv("MLX_TRACE_COMPOSITOR");
    if (trace && taken)
        fprintf(stderr, "compositor take %.1f ms (presented %.1f)\n", (MonotonicTime::now() - asked).milliseconds(), presentTime);
    return taken;
}

void MlxLayerTreeHost::composite(SkCanvas& canvas, Vector<IntRect>* damage)
{
    {
        Locker locker { m_stateLock };
        m_reasons = { };
    }
    IntSize size(canvas.getBaseLayerSize().width(), canvas.getBaseLayerSize().height());
    std::optional<Damage> frameDamage = Damage(size);
    if (m_onGPU) {
        // (On the compositor's thread, where its context is: the whole
        // frame composed and read back into the canvas.)
        m_compositorQueue->dispatchSync([&] {
            flushCompositingState({ CompositionReason::RenderingUpdate, CompositionReason::AsyncScrolling });
            if (size.isEmpty() || !m_gpu)
                return;
            bool running = false;
            paintFrameOnGPU(size, frameDamage, false, running);
            m_animationsRunning = running;
            Vector<uint32_t> pixels(static_cast<size_t>(size.width()) * size.height());
            m_gpu->read(IntRect({ }, size), pixels.mutableSpan().data(), size.width());
            auto info = SkImageInfo::MakeN32Premul(size.width(), size.height());
            canvas.writePixels(info, pixels.span().data(), static_cast<size_t>(size.width()) * 4, 0, 0);
        });
    } else {
        flushCompositingState({ CompositionReason::RenderingUpdate, CompositionReason::AsyncScrolling });
        m_animationsRunning = paintFrame(canvas, frameDamage, false);
    }
    countFrame();
    if (damage && frameDamage) {
        damage->clear();
        for (const auto& rect : *frameDamage)
            damage->append(rect);
    }
}

// ---------------------------------------------------------------------------
// Wheel events (EventDispatcher's, WebKit2)

void MlxLayerTreeHost::handleWheelEvent(LocalFrame& frame, const PlatformWheelEvent& event)
{
    OptionSet<WheelEventProcessingSteps> mainThreadSteps { WheelEventProcessingSteps::SynchronousScrolling, WheelEventProcessingSteps::BlockingDOMEventDispatch };
    RefPtr tree = mainThreadScrollingTree();
    if (!tree || !is<ThreadedScrollingTree>(*tree) || m_mode == Mode::Synchronous) {
        frame.eventHandler().handleWheelEvent(event, mainThreadSteps);
        return;
    }
    auto steps = tree->determineWheelEventProcessing(event);
    bool useMainThread = steps.contains(WheelEventProcessingSteps::SynchronousScrolling);
    // (A gesture's later events go to the scrolling thread to its end.)
    if ((event.phase() == PlatformWheelEventPhase::Changed || event.phase() == PlatformWheelEventPhase::Ended) && tree->isUserScrollInProgressAtEventLocation(event))
        useMainThread = false;
    tree->willProcessWheelEvent();
    auto toMainThread = [this, alive = m_alive, event](OptionSet<WheelEventProcessingSteps> steps) {
        RunLoop::mainSingleton().dispatch([this, alive, event, steps] {
            if (!alive->load())
                return;
            RefPtr page = m_page.get();
            RefPtr frame = page ? page->localMainFrame() : nullptr;
            if (frame)
                frame->eventHandler().handleWheelEvent(event, steps - WheelEventProcessingSteps::AsyncScrolling);
        });
    };
    auto threadedTree = downcast<ThreadedScrollingTree>(tree.releaseNonNull());
    ScrollingThread::dispatch([threadedTree = WTF::move(threadedTree), event, steps, useMainThread, toMainThread = WTF::move(toMainThread)] {
        if (useMainThread) {
            threadedTree->willSendEventToMainThread(event);
            toMainThread(steps);
            threadedTree->waitForEventToBeProcessedByMainThread(event);
            return;
        }
        auto result = threadedTree->handleWheelEvent(event, steps);
        if (result.needsMainThreadProcessing())
            toMainThread(result.steps);
    });
}

// ---------------------------------------------------------------------------
// Counts

void MlxLayerTreeHost::countFrame()
{
    ++m_frames;
    auto now = MonotonicTime::now();
    Locker locker { m_fpsLock };
    m_frameTimes.append(now);
    while (!m_frameTimes.isEmpty() && now - m_frameTimes.first() > 1_s)
        m_frameTimes.removeAt(0);
}

double MlxLayerTreeHost::framesPerSecond() const
{
    auto& self = const_cast<MlxLayerTreeHost&>(*this);
    Locker locker { self.m_fpsLock };
    if (self.m_frameTimes.size() < 2)
        return 0;
    auto span = self.m_frameTimes.last() - self.m_frameTimes.first();
    if (span <= 0_s)
        return 0;
    return (self.m_frameTimes.size() - 1) / span.seconds();
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
