// The desktop's palettes (std.ui.theme's light and dark, through
// libmlxwebport.so's mlxweb_theme_color) for the port's theme: the page's
// system colours (port/rendering/RenderThemeMlx.cpp) and its form controls
// (ControlFactoryMlx.cpp), so a page's buttons, fields and selection look
// like the desktop's apps.
#pragma once

#include "Color.h"

namespace WebCore {

// std.ui.theme's Theme's colours, in their order (MLXWEB_THEME_* of
// projects/webkit/port/mlx/api/MlxWebPort.mlx).
enum class MlxThemeColor : uint8_t {
    Background, Panel, Raised, Field, Hover, Pressed, Selected, Divider, Border,
    Text, Dim, Disabled, OnAccent, Accent, Danger, Control, ControlInner,
    Button, ButtonHover, Primary, PrimaryHover, SwitchOff, SwitchOffHover, Knob,
    Track, Thumb, Indicator, Selection, Caret,
};

// The colour of the dark palette (DARK) or the light one.
Color mlxThemeColor(MlxThemeColor, bool dark);

// The text selection's background: the accent, translucent (std.ui's
// selection colour over the page's canvas, light and dark; what is under
// it shows through, images too).
Color mlxSelectionBackground();

} // namespace WebCore
