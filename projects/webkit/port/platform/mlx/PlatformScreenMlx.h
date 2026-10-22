// The screen the Mlx port's pages are shown on, as the embedder describes
// it (Voyager, from the compositor's output: wl_output's mode and scale).
// PlatformScreen.h's functions answer from it.
#pragma once

#include "FloatRect.h"

namespace WebCore {

struct MlxScreen {
    // The output's size and the part windows may use, in CSS pixels;
    // empty: the widget's own size, as a view has no other.
    FloatRect rect;
    FloatRect availableRect;
    int depth { 24 };
    int depthPerComponent { 8 };
    double dpi { 96 };
};

WEBCORE_EXPORT void setMlxScreen(const MlxScreen&);
const MlxScreen& mlxScreen();

} // namespace WebCore
