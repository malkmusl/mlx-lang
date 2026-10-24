// The Mlx port's image encoding (canvas's toDataURL and toBlob, the
// clipboard's images): ImageUtilities.cpp's platformEncodeData, which
// ImageUtilitiesSkia.cpp defines over Skia's encoders (libpng, libjpeg,
// libwebp; not compiled, lists/webcore-mlx-skipped.txt).
//
// STUB (M4: std.png, std.jpeg and std.webp's encoders through the Mlx
// side): nothing is encoded yet; an image encodes to no data, which
// WebCore takes as a failure (toDataURL gives "data:,").
#include "config.h"
#include "ImageUtilities.h"

#include "NativeImage.h"

namespace WebCore {

Vector<uint8_t> platformEncodeData(const NativeImage&, const String&, std::optional<double>)
{
    return { };
}

} // namespace WebCore
