// Skia's hottest raster routines for the port (PORT.md, "Compositing").
// Skia is built with its portable paths (projects/webkit/skia.mlx: mlx-cc
// has no vector registers), where a row blend or a fill goes through
// SkVx's vectors as arrays: each lane of each operation a call of
// templates nested a few levels deep. A translucent box composited each
// frame took 65 ms of an 800x600 frame that way (the compositor's profile:
// `mlx-profile --thread MlxCompositor`, in SkOpts's blit_row_color32).
// Skia chooses these routines at run time through the function pointers
// of SkOpts (SkOpts.h: what its SSE, AVX and NEON versions replace); the
// port sets them, once, to the same arithmetic on plain integers: two
// channels a multiplication (SkColorPriv.h's SkAlphaMulQ), whole words
// stored. The results are the portable routines', bit for bit.
#include "config.h"
#include "MlxSkiaRoutines.h"

#if USE(SKIA)
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include "src/core/SkBlitRow.h"
#include "src/core/SkColorPriv.h"
#include "src/core/SkMemset.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <cstring>

namespace WebCore {

static constexpr uint32_t channelMask = 0x00FF00FF;

// Each channel of PIXEL times SCALE (1 to 256) / 256.
static inline uint32_t scaled(uint32_t pixel, uint32_t scale)
{
    uint32_t redBlue = (((pixel & channelMask) * scale) >> 8) & channelMask;
    uint32_t alphaGreen = (((pixel >> 8) & channelMask) * scale) & ~channelMask;
    return redBlue | alphaGreen;
}

// A premultiplied COLOR over COUNT pixels (SkBlitRow_opts.h's
// blit_row_color32: the pixel scaled by 256 - alpha's complement, the colour
// added).
static void blitRowColor32(SkPMColor* destination, int count, SkPMColor color)
{
    uint32_t scale = 256 - SkGetPackedA32(color);
    for (int i = 0; i < count; ++i)
        destination[i] = scaled(destination[i], scale) + color;
}

// Premultiplied SOURCE pixels over DESTINATION's (SkPMSrcOver).
static void blitRowS32AOpaque(SkPMColor* destination, const SkPMColor* source, int count, U8CPU)
{
    for (int i = 0; i < count; ++i) {
        uint32_t pixel = source[i];
        uint32_t alpha = pixel >> 24;
        if (alpha == 255)
            destination[i] = pixel;
        else if (pixel)
            destination[i] = pixel + scaled(destination[i], 256 - alpha);
    }
}

template<typename T>
static void fill(T* buffer, T value, int count)
{
    if (count <= 0)
        return;
    // (Bytes all the same: memset.)
    bool sameBytes = true;
    for (size_t i = 1; i < sizeof(T); ++i)
        sameBytes &= static_cast<uint8_t>(value >> (8 * i)) == static_cast<uint8_t>(value);
    if (sameBytes) {
        memset(buffer, static_cast<uint8_t>(value), static_cast<size_t>(count) * sizeof(T));
        return;
    }
    int i = 0;
    if constexpr (sizeof(T) < 8) {
        uint64_t wide = value;
        for (size_t width = sizeof(T); width < 8; width *= 2)
            wide |= wide << (8 * width);
        constexpr int perWord = 8 / sizeof(T);
        for (; i < count && reinterpret_cast<uintptr_t>(buffer + i) % 8; ++i)
            buffer[i] = value;
        for (; i + perWord <= count; i += perWord)
            memcpy(buffer + i, &wide, 8);
    }
    for (; i < count; ++i)
        buffer[i] = value;
}

static void memset16(uint16_t buffer[], uint16_t value, int count) { fill(buffer, value, count); }
static void memset32(uint32_t buffer[], uint32_t value, int count) { fill(buffer, value, count); }
static void memset64(uint64_t buffer[], uint64_t value, int count) { fill(buffer, value, count); }

template<typename T>
static void rectFill(T buffer[], T value, int count, size_t rowBytes, int height)
{
    for (; height > 0; --height) {
        fill(buffer, value, count);
        buffer = reinterpret_cast<T*>(reinterpret_cast<char*>(buffer) + rowBytes);
    }
}

static void rectMemset16(uint16_t buffer[], uint16_t value, int count, size_t rowBytes, int height) { rectFill(buffer, value, count, rowBytes, height); }
static void rectMemset32(uint32_t buffer[], uint32_t value, int count, size_t rowBytes, int height) { rectFill(buffer, value, count, rowBytes, height); }
static void rectMemset64(uint64_t buffer[], uint64_t value, int count, size_t rowBytes, int height) { rectFill(buffer, value, count, rowBytes, height); }

void installMlxSkiaRoutines()
{
    static bool installed = [] {
        // (Skia's own choice first, so that a later SkGraphics::Init keeps
        // the port's.)
        SkOpts::Init_BlitRow();
        SkOpts::Init_Memset();
        SkOpts::blit_row_color32 = blitRowColor32;
        SkOpts::blit_row_s32a_opaque = blitRowS32AOpaque;
        SkOpts::memset16 = memset16;
        SkOpts::memset32 = memset32;
        SkOpts::memset64 = memset64;
        SkOpts::rect_memset16 = rectMemset16;
        SkOpts::rect_memset32 = rectMemset32;
        SkOpts::rect_memset64 = rectMemset64;
        return true;
    }();
    (void)installed;
}

} // namespace WebCore

#endif // USE(SKIA)
