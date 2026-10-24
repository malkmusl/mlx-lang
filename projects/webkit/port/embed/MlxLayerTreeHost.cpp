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
#include "TextureMapperLayer.h"
#include "ThreadedScrollingCoordinator.h"
#include "ThreadedScrollingTree.h"
#include "port/platform/graphics/mlx/MlxDarkMode.h"
#include "port/platform/graphics/mlx/PlatformDisplayMlx.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkCanvas.h>
#include <skia/core/SkImage.h>
#include <skia/core/SkSurface.h>
#include <skia/utils/SkNWayCanvas.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <stdlib.h>
#include <wtf/Condition.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/SetForScope.h>
#include <wtf/text/MakeString.h>
#include <wtf/Threading.h>

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

// Whether a host is to compose on the GPU, and why (compositingChoice):
// MLX_COMPOSITING=cpu, or the embedder's setGPUCompositingAllowed(false),
// the CPU; MLX_COMPOSITING=gpu, the GPU whatever the device; otherwise
// the GPU unless ANGLE's Vulkan device is a CPU's (lavapipe, llvmpipe,
// SwiftShader: TextureMapper's frames and their read back cost more there
// than the CPU's compositor). WebGL uses ANGLE on whatever device there
// is either way.
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
    if (mode && !strcmp(mode, "gpu")) {
        reason = "MLX_COMPOSITING=gpu"_s;
        return true;
    }
    if (!mlxANGLEDisplay()) {
        reason = "no Vulkan device"_s;
        return false;
    }
    const char* device = "";
    if (mlxANGLEDeviceIsCPU(&device)) {
        reason = makeString("a software Vulkan device, "_s, String::fromLatin1(device), "; MLX_COMPOSITING=gpu composes there"_s);
        return false;
    }
    reason = makeString("Vulkan device "_s, String::fromLatin1(device));
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

    static std::unique_ptr<GPUCompositor> create()
    {
        auto& display = PlatformDisplay::sharedDisplay();
        auto context = GLContext::create(display.glDisplay(), GLContext::Target::Surfaceless, display.sharingGLContext());
        if (!context || !context->makeContextCurrent())
            return nullptr;
        auto compositor = makeUnique<GPUCompositor>();
        compositor->context = WTF::move(context);
        compositor->textureMapper = TextureMapper::create();
        if (getenv("MLX_TRACE_COMPOSITOR"))
            fprintf(stderr, "compositor on the GPU: %s\n", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
        return compositor;
    }

    ~GPUCompositor()
    {
        if (!context->makeContextCurrent())
            return;
        textureMapper = nullptr;
        deleteTarget();
    }

    void deleteTarget()
    {
        if (framebuffer)
            glDeleteFramebuffers(1, &framebuffer);
        if (color)
            glDeleteTextures(1, &color);
        if (depthStencil)
            glDeleteRenderbuffers(1, &depthStencil);
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
        for (int y = 0; y < rect.height(); ++y) {
            uint32_t* row = pixels + static_cast<size_t>(rect.y() + y) * stride + rect.x();
            const uint32_t* source = rows.span().data() + y * width;
            for (size_t x = 0; x < width; ++x) {
                uint32_t pixel = source[x];
                row[x] = (pixel & 0xFF00FF00) | ((pixel & 0xFF) << 16) | ((pixel >> 16) & 0xFF);
            }
        }
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
            m_gpu = GPUCompositor::create();
        });
        m_onGPU = !!m_gpu;
        if (!m_onGPU)
            m_compositingReason = "no GL context on ANGLE's display"_s;
        if (!m_onGPU && m_mode == Mode::Synchronous)
            m_compositorQueue = nullptr;
    }
    if (!m_onGPU && getenv("MLX_TRACE_COMPOSITOR"))
        fprintf(stderr, "compositor on the CPU: %s\n", m_compositingReason.utf8().data());
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
    {
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
                m_gpu->read(changed, m_framePixels.mutableSpan().data(), size.width());
            } else {
                auto info = SkImageInfo::MakeN32Premul(size.width(), size.height());
                auto surface = SkSurfaces::WrapPixels(info, m_framePixels.mutableSpan().data(), static_cast<size_t>(size.width()) * 4);
                running = paintFrame(*surface->getCanvas(), damage, valid);
            }
            m_frameValid = true;
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
    m_newFrame = true;
    if (m_frameReady)
        m_frameReady();
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

void MlxLayerTreeHost::setFrameReadyCallback(Function<void()>&& callback)
{
    m_frameReady = WTF::move(callback);
}

bool MlxLayerTreeHost::takeFrame(uint32_t* pixels, int width, int height, int stride, Vector<IntRect>& damage)
{
    Locker locker { m_frameLock };
    m_newFrame = false;
    if (!m_frameValid || m_frameSize.isEmpty())
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
    return true;
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
