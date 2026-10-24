// The Mlx port's CoordinatedPlatformLayerBufferNativeImage
// (platform/graphics/texmap/coordinated, WebKit's header): an image a
// layer shows as its contents (an <img> composited directly, a canvas'
// image), handed to the compositor. WebKit's file (not compiled:
// lists/webcore-mlx-sources-apart.txt leaves it out) uploads it into a GL
// texture; the port's compositor draws the image itself, a raster
// SkImage (SkiaCompositingLayer asks for skiaImage()).
#include "config.h"
#include "CoordinatedPlatformLayerBufferNativeImage.h"

#if USE(COORDINATED_GRAPHICS) && USE(SKIA)
#include "GLFence.h"
#include "NativeImage.h"
#include "TextureMapperFlags.h"

namespace WebCore {

std::unique_ptr<CoordinatedPlatformLayerBufferNativeImage> CoordinatedPlatformLayerBufferNativeImage::create(Ref<NativeImage>&& image, std::unique_ptr<GLFence>&& fence)
{
    OptionSet<TextureMapperFlags> flags;
    if (image->hasAlpha())
        flags.add(TextureMapperFlags::ShouldBlend);
    return makeUnique<CoordinatedPlatformLayerBufferNativeImage>(WTF::move(image), flags, WTF::move(fence));
}

CoordinatedPlatformLayerBufferNativeImage::CoordinatedPlatformLayerBufferNativeImage(Ref<NativeImage>&& image, OptionSet<TextureMapperFlags> flags, std::unique_ptr<GLFence>&& fence)
    : CoordinatedPlatformLayerBuffer(Type::NativeImage, image->size(), flags, WTF::move(fence))
    , m_image(WTF::move(image))
{
}

CoordinatedPlatformLayerBufferNativeImage::~CoordinatedPlatformLayerBufferNativeImage() = default;

bool CoordinatedPlatformLayerBufferNativeImage::tryEnsureBuffer(UseSkiaForCompositing)
{
    return false;
}

// STUB (M8: TextureMapper's path, which the port's compositor never takes).
void CoordinatedPlatformLayerBufferNativeImage::paintToTextureMapper(TextureMapper&, const FloatRect&, const TransformationMatrix&, float)
{
    RELEASE_ASSERT_NOT_REACHED();
}

sk_sp<SkImage> CoordinatedPlatformLayerBufferNativeImage::skiaImage()
{
    return m_image ? m_image->platformImage() : nullptr;
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
