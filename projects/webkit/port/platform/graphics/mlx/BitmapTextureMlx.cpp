// The Mlx port's BitmapTexture: texmap's GL texture, which WebCore's
// Skia sources name for their accelerated paths (SkiaGPUAtlas uploads its
// images into one) though the port has no GL (texmap/BitmapTexture.cpp
// is GL's, not compiled: PORT.md, "GL in the Skia sources"). No
// BitmapTexture is ever made: there is no display (PlatformDisplayMlx.cpp),
// so nothing is accelerated.
//
// STUB (after M8: textures of Skia's Ganesh over std.gpu): the
// functions WebCore's code names, which nothing reaches (the compositor's
// tiles are raster images: SkiaBackingStoreMlx.cpp), BitmapTexturePool's
// among them.
#include "config.h"
#include "BitmapTexture.h"
#include "BitmapTexturePool.h"

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

sk_sp<SkSurface> BitmapTexture::createSkiaSurface(GrDirectContext*, GrSurfaceOrigin, unsigned) const { RELEASE_ASSERT_NOT_REACHED(); }
void BitmapTexture::swapTexture(BitmapTexture&) { RELEASE_ASSERT_NOT_REACHED(); }
void BitmapTexture::reset(const IntSize&, OptionSet<Flags>) { RELEASE_ASSERT_NOT_REACHED(); }
void BitmapTexture::copyFromExternalTexture(GLuint, const IntRect&, const IntSize&) { RELEASE_ASSERT_NOT_REACHED(); }

BitmapTexturePool& BitmapTexturePool::singleton() { RELEASE_ASSERT_NOT_REACHED(); }
Ref<BitmapTexture> BitmapTexturePool::acquireTexture(const IntSize&, OptionSet<BitmapTexture::Flags>) { RELEASE_ASSERT_NOT_REACHED(); }

} // namespace WebCore
