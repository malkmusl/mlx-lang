// The Mlx port's JPEGImageDecoder (PORT.md, "Images"): the class
// ScalableImageDecoder::create names, found before WebKit's
// (platform/image-decoders/jpeg, libjpeg's, is not compiled); its JPEGs
// decoded by std.jpeg, with their EXIF orientation.
#pragma once

#include "MlxImageDecoder.h"

namespace WebCore {

class JPEGImageDecoder final : public MlxImageDecoder {
public:
    static Ref<ScalableImageDecoder> create(AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
    {
        return adoptRef(*new JPEGImageDecoder(alphaOption, gammaAndColorProfileOption));
    }

private:
    JPEGImageDecoder(AlphaOption, GammaAndColorProfileOption);
};

} // namespace WebCore
