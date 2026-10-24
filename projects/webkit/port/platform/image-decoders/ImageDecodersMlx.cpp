// The Mlx port's PNG, JPEG and WebP decoders: MlxImageDecoder with the
// format of each.
#include "config.h"

#include "JPEGImageDecoder.h"
#include "PNGImageDecoder.h"
#include "WEBPImageDecoder.h"
#include "port/MlxWebPort.h"

namespace WebCore {

PNGImageDecoder::PNGImageDecoder(AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
    : MlxImageDecoder(MLXWEB_IMAGE_PNG, "png"_s, alphaOption, gammaAndColorProfileOption)
{
}

JPEGImageDecoder::JPEGImageDecoder(AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
    : MlxImageDecoder(MLXWEB_IMAGE_JPEG, "jpeg"_s, alphaOption, gammaAndColorProfileOption)
{
}

WEBPImageDecoder::WEBPImageDecoder(AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
    : MlxImageDecoder(MLXWEB_IMAGE_WEBP, "webp"_s, alphaOption, gammaAndColorProfileOption)
{
}

} // namespace WebCore
