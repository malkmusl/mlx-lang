// The Mlx port's SkiaPaintingEngine (platform/graphics/skia/
// SkiaPaintingEngine.h, WebKit's header): how a composited layer's dirty
// tiles are painted. As WebKit's own engine does on the CPU, the layer's
// contents are recorded on the main thread (GraphicsContextSkia over an
// SkPictureRecorder: the main thread's paint) and the recording is played
// back into each dirty tile's buffer (CoordinatedUnacceleratedTileBuffer)
// on worker threads, which the compositor copies into its tiles
// (SkiaBackingStoreMlx.cpp). With a GrContextThreadSafeProxy (the GPU
// compositor's host gives one with MLX_PAINTING=gpu: MlxLayerTreeHost),
// the recording is played
// back by Ganesh over ANGLE's GLES instead, as WebKit's engine does without
// deferred display lists: into a texture of BitmapTexturePool's
// (CoordinatedAcceleratedTileBuffer, taken on the main thread with its
// Skia GL context current) on a GPU painting thread with its own
// (PlatformDisplayMlx.cpp), which the compositor's tile then shows or
// copies from on the GPU: no upload. WebKit's SkiaPaintingEngine.cpp (not
// compiled: lists/webcore-mlx-skipped.txt) also has atlases of images in
// GL textures and deferred display lists; the port's recordings keep
// their images as they are (raster images Ganesh uploads as it draws).
//
// Forced dark mode (PORT.md, "Forced dark mode") is applied here, to the
// layers' contents as they are recorded (MlxDarkModeCanvas in front of the
// recorder), not when the layers are composited: images stay as they are,
// and a layer's colours are darkened once, not each frame. Each recording
// is counted for the page's host (MlxLayerTreeHost::didPaintLayer): the
// paints Voyager traces. Played back on the CPU or on the GPU, the
// recording is the same, darkened or not.
#include "config.h"
#include "SkiaPaintingEngine.h"

#if USE(COORDINATED_GRAPHICS) && USE(SKIA)
#include "BitmapTexturePool.h"
#include "CoordinatedPlatformLayer.h"
#include "CoordinatedTileBuffer.h"
#include "GraphicsContextSkia.h"
#include "GLContext.h"
#include "GraphicsLayerCoordinated.h"
#include "PlatformDisplay.h"
#include "SkiaRecordingResult.h"
#include "port/embed/MlxLayerTreeHost.h"
#include "port/platform/graphics/mlx/MlxDarkMode.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkPictureRecorder.h>
#include <skia/core/SkBBHFactory.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/NumberOfCores.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SkiaPaintingEngine);

SkiaPaintingEngine::SkiaPaintingEngine(sk_sp<GrContextThreadSafeProxy>&& threadSafeGrContext)
    : m_threadSafeGrContext(WTF::move(threadSafeGrContext))
{
    if (m_threadSafeGrContext)
        m_paintingWorkerPool = WorkerPool::create("MlxGPUPainting"_s, numberOfGPUPaintingThreads());
    else
        m_paintingWorkerPool = WorkerPool::create("MlxPaintingWorker"_s, numberOfCPUPaintingThreads());
}

SkiaPaintingEngine::~SkiaPaintingEngine() = default;

std::unique_ptr<SkiaPaintingEngine> SkiaPaintingEngine::create(sk_sp<GrContextThreadSafeProxy>&& threadSafeGrContext)
{
    return makeUnique<SkiaPaintingEngine>(WTF::move(threadSafeGrContext));
}

// Half the processor's cores, 1 to 8 (WEBKIT_SKIA_CPU_PAINTING_THREADS, as
// WebKit's engine reads it, sets it).
unsigned SkiaPaintingEngine::numberOfCPUPaintingThreads()
{
    static unsigned count = [] {
        unsigned threads = std::max(1, std::min(8, WTF::numberOfProcessorCores() / 2));
        if (const char* variable = getenv("WEBKIT_SKIA_CPU_PAINTING_THREADS")) {
            auto value = parseInteger<unsigned>(StringView::fromLatin1(variable));
            if (value && *value >= 1 && *value <= 8)
                threads = *value;
        }
        return threads;
    }();
    return count;
}

// Painting on the GPU: 2 threads with four cores or more, else 1, as
// WebKit's engine has it (WEBKIT_SKIA_GPU_PAINTING_THREADS, 1 to 4).
unsigned SkiaPaintingEngine::numberOfGPUPaintingThreads()
{
    static unsigned count = [] {
        unsigned threads = WTF::numberOfProcessorCores() >= 4 ? 2 : 1;
        if (const char* variable = getenv("WEBKIT_SKIA_GPU_PAINTING_THREADS")) {
            auto value = parseInteger<unsigned>(StringView::fromLatin1(variable));
            if (value && *value >= 1 && *value <= 4)
                threads = *value;
        }
        return threads;
    }();
    return count;
}
bool SkiaPaintingEngine::shouldUseDMABufAtlasTextures() { return false; }
bool SkiaPaintingEngine::shouldUseLinearTileTextures() { return false; }
bool SkiaPaintingEngine::shouldUseVivanteSuperTiledTileTextures() { return false; }
bool SkiaPaintingEngine::isDDLEnabled() { return false; }

