// A page's composited layers (PORT.md, "Compositing"): what
// WebKit2's LayerTreeHost, CoordinatedSceneState and ThreadedCompositor
// (Source/WebKit, which the port does not build) are to WPE, for the
// port's single process. WebCore's coordinated graphics make the layers
// (GraphicsLayerCoordinated, each with its thread-safe
// CoordinatedPlatformLayer: this host is their client), paint their
// tiles (the port's SkiaPaintingEngine: recorded on the main thread,
// played back on worker threads) and composite them (SkiaCompositingLayer:
// transforms, opacity, clips, filters, CSS animations of transform and
// opacity run on the compositor's clock, damage). The host keeps the
// scene (the layers made, the root and its children), runs WebCore's
// rendering update when the frame clock says (a DisplayRefreshMonitor of
// the port: requestAnimationFrame and the rendering update at the
// display's rate) and composes frames into a raster frame of the view's
// size, with the damage of each.
//
// On the GPU when it can (WPE's way): the compositor thread owns a GL
// context of ANGLE's surfaceless EGL display (PlatformDisplayMlx.h), the
// layers' targets are TextureMapperLayers, their tiles textures, and
// WebKit's TextureMapper composes them into a framebuffer of the view's
// size, of which each frame's damage is read back into the raster frame
// (Threaded: asynchronously, the frame ready when its read is done).
// On the CPU (no Vulkan device, setGPUCompositingAllowed(false),
// MLX_COMPOSITING=cpu): SkiaCompositingLayer composes the layers straight
// into the raster frame.
//
// Threaded (Voyager): a compositor thread composes when something asks for
// a frame (a rendering update, a scroll of the scrolling thread, a running
// animation), on the ticks of MlxDisplayLink, the process's frame clock;
// the embedder takes each frame and its damage (takeFrame) when told one
// is ready (the frame-ready callback, on the compositor thread). Wheel
// events go to the scrolling thread first (WebCore's ThreadedScrollingTree,
// as WebKit2's EventDispatcher sends them), which scrolls the layers
// without the main thread; what it cannot handle goes to the main thread.
//
// Synchronous (webcore-dump): composite() composes on the calling thread
// (on the GPU: on the compositor's thread, waited for), after the painting
// of the tiles is done.
#pragma once

#include "CoordinatedPlatformLayer.h"
#include "DisplayRefreshMonitorFactory.h"
#include "GraphicsLayerFactory.h"
#include "IntRect.h"
#include "IntSize.h"
#include "PlatformWheelEvent.h"
#include <atomic>
#include <memory>
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/MonotonicTime.h>
#include <wtf/OptionSet.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/WTFString.h>

class SkCanvas;

namespace WebCore {

class Damage;
class GraphicsLayer;
class GraphicsLayerCoordinated;
class LocalFrame;
class NativeImage;
class Page;
class ScrollableArea;
class ScrollingCoordinator;
class ScrollingTree;
class Settings;
class SkiaPaintingEngine;
class MlxDisplayLinkObserver;

class MlxLayerTreeHost final : public GraphicsLayerFactory, public CoordinatedPlatformLayer::Client, public DisplayRefreshMonitorFactory, public CanMakeWeakPtr<MlxLayerTreeHost> {
    WTF_MAKE_NONCOPYABLE(MlxLayerTreeHost);
    WTF_MAKE_TZONE_ALLOCATED_INLINE(MlxLayerTreeHost);
public:
    enum class Mode : bool { Threaded, Synchronous };

    MlxLayerTreeHost(Page&, Mode);
    ~MlxLayerTreeHost();

    // The settings of a composited page: compositing forced (the view's
    // root has a layer: the main frame scrolls on the scrolling thread),
    // fixed elements composited, frames and overflow scrolled
    // asynchronously, damage propagated for compositing from it.
    static void configure(Settings&);

    // Whether hosts made from now on may compose on the GPU (true by
    // default: on the GPU when a display exists, on any Vulkan device,
    // software ones included; MLX_COMPOSITING=cpu never);
    // false is the CPU's compositor (an embedder's --no-gpu-compositing).
    static void setGPUCompositingAllowed(bool);
    // Whether this host composes on the GPU.
    bool composesOnGPU() const { return m_onGPU; }
    // Why the host composes where it does (the device, or what chose).
    const String& compositingReason() const { return m_compositingReason; }

    // The host of LAYER's page (the client of its platform layer).
    static MlxLayerTreeHost& of(const GraphicsLayerCoordinated&);

    // What the page's ChromeClient forwards.
    GraphicsLayerFactory* graphicsLayerFactory() { return this; }
    DisplayRefreshMonitorFactory* displayRefreshMonitorFactory() { return this; }
    void attachRootGraphicsLayer(GraphicsLayer*);
    void attachViewOverlayGraphicsLayer(GraphicsLayer*);
    // The rendering update is due (RenderingUpdateScheduler, on the
    // frame clock's tick): run now, or after the frame of the one before.
    void triggerRenderingUpdate();
    // (The page's asynchronous scrolling: WebCore's threaded coordinator
    // and tree, as WPE's; the host finds the tree on its page.)
    static RefPtr<ScrollingCoordinator> createScrollingCoordinator(Page&);
    // (A composited scrolled area's scrollbars drawn by the scrolling
    // thread; ChromeClient::ensureScrollbarsController.)
    static void ensureScrollbarsController(Page&, ScrollableArea&);

