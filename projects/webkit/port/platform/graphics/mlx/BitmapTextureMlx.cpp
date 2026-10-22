// The Mlx port's BitmapTexture: texmap's GL texture, which WebCore's
// Skia sources name for their accelerated paths (SkiaGPUAtlas uploads its
// images into one) though the port has no GL (texmap/BitmapTexture.cpp
// is GL's, not compiled: PORT.md, "GL in the Skia sources"). No
// BitmapTexture is ever made: there is no display (PlatformDisplayMlx.cpp),
// so nothing is accelerated.
//
// STUB (M8 and after: textures of Skia's Ganesh over std.gpu): the
// functions WebCore's code names, which nothing reaches.
#include "config.h"
#include "BitmapTexture.h"

namespace WebCore {

BitmapTexture::~BitmapTexture() = default;

void BitmapTexture::updateContents(const void*, const IntRect&, const IntPoint&, int, PixelFormat)
{
    RELEASE_ASSERT_NOT_REACHED();
}

GrBackendTexture BitmapTexture::createSkiaBackendTexture() const
{
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}

} // namespace WebCore
