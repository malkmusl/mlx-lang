// The Mlx port's ScrollerCoordinated (page/scrolling/coordinated/
// ScrollerCoordinated.h, WebKit's header): a scrollbar of an
// asynchronously scrolled area drawn by the scrolling thread (Adwaita's
// painter, ScrollerImpAdwaita), so that its thumb follows a scroll the
// main thread has not seen yet. WebKit's ScrollerCoordinated.cpp (not
// compiled: lists/webcore-mlx-sources-apart.txt leaves it out) draws it
// into a GL texture and gives up without a GL context; the port draws it
// on the CPU into a raster image, the scrollbar layer's contents
// (CoordinatedPlatformLayerBufferSkiaImage), which the compositor draws
// as any image.
#include "config.h"
#include "ScrollerCoordinated.h"

#if USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
#include "CoordinatedPlatformLayer.h"
#include "CoordinatedPlatformLayerBufferSkiaImage.h"
#include "GraphicsContextSkia.h"
#include "ScrollerImpAdwaita.h"
#include "TextureMapperFlags.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkImage.h>
#include <skia/core/SkSurface.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ScrollerPairCoordinated);

ScrollerCoordinated::ScrollerCoordinated(ScrollerPairCoordinated& pair, ScrollbarOrientation orientation)
    : m_pair(pair)
    , m_orientation(orientation)
{
    m_state.orientation = m_orientation;
    m_state.pressedPart = NoPart;
}

ScrollerCoordinated::~ScrollerCoordinated() = default;

void ScrollerCoordinated::setScrollerImp(ScrollerImpAdwaita* scrollerImp)
{
    Locker locker { m_lock };
    m_scrollerImp = scrollerImp;
    m_needsUpdate = true;
}

void ScrollerCoordinated::setHostLayer(CoordinatedPlatformLayer* layer)
{
    Locker locker { m_lock };
    m_hostLayer = layer;
    m_needsUpdate = true;
}

// The scrollbar drawn again when the scrolled area's values (its visible
// size, its proportion, its position) or the scrollbar's state changed.
void ScrollerCoordinated::updateValues()
{
    RefPtr<CoordinatedPlatformLayer> hostLayer;
    RefPtr<ScrollerImpAdwaita> scrollerImp;
    {
        Locker locker { m_lock };
        scrollerImp = m_scrollerImp;
        hostLayer = m_hostLayer;
    }
    if (!hostLayer)
        return;
    if (!scrollerImp) {
        // (A custom scrollbar: RenderScrollbar paints it.)
        Locker layerLocker { hostLayer->lock() };
        hostLayer->setContentsBuffer(nullptr);
        return;
    }
    RefPtr pair = m_pair.get();
    if (!pair)
        return;
    auto values = pair->valuesForOrientation(m_orientation);
    AdwaitaScrollbarPainter::State state;
    {
        Locker locker { m_lock };
        if (m_currentValue != values)
            m_needsUpdate = true;
        m_currentValue = values;
        if (!m_needsUpdate)
            return;
        m_needsUpdate = false;
        state = m_state;
    }
    IntRect rect { 0, 0, AdwaitaScrollbarPainter::scrollbarSize, static_cast<int>(values.visibleSize) };
    if (rect.isEmpty())
        return;
    if (m_orientation == ScrollbarOrientation::Horizontal)
        rect = rect.transposedRect();
    // (Adwaita's thumb: its proportion of the track, not shorter than
    // its minimum; none when the track is shorter than that.)
    if (values.visibleSize <= AdwaitaScrollbarPainter::minimumThumbSize)
        state.thumbLength = 0;
    else
        state.thumbLength = std::max<int>(values.visibleSize * values.proportion, AdwaitaScrollbarPainter::minimumThumbSize);
    state.thumbPosition = (values.visibleSize - state.thumbLength) * values.value;
    state.frameRect = rect;

    float contentsScale;
    {
        Locker layerLocker { hostLayer->lock() };
        contentsScale = hostLayer->contentsScale();
    }
    auto pixels = enclosingIntRect(FloatRect(rect.x() * contentsScale, rect.y() * contentsScale, rect.width() * contentsScale, rect.height() * contentsScale));
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(pixels.width(), pixels.height()));
    if (!surface)
        return;
    surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    {
        GraphicsContextSkia context(*surface->getCanvas(), RenderingMode::Unaccelerated, RenderingPurpose::DOM);
        context.scale(contentsScale);
        scrollerImp->paint(context, state.frameRect, state);
    }
    auto buffer = makeUnique<CoordinatedPlatformLayerBufferSkiaImage>(surface->makeImageSnapshot(), OptionSet<TextureMapperFlags> { TextureMapperFlags::ShouldBlend });
    Locker layerLocker { hostLayer->lock() };
    hostLayer->setContentsRect(state.frameRect);
    hostLayer->setContentsClippingRect(FloatRoundedRect(state.frameRect));
    hostLayer->setContentsBuffer(WTF::move(buffer));
}

void ScrollerCoordinated::setHoveredAndPressedParts(ScrollbarPart hoveredPart, ScrollbarPart pressedPart)
{
    Locker locker { m_lock };
    m_state.hoveredPart = hoveredPart;
    m_state.pressedPart = pressedPart;
    m_needsUpdate = true;
}

void ScrollerCoordinated::setEnabled(bool enabled)
{
    Locker locker { m_lock };
    m_state.enabled = enabled;
    m_needsUpdate = true;
}

void ScrollerCoordinated::setOverlayScrollbarEnabled(bool enabled)
{
    Locker locker { m_lock };
    m_state.usesOverlayScrollbars = enabled;
    m_needsUpdate = true;
}

void ScrollerCoordinated::setUseDarkAppearance(bool isDark)
{
    Locker locker { m_lock };
    m_state.useDarkAppearanceForScrollbars = isDark;
    m_needsUpdate = true;
}

void ScrollerCoordinated::setOpacity(float opacity)
{
    Locker locker { m_lock };
    m_state.opacity = opacity;
    m_needsUpdate = true;
}

void ScrollerCoordinated::setScrollbarColor(const std::optional<ScrollbarColor>& scrollbarColor)
{
    Locker locker { m_lock };
    m_state.scrollbarColor = scrollbarColor;
    m_needsUpdate = true;
}

void ScrollerCoordinated::setScrollbarLayoutDirection(UserInterfaceLayoutDirection direction)
{
    Locker locker { m_lock };
    m_state.shouldPlaceVerticalScrollbarOnLeft = direction == UserInterfaceLayoutDirection::RTL;
    m_needsUpdate = true;
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
