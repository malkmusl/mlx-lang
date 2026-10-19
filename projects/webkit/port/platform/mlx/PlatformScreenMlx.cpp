// The Mlx port's PlatformScreen.h functions (PORT.md, as
// PlatformScreenPlayStation.cpp is PlayStation's): the screen the
// embedder described (PlatformScreenMlx.h), sRGB, no HDR.
#include "config.h"
#include "PlatformScreen.h"

#include "DestinationColorSpace.h"
#include "PlatformScreenMlx.h"
#include "Widget.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static MlxScreen& screen()
{
    static NeverDestroyed<MlxScreen> screen;
    return screen;
}

void setMlxScreen(const MlxScreen& description)
{
    screen() = description;
}

const MlxScreen& mlxScreen()
{
    return screen();
}

int screenDepth(Widget*)
{
    return screen().depth;
}

int screenDepthPerComponent(Widget*)
{
    return screen().depthPerComponent;
}

bool screenIsMonochrome(Widget* widget)
{
    return screenDepth(widget) < 2;
}

bool screenHasInvertedColors()
{
    return false;
}

double screenDPI(PlatformDisplayID)
{
    return screen().dpi;
}

double fontDPI()
{
    return screen().dpi;
}

FloatRect screenRect(Widget* widget)
{
    if (!screen().rect.isEmpty())
        return screen().rect;
    if (!widget)
        return { };
    return widget->boundsRect();
}

FloatRect screenAvailableRect(Widget* widget)
{
    if (!screen().availableRect.isEmpty())
        return screen().availableRect;
    return screenRect(widget);
}

DestinationColorSpace screenColorSpace(Widget*)
{
    return DestinationColorSpace::SRGB();
}

bool screenSupportsExtendedColor(Widget*)
{
    return false;
}

} // namespace WebCore
