// The Mlx port's SkiaPaintingEngine (platform/graphics/skia/
// SkiaPaintingEngine.h, WebKit's header): how a composited layer's dirty
// tiles are painted. As WebKit's own engine does on the CPU, the layer's
// contents are recorded on the main thread (GraphicsContextSkia over an
// SkPictureRecorder: the main thread's paint) and the recording is played
// back into each dirty tile's buffer (CoordinatedUnacceleratedTileBuffer)
// on worker threads, which the compositor copies into its tiles
// (SkiaBackingStoreMlx.cpp). WebKit's SkiaPaintingEngine.cpp (not compiled:
// lists/webcore-mlx-skipped.txt) also has the GPU's paths (atlases of
// images in GL textures, deferred display lists); the port has no GPU.
//
// Forced dark mode (PORT.md, "Forced dark mode") is applied here, to the
// layers' contents as they are recorded (MlxDarkModeCanvas in front of the
// recorder), not when the layers are composited: images stay as they are,
// and a layer's colours are darkened once, not each frame. Each recording
// is counted for the page's host (MlxLayerTreeHost::didPaintLayer): the
// paints Voyager traces.
#include "config.h"
#include "SkiaPaintingEngine.h"

#if USE(COORDINATED_GRAPHICS) && USE(SKIA)
#include "CoordinatedPlatformLayer.h"
#include "CoordinatedTileBuffer.h"
#include "GraphicsContextSkia.h"
#include "GraphicsLayerCoordinated.h"
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

unsigned SkiaPaintingEngine::numberOfGPUPaintingThreads() { return 0; }
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

Ref<CoordinatedTileBuffer> SkiaPaintingEngine::createBuffer(RenderingMode, const IntSize& size, bool contentsOpaque) const
{
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
    auto buffer = createBuffer(RenderingMode::Unaccelerated, dirtyRect.size(), recording->contentsOpaque());
    buffer->beginPainting();
    m_paintingWorkerPool->postTask([platformLayer = WTF::move(platformLayer), buffer = Ref { buffer }, dirtyRect, recording = WTF::move(recording)]() mutable {
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