void SkiaPaintingEngine::paintIntoGraphicsContext(const GraphicsLayer& layer, GraphicsContext& context, const IntRect& dirtyRect, bool contentsOpaque, float contentsScale) const
{
    IntRect clip({ }, dirtyRect.size());
    context.clip(clip);
    if (!contentsOpaque) {
        context.setCompositeOperation(CompositeOperator::Copy);
        context.fillRect(clip, Color::transparentBlack);
        context.setCompositeOperation(CompositeOperator::SourceOver);
    }
    FloatRect contents(dirtyRect);
    contents.scale(1 / contentsScale);
    context.translate(-dirtyRect.x(), -dirtyRect.y());
    context.scale(contentsScale);
    layer.paintGraphicsLayerContents(context, contents);
}

Ref<CoordinatedTileBuffer> SkiaPaintingEngine::createBuffer(RenderingMode renderingMode, const IntSize& size, bool contentsOpaque) const
{
    if (renderingMode == RenderingMode::Accelerated) {
        // (A texture of the pool, taken with a context of the share group
        // current: the main thread's Skia one.)
        OptionSet<BitmapTexture::Flags> flags;
        if (!contentsOpaque)
            flags.add(BitmapTexture::Flags::SupportsAlpha);
        if (auto* context = PlatformDisplay::sharedDisplay().skiaGLContext(); context && context->makeContextCurrent())
            return CoordinatedAcceleratedTileBuffer::create(BitmapTexturePool::singleton().acquireTexture(size, flags));
    }
    return CoordinatedUnacceleratedTileBuffer::create(size, contentsOpaque ? CoordinatedTileBuffer::NoFlags : CoordinatedTileBuffer::SupportsAlpha);
}

Ref<SkiaRecordingResult> SkiaPaintingEngine::record(const GraphicsLayerCoordinated& layer, const IntRect& recordRect, bool contentsOpaque, float contentsScale, unsigned dirtyTilesCount)
{
    auto& host = MlxLayerTreeHost::of(layer);
    host.didPaintLayer();
    // (MLX_TRACE_LAYER_PAINTS: each recording on standard error, the
    // layer's name and the rectangle.)
    static bool trace = getenv("MLX_TRACE_LAYER_PAINTS");
    if (trace)
        fprintf(stderr, "layer paint %s %d,%d %dx%d\n", layer.name().utf8().data(), recordRect.x(), recordRect.y(), recordRect.width(), recordRect.height());
    SkPictureRecorder recorder;
    // (An R-tree when the recording is played back into several tiles, so
    // each takes only what falls in it.)
    SkRTreeFactory rtree;
    auto* canvas = recorder.beginRecording(recordRect.width(), recordRect.height(), dirtyTilesCount > 1 ? &rtree : nullptr);
    SkiaRecordingData data;
    if (host.paintsForcedDark()) {
        MlxDarkModeCanvas dark(*canvas);
        GraphicsContextSkia context(dark, RenderingMode::Unaccelerated, RenderingPurpose::LayerBacking);
        context.beginRecording(GraphicsContextSkia::RecordingMode::Tile);
        paintIntoGraphicsContext(layer, context, recordRect, contentsOpaque, contentsScale);
        data = context.endRecording();
    } else {
        GraphicsContextSkia context(*canvas, RenderingMode::Unaccelerated, RenderingPurpose::LayerBacking);
        context.beginRecording(GraphicsContextSkia::RecordingMode::Tile);
        paintIntoGraphicsContext(layer, context, recordRect, contentsOpaque, contentsScale);
        data = context.endRecording();
    }
    return SkiaRecordingResult::create(recorder.finishRecordingAsPicture(), WTF::move(data), recordRect, RenderingMode::Unaccelerated, contentsOpaque, contentsScale);
}

Ref<CoordinatedTileBuffer> SkiaPaintingEngine::replay(const GraphicsLayerCoordinated& layer, Ref<SkiaRecordingResult>&& recording, const IntRect& tileRect, const IntRect& dirtyRect)
{
    Ref platformLayer = layer.coordinatedPlatformLayer();
    platformLayer->willPaintTile();
    // (The buffer is the dirty rectangle's: the compositor copies it into
    // the tile at its place.)
    auto buffer = createBuffer(m_threadSafeGrContext ? RenderingMode::Accelerated : RenderingMode::Unaccelerated, dirtyRect.size(), recording->contentsOpaque());
    buffer->beginPainting();
    m_paintingWorkerPool->postTask([platformLayer = WTF::move(platformLayer), buffer = Ref { buffer }, dirtyRect, recording = WTF::move(recording)]() mutable {
        // (On the GPU: this thread's Skia GL context, made the first time.)
        if (buffer->isBackedByOpenGL() && !PlatformDisplay::sharedDisplay().skiaGLContext()) {
            buffer->completePainting();
            platformLayer->didPaintTile();
            return;
        }
        if (auto* canvas = buffer->canvas()) {
            canvas->save();
            canvas->clear(SkColors::kTransparent);
            canvas->clipRect(SkRect::MakeWH(dirtyRect.width(), dirtyRect.height()));
            canvas->translate(recording->recordRect().x() - dirtyRect.x(), recording->recordRect().y() - dirtyRect.y());
            recording->picture()->playback(canvas);
            canvas->restore();
        }
        buffer->completePainting();
        platformLayer->didPaintTile();
    });
    return buffer;
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
