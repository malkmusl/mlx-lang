// The Mlx port's CoordinatedPlatformLayerBufferNativeImage
// (platform/graphics/texmap/coordinated, WebKit's header): an image a
// layer shows as its contents (an <img> composited directly, a canvas'
// image), handed to the compositor. WebKit's file (not compiled:
// lists/webcore-mlx-sources-apart.txt leaves it out) takes Ganesh's
// textures for granted; the port's images are raster: the CPU compositor
// draws the SkImage itself (SkiaCompositingLayer asks for skiaImage()),
// the GPU compositor uploads its pixels into a texture once
// (TextureMapper's path, as WebKit's file does for a raster image).
#include "config.h"
#include "CoordinatedPlatformLayerBufferNativeImage.h"

#if USE(COORDINATED_GRAPHICS) && USE(SKIA)
#include "BitmapTexture.h"
#include "BitmapTexturePool.h"
#include "CoordinatedPlatformLayerBufferRGB.h"
#include "GLFence.h"
#include "NativeImage.h"
#include "TextureMapperFlags.h"
#include <skia/core/SkPixmap.h>

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

// The GPU compositor's (TextureMapper's) texture of the image: its pixels
// uploaded once, as WebKit's file does for a raster image.
bool CoordinatedPlatformLayerBufferNativeImage::tryEnsureBuffer(UseSkiaForCompositing)
{
    if (m_buffer)
        return true;
    const auto& image = m_image->platformImage();
    SkPixmap pixmap;
    if (!image || !image->peekPixels(&pixmap))
        return false;
    OptionSet<BitmapTexture::Flags> textureFlags;
    if (m_image->hasAlpha())
        textureFlags.add(BitmapTexture::Flags::SupportsAlpha);
    if (pixmap.colorType() == kBGRA_8888_SkColorType)
        textureFlags.add(BitmapTexture::Flags::UseBGRALayout);
    auto texture = BitmapTexturePool::singleton().acquireTexture(m_size, textureFlags);
    texture->updateContents(pixmap.addr(), IntRect(IntPoint(), m_size), IntPoint(), pixmap.rowBytes(), PixelFormat::BGRA8);
    m_buffer = CoordinatedPlatformLayerBufferRGB::create(WTF::move(texture), m_flags, nullptr);
    return true;
}

void CoordinatedPlatformLayerBufferNativeImage::paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity)
{
    waitForContentsIfNeeded();
    if (!tryEnsureBuffer(UseSkiaForCompositing::No))
        return;
    m_buffer->paintToTextureMapper(textureMapper, targetRect, modelViewMatrix, opacity);
}

sk_sp<SkImage> CoordinatedPlatformLayerBufferNativeImage::skiaImage()
{
    return m_image ? m_image->platformImage() : nullptr;
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
