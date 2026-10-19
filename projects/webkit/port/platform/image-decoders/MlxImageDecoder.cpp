// The Mlx port's image decoders (MlxImageDecoder.h).
#include "config.h"
#include "MlxImageDecoder.h"

#include "port/MlxWebPort.h"

namespace WebCore {

MlxImageDecoder::MlxImageDecoder(uint32_t format, ASCIILiteral extension, AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
    : ScalableImageDecoder(alphaOption, gammaAndColorProfileOption)
    , m_format(format)
    , m_extension(extension)
{
}

MlxImageDecoder::~MlxImageDecoder()
{
    releaseDecoded();
}

void MlxImageDecoder::releaseDecoded()
{
    if (!m_decoded)
        return;
    mlxweb_image_destroy(m_decoded);
    m_decoded = 0;
}

bool MlxImageDecoder::setFailed()
{
    releaseDecoded();
    return ScalableImageDecoder::setFailed();
}

// The size is known when the whole image is decoded: std's decoders take
// a whole file. Until the last data comes the image has no size, and
// WebCore waits for it.
void MlxImageDecoder::tryDecodeSize(bool allDataReceived)
{
    if (!allDataReceived || failed())
        return;
    decodeWhole();
}

void MlxImageDecoder::decodeWhole()
{
    if (m_decoded || !m_data)
        return;
    auto bytes = m_data->span();
    m_decoded = mlxweb_image_decode(m_format, bytes.data(), bytes.size());
    if (!m_decoded) {
        setFailed();
        return;
    }
    IntSize size(static_cast<int>(mlxweb_image_width(m_decoded)), static_cast<int>(mlxweb_image_height(m_decoded)));
    m_orientation = ImageOrientation(static_cast<int>(mlxweb_image_orientation(m_decoded)));
    if (!setSize(size))
        return;
}

ScalableImageDecoderFrame* MlxImageDecoder::frameBufferAtIndex(size_t index)
{
    if (index)
        return nullptr;
    if (m_frameBufferCache.isEmpty())
        m_frameBufferCache.grow(1);
    auto& frame = m_frameBufferCache[0];
    if (frame.isComplete() || failed())
        return &frame;
    if (!m_decoded)
        decodeWhole();
    if (!m_decoded)
        return &frame;

    size_t width = mlxweb_image_width(m_decoded);
    size_t height = mlxweb_image_height(m_decoded);
    if (!frame.initialize(size(), m_premultiplyAlpha)) {
        setFailed();
        return &frame;
    }
    const uint8_t* pixels = mlxweb_image_pixels(m_decoded);
    auto* backingStore = frame.backingStore();
    bool hasAlpha = false;
    for (size_t y = 0; y < height; ++y) {
        const uint8_t* row = pixels + y * width * 4;
        for (size_t x = 0; x < width; ++x) {
            const uint8_t* pixel = row + x * 4;
            if (pixel[3] != 255)
                hasAlpha = true;
            backingStore->setPixel(static_cast<int>(x), static_cast<int>(y), pixel[0], pixel[1], pixel[2], pixel[3]);
        }
    }
    frame.setHasAlpha(hasAlpha);
    frame.setOrientation(m_orientation);
    frame.setDecodingStatus(DecodingStatus::Complete);
    releaseDecoded();
    return &frame;
}

ScalableImageDecoderFrame* MlxImageDecoder::firstFrameBuffer()
{
    Locker locker { m_lock };
    return frameBufferAtIndex(0);
}

} // namespace WebCore
