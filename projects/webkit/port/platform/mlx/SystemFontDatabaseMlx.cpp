// The Mlx port's system font (CSS's `font: caption` and the other system
// font keywords): the standard family at 16px, normal weight, as the WPE
// port gives it when GLib's SystemSettings name no font.
//
// STUB (M5: the desktop's interface font, from MLX's settings through the
// Mlx side): the system font is the page's standard family.
#include "config.h"
#include "SystemFontDatabase.h"

#include "WebKitFontFamilyNames.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

SystemFontDatabase& SystemFontDatabase::singleton()
{
    static NeverDestroyed<SystemFontDatabase> database = SystemFontDatabase();
    return database.get();
}

auto SystemFontDatabase::platformSystemFontShorthandInfo(FontShorthand) -> SystemFontShorthandInfo
{
    return { WebKitFontFamilyNames::standardFamily, 16, normalWeightValue() };
}

void SystemFontDatabase::platformInvalidate()
{
}

} // namespace WebCore
