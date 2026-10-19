// The Mlx port's image decoders' common part (PORT.md, "Images"): a
// ScalableImageDecoder whose one frame std.png, std.jpeg or std.webp
// decodes through the port's Mlx side (MlxWebPort.h) once the whole file
// is there. PNGImageDecoder, JPEGImageDecoder and WEBPImageDecoder, the
// classes ScalableImageDecoder::create names, are this with their format.
//
// Not yet: decoding as the data comes in (the size and the rows known
// before the end of the file), the frames of APNG and animated WebP, and
// the files' colour profiles (std.colorprofile).
#pragma once

#include "ScalableImageDecoder.h"

namespace WebCore {

class MlxImageDecoder : public ScalableImageDecoder {
public:
    virtual ~MlxImageDecoder();

    String filenameExtension() const override { return m_extension; }

    // The decoded frame (the ICO decoder takes a PNG entry's so).
    ScalableImageDecoderFrame* firstFrameBuffer();

protected:
    MlxImageDecoder(uint32_t format, ASCIILiteral extension, AlphaOption, GammaAndColorProfileOption);

    ScalableImageDecoderFrame* frameBufferAtIndex(size_t) override;

private:
    void tryDecodeSize(bool allDataReceived) override;
    bool setFailed() override;
    void decodeWhole();
    void releaseDecoded();

    uint32_t m_format;
    ASCIILiteral m_extension;
    // The Mlx side's image between its decoding and its copy into the
    // frame (0 before and after).
    uintptr_t m_decoded { 0 };
};

} // namespace WebCore
