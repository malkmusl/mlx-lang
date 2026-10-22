// The Mlx port's WEBPImageDecoder (PORT.md, "Images"): the class
// ScalableImageDecoder::create names, found before WebKit's
// (platform/image-decoders/webp, libwebp's, is not compiled); its WebP
// images decoded by std.webp (an animation's first frame for now).
#pragma once

#include "MlxImageDecoder.h"

namespace WebCore {

class WEBPImageDecoder final : public MlxImageDecoder {
public:
    static Ref<ScalableImageDecoder> create(AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
    {
        return adoptRef(*new WEBPImageDecoder(alphaOption, gammaAndColorProfileOption));
    }

private:
    WEBPImageDecoder(AlphaOption, GammaAndColorProfileOption);
};

} // namespace WebCore
