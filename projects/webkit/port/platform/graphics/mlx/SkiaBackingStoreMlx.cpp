// The Mlx port's SkiaBackingStore (platform/graphics/skia/SkiaBackingStore.h,
// WebKit's header): the compositor's side of a layer's tiles, which
// SkiaCompositingLayer draws. WebKit's SkiaBackingStore.cpp (not compiled:
// lists/webcore-mlx-skipped.txt) keeps each tile in a GL texture and draws
// it as a Ganesh image; the port composites on the CPU (PORT.md,
// "Compositing"), so a tile here is a raster surface the painted buffers
// (CoordinatedUnacceleratedTileBuffer, BGRA, painted by the port's
// SkiaPaintingEngine on its worker threads) are copied into on the
// compositor thread, and drawn as a raster image of it. The header's
// texture member stays empty.
#include "config.h"
#include "SkiaBackingStore.h"

#if USE(COORDINATED_GRAPHICS) && USE(SKIA)
#include "CoordinatedTileBuffer.h"
#include "SkiaDamageRegion.h"
#include "SkiaUtilities.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkColorSpace.h>
#include <skia/core/SkImage.h>
#include <skia/core/SkPixmap.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SkiaBackingStore);

void SkiaBackingStore::update(const FloatSize& size, float scale, CoordinatedBackingStoreProxy::Update&& update)
{
    m_size = size;
    m_scale = scale;
    for (auto tileID : update.tilesToCreate())
        m_tiles.add(tileID, Tile(m_scale));
    for (auto tileID : update.tilesToRemove())
        m_tiles.remove(tileID);
    for (const auto& tileUpdate : update.tilesToUpdate()) {
        auto found = m_tiles.find(tileUpdate.tileID);
        if (found == m_tiles.end())
            continue;
        found->value.scheduleUpdate(tileUpdate.dirtyRect, tileUpdate.tileRect, tileUpdate.buffer);
        m_hasPendingTileUpdates = true;
    }
}

void SkiaBackingStore::processPendingTileUpdates()
{
    if (!m_hasPendingTileUpdates)
        return;
    for (auto& tile : m_tiles.values())
        tile.processPendingUpdateIfNeeded();
    m_hasPendingTileUpdates = false;
    m_hasPaddedTiles = false;
}

SkSamplingOptions SkiaBackingStore::samplingOptionsForMatrix(const SkMatrix& deviceMatrix) const
{
    if (!deviceMatrix.isScaleTranslate())
        return SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kNone);
    // (The tiles hold m_scale device pixels a layer pixel: what is left of
    // the matrix past that decides.)
    SkMatrix matrix = deviceMatrix;
    matrix.preScale(1 / m_scale, 1 / m_scale);
    return SkiaUtilities::samplingOptionsForMatrix(matrix);
}

// Whether TILE touches every edge of the layer (LAYER): only then are its
// edges the layer's, to be antialiased; a tile's inner edges meet others.
static bool touchesAllEdges(const FloatRect& layer, const FloatRect& tile)
{
    return !tile.x() && !tile.y() && tile.maxX() >= layer.width() && tile.maxY() >= layer.height();
}

// TILE (its rectangle in the layer) and SOURCE (in its image) cut to what
// CLIP (a little outset, so the cut edge lies outside what shows) leaves;
// false when nothing is left.
static bool cutToClip(const SkRect& clip, SkRect& tile, SkRect& source)
{
    SkRect cut = tile;
    if (!cut.intersect(clip.makeOutset(1, 1)))
        return false;
    if (cut == tile)
        return true;
    float scaleX = source.width() / tile.width();
    float scaleY = source.height() / tile.height();
    source = SkRect::MakeLTRB(source.fLeft + (cut.fLeft - tile.fLeft) * scaleX, source.fTop + (cut.fTop - tile.fTop) * scaleY,
        source.fLeft + (cut.fRight - tile.fLeft) * scaleX, source.fTop + (cut.fBottom - tile.fTop) * scaleY);
    tile = cut;
    return true;
}

void SkiaBackingStore::paintToCanvas(SkCanvas& canvas, const SkPaint& paint, const SkiaDamageRegion* damageRegion)
{
    if (m_tiles.isEmpty())
        return;
    FloatRect layer { { }, m_size };
    auto matrix = canvas.getLocalToDeviceAs3x3();
    auto sampling = samplingOptionsForMatrix(matrix);
    auto constraint = requiresStrictSourceConstraint(sampling) ? SkCanvas::kStrict_SrcRectConstraint : SkCanvas::kFast_SrcRectConstraint;
    auto clip = canvas.getLocalClipBounds();
    SkPaint tilePaint = paint;
    for (auto& tile : m_tiles.values()) {
        if (canvas.quickReject(tile.rect()))
            continue;
        // (The damage, not only the clip's bounds: two damaged rectangles
        // far apart make a clip as large as the frame.)
        if (damageRegion && !damageRegion->intersects(matrix.mapRect(tile.rect())))
            continue;
        auto image = tile.image();
        if (!image)
            continue;
        SkRect destination = tile.rect();
        SkRect source = tile.imageSourceRect();
        if (!cutToClip(clip, destination, source))
            continue;
        tilePaint.setAntiAlias(paint.isAntiAlias() && touchesAllEdges(layer, tile.rect()));
        canvas.drawImageRect(image, source, destination, sampling, &tilePaint, constraint);
    }
}