    // The view's size (the frame's), whether its frames are wanted (a tab
    // shown), whether its layers are painted in forced dark.
    void setSize(const IntSize&);
    void setVisible(bool);
    void setForcedDark(bool);
    bool paintsForcedDark() const { return m_forcedDark.load(); }

    // WebCore's rendering update: style, layout, animation frames, the
    // layers' changes flushed (tiles recorded), the scene committed, a
    // frame asked for.
    void updateRendering();

    // Threaded: CALLBACK runs on the compositor thread after each frame.
    void setFrameReadyCallback(Function<void()>&&);
    // The last frame copied into PIXELS (WIDTH x HEIGHT, STRIDE words a
    // row; what the frame does not cover is cleared), DAMAGE the
    // rectangles that changed since the last take (all of it after a
    // resize or a first take); false when no frame is composed yet.
    bool takeFrame(uint32_t* pixels, int width, int height, int stride, Vector<IntRect>& damage);
    // Whether a frame was composed since the last take.
    bool hasNewFrame() const { return m_newFrame.load(); }

    // Synchronous: the scene composed into CANVAS now (the tiles' painting
    // waited for); the damage of this frame in DAMAGE when given.
    void composite(SkCanvas&, Vector<IntRect>* damage = nullptr);
    // Synchronous: whether CSS animations of the layers still run (another
    // composite() shows their next state).
    bool hasRunningAnimations() const { return m_animationsRunning.load(); }

    // A wheel event: to the scrolling thread first.
    void handleWheelEvent(LocalFrame&, const PlatformWheelEvent&);

    // Counts: the layers' contents recorded on the main thread (paints),
    // frames composed, and frames a second over the last second (on the
    // compositor thread).
    unsigned paints() const { return m_paints.load(); }
    unsigned frames() const { return m_frames.load(); }
    double framesPerSecond() const;
    void didPaintLayer() { ++m_paints; }
    // On the GPU, with counting asked for (setCountsTextures): the GL
    // textures in the compositors' share group (BitmapTexturePool's,
    // WebGL's, the frames' targets), counted by the compositor thread
    // twice a second at most while it composes.
    void setCountsTextures(bool counts) { m_countsTextures = counts; }
    unsigned textures() const { return m_textures.load(); }

    // The page goes away: the layers let go, the compositor stopped.
    void invalidate();

    // The frame clock's tick (MlxDisplayLink, its thread): false once no
    // frame is wanted.
    bool displayLinkFired(MonotonicTime);

private:
    // GraphicsLayerFactory
    Ref<GraphicsLayer> createGraphicsLayer(GraphicsLayer::Type, GraphicsLayerClient&) final;

    // CoordinatedPlatformLayer::Client
    void attachLayer(CoordinatedPlatformLayer&) final;
    void detachLayer(CoordinatedPlatformLayer&) final;
    SkiaPaintingEngine& paintingEngine() const final { return *m_paintingEngine; }
    Ref<CoordinatedImageBackingStore> imageBackingStore(Ref<NativeImage>&&) final;
    void notifyCompositionRequired() final;
    bool isCompositionRequiredOrOngoing() const final;
    void requestComposition(CompositionReason) final;
    RunLoop* compositingRunLoop() const final;
    int maxTextureSize() const final { return 8192; }
    void willPaintTile() final { ++m_pendingTiles; }
    void didPaintTile() final;

    // DisplayRefreshMonitorFactory
    RefPtr<DisplayRefreshMonitor> createDisplayRefreshMonitor(PlatformDisplayID) final;

    void updateRootChildren();
    bool flushScene();
    void flushPendingState();
    void commitScene();
    void flushCompositingState(const OptionSet<CompositionReason>&);
    void requestCompositionForRenderingUpdate();
    void scheduleFrameLocked() WTF_REQUIRES_LOCK(m_stateLock);
    void renderFrame();
    void renderFrameOnGPU(bool& running, MonotonicTime& locked, MonotonicTime& composed);
    void takeReads(bool wait);
    void scheduleReadPoll();
    bool paintFrame(SkCanvas&, std::optional<Damage>& frameDamage, bool targetValid);
    // The GPU's compositor (its GL context, TextureMapper and framebuffer,
    // on the compositor thread): the frame of SIZE composed (only its
    // damage when TARGETVALID), the damage in FRAMEDAMAGE; false when its
    // context is lost.
    struct GPUCompositor;
    bool paintFrameOnGPU(const IntSize&, std::optional<Damage>& frameDamage, bool targetValid, bool& running);
    void darkenSolidColors();
    void didCompositeRenderingUpdate();
    void countFrame();
    RefPtr<ScrollingTree> mainThreadScrollingTree();

