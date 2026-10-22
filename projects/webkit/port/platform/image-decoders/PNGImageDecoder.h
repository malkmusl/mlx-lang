// The Mlx port's PNGImageDecoder (PORT.md, "Images"): the class
// ScalableImageDecoder::create and the ICO decoder name, found before
// WebKit's (platform/image-decoders/png, libpng's, is not compiled); its
// PNGs decoded by std.png.
#pragma once

#include "MlxImageDecoder.h"

namespace WebCore {

class PNGImageDecoder final : public MlxImageDecoder {
public:
    static Ref<ScalableImageDecoder> create(AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
    {
        return adoptRef(*new PNGImageDecoder(alphaOption, gammaAndColorProfileOption));
    }

private:
    PNGImageDecoder(AlphaOption, GammaAndColorProfileOption);
};

} // namespace WebCore