void SkiaBackingStore::appendImageSetEntries(SkCanvas& canvas, const SkMatrix& ctm, size_t matrixIndex, float opacity, bool enableAntialias, Vector<SkCanvas::ImageSetEntry>& images, const SkiaDamageRegion* damageRegion) const
{
    if (m_tiles.isEmpty())
        return;
    SkMatrix inverse;
    if (damageRegion && !ctm.invert(&inverse))
        return;
    FloatRect layer { { }, m_size };
    SkAutoCanvasRestore restore(&canvas, true);
    canvas.concat(ctm);
    auto clip = canvas.getLocalClipBounds();
    for (auto& tile : m_tiles.values()) {
        if (canvas.quickReject(tile.rect()))
            continue;
        auto image = tile.image();
        if (!image)
            continue;
        unsigned edges = enableAntialias && touchesAllEdges(layer, tile.rect()) ? SkCanvas::kAll_QuadAAFlags : SkCanvas::kNone_QuadAAFlags;
        SkRect destination = tile.rect();
        SkRect source = tile.imageSourceRect();
        if (!cutToClip(clip, destination, source))
            continue;
        if (!damageRegion) {
            images.append(SkCanvas::ImageSetEntry(image, source, destination, matrixIndex, opacity, edges, false));
            continue;
        }
        auto device = ctm.mapRect(destination);
        if (!damageRegion->intersects(device))
            continue;
        damageRegion->forEachDamagedSubRect(device, destination, source, inverse, [&](const SkRect& subSource, const SkRect& subDestination) {
            images.append(SkCanvas::ImageSetEntry(image, subSource, subDestination, matrixIndex, opacity, edges, false));
        });
    }
}

void SkiaBackingStore::drawDebugBorders(SkCanvas& canvas, const SkPaint& paint)
{
    for (const auto& tile : m_tiles.values())
        canvas.drawRect(SkRect(tile.rect()), paint);
}

void SkiaBackingStore::Tile::scheduleUpdate(const IntRect& dirtyRect, const IntRect& tileRect, CoordinatedTileBuffer& buffer)
{
    m_pendingUpdates.append({ tileRect, dirtyRect, Ref { buffer } });
}

void SkiaBackingStore::Tile::processPendingUpdateIfNeeded()
{
    for (auto& pending : m_pendingUpdates)
        update(pending.dirtyRect, pending.tileRect, pending.buffer.get());
    m_pendingUpdates.clear();
}

void SkiaBackingStore::Tile::ensureTexture(const IntSize& size, CoordinatedTileBuffer& buffer)
{
    // (A raster surface of the tile's size, opaque when the buffer is.)
    auto alpha = buffer.supportsAlpha() ? kPremul_SkAlphaType : kOpaque_SkAlphaType;
    if (m_surface && m_surface->width() == size.width() && m_surface->height() == size.height() && m_surface->imageInfo().alphaType() == alpha)
        return;
    auto info = SkImageInfo::Make(size.width(), size.height(), kBGRA_8888_SkColorType, alpha, SkColorSpace::MakeSRGB());
    m_surface = SkSurfaces::Raster(info);
    if (m_surface)
        m_surface->getCanvas()->clear(SkColors::kTransparent);
    m_cachedImage = nullptr;
}

// A painted buffer (the tile's DIRTYRECT, from the tile's origin; the tile
// at TILERECT of the layer, in device pixels) copied into the tile.
void SkiaBackingStore::Tile::update(const IntRect& dirtyRect, const IntRect& tileRect, CoordinatedTileBuffer& buffer)
{
    FloatRect rect(tileRect);
    rect.scale(1 / m_scale);
    if (rect != m_rect) {
        m_rect = rect;
        m_surface = nullptr;
        m_cachedImage = nullptr;
    }
    if (buffer.isBackedByOpenGL())
        return;
    buffer.waitUntilPaintingComplete();
    ensureTexture(tileRect.size(), buffer);
    if (!m_surface)
        return;
    auto& painted = static_cast<CoordinatedUnacceleratedTileBuffer&>(buffer);
    auto size = painted.size();
    auto info = SkImageInfo::Make(size.width(), size.height(), kBGRA_8888_SkColorType, buffer.supportsAlpha() ? kPremul_SkAlphaType : kOpaque_SkAlphaType, SkColorSpace::MakeSRGB());
    SkPixmap pixels(info, painted.data(), painted.stride());
    // (writePixels copies the pixels: the image of the surface taken before
    // keeps the old ones, so it is let go.)
    m_cachedImage = nullptr;
    m_surface->writePixels(pixels, dirtyRect.x(), dirtyRect.y());
}

sk_sp<SkImage> SkiaBackingStore::Tile::image() const
{
    if (!m_surface)
        return nullptr;
    if (!m_cachedImage)
        m_cachedImage = m_surface->makeImageSnapshot();
    return m_cachedImage;
}

SkRect SkiaBackingStore::Tile::imageSourceRect() const
{
    if (!m_surface)
        return SkRect::MakeEmpty();
    return SkRect::MakeWH(m_surface->width(), m_surface->height());
}

bool SkiaBackingStore::Tile::isPadded() const
{
    return false;
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