    WeakPtr<Page> m_page;
    Mode m_mode;
    Ref<CoordinatedPlatformLayer> m_rootLayer;
    GraphicsLayer* m_rootCompositingLayer { nullptr };
    GraphicsLayer* m_overlayCompositingLayer { nullptr };
    std::unique_ptr<SkiaPaintingEngine> m_paintingEngine;
    HashMap<uint64_t, Ref<CoordinatedImageBackingStore>> m_imageBackingStores;
    // (Written on the main thread under m_frameLock, which the compositor
    // reads it under.)
    IntSize m_size;
    std::atomic<bool> m_forcedDark { false };
    std::atomic<bool> m_visible { true };
    bool m_invalidated { false };
    // (False once the host is gone: what was dispatched to the main
    // thread before then does nothing.)
    std::shared_ptr<std::atomic<bool>> m_alive;

    // The scene (CoordinatedSceneState's): the layers made (main thread),
    // those handed to the compositor at the last flush, those it has.
    Lock m_layersLock;
    HashSet<Ref<CoordinatedPlatformLayer>> m_layers WTF_GUARDED_BY_LOCK(m_layersLock);
    HashSet<Ref<CoordinatedPlatformLayer>> m_layersToRemove;
    Lock m_pendingLayersLock;
    HashSet<Ref<CoordinatedPlatformLayer>> m_pendingLayers WTF_GUARDED_BY_LOCK(m_pendingLayersLock);
    HashSet<Ref<CoordinatedPlatformLayer>> m_pendingLayersToRemove WTF_GUARDED_BY_LOCK(m_pendingLayersLock);
    HashSet<Ref<CoordinatedPlatformLayer>> m_committedLayers;
    Lock m_sceneStateLock;
    std::atomic<bool> m_didChangeLayers { false };
    std::atomic<unsigned> m_pendingTiles { 0 };

    // The rendering update's pacing (LayerTreeHost's): one at a time,
    // the next after the compositor has shown the one before.
    bool m_compositionRequired { false };
    bool m_compositionRequiredInScrollingThread { false };
    bool m_isWaitingForRenderer { false };
    bool m_scheduledWhileWaitingForRenderer { false };
    bool m_isUpdatingRendering { false };
    bool m_pendingResize { false };

    // The compositor (ThreadedCompositor's state).
    RefPtr<WorkQueue> m_compositorQueue;
    std::unique_ptr<MlxDisplayLinkObserver> m_displayLinkObserver;
    mutable Lock m_stateLock;
    OptionSet<CompositionReason> m_reasons WTF_GUARDED_BY_LOCK(m_stateLock);
    bool m_frameScheduled WTF_GUARDED_BY_LOCK(m_stateLock) { false };
    bool m_frameInProgress WTF_GUARDED_BY_LOCK(m_stateLock) { false };
    bool m_waitingForTiles WTF_GUARDED_BY_LOCK(m_stateLock) { false };
    bool m_notifyRenderingUpdate WTF_GUARDED_BY_LOCK(m_stateLock) { false };
    RefPtr<ScrollingTree> m_scrollingTree WTF_GUARDED_BY_LOCK(m_stateLock);
    Function<void()> m_frameReady;
    std::atomic<bool> m_animationsRunning { false };
    std::unique_ptr<GPUCompositor> m_gpu;
    bool m_onGPU { false };
    // (On the GPU, Threaded: frames read back asynchronously.)
    bool m_asynchronousReads { false };
    bool m_readPollScheduled { false };
    String m_compositingReason;
    // (On the GPU: the damage the layers' TextureMapperLayers infer in the
    // view's coordinates, shared by all, as LayerTreeHost's.)
    std::shared_ptr<Damage> m_globalDamage;

    // The frame (the compositor's target): its pixels, whether they hold
    // the last frame, the damage not yet taken.
    Lock m_frameLock;
    Vector<uint32_t> m_framePixels WTF_GUARDED_BY_LOCK(m_frameLock);
    IntSize m_frameSize WTF_GUARDED_BY_LOCK(m_frameLock);
    bool m_frameValid WTF_GUARDED_BY_LOCK(m_frameLock) { false };
    // (Asynchronous reads: whether a frame of its size has been read.)
    bool m_frameReadable WTF_GUARDED_BY_LOCK(m_frameLock) { false };
    Vector<IntRect> m_frameDamage WTF_GUARDED_BY_LOCK(m_frameLock);
    bool m_frameDamageFull WTF_GUARDED_BY_LOCK(m_frameLock) { true };
    std::atomic<bool> m_newFrame { false };
    std::atomic<bool> m_hasFrame { false };

    std::atomic<unsigned> m_paints { 0 };
    std::atomic<unsigned> m_frames { 0 };
    std::atomic<bool> m_countsTextures { false };
    std::atomic<unsigned> m_textures { 0 };
    MonotonicTime m_texturesCounted;
    Lock m_fpsLock;
    Vector<MonotonicTime> m_frameTimes WTF_GUARDED_BY_LOCK(m_fpsLock);
};

} // namespace WebCore
