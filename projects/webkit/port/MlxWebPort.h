// The Mlx side of PlatformMlx as the port's C++ sees it: the functions of
// libmlxwebport.so (projects/webkit/port/mlx/webport.mlx), with the C ABI.
// Handles are addresses of state the Mlx side keeps on the C library's
// heap; a function given a null handle does nothing.
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Digests (std.sha1, std.sha2) for PAL's CryptoDigest. The algorithm is
// the digest's length in bits, 1 for SHA-1; create gives 0 for another.
enum {
    MLXWEB_DIGEST_SHA1 = 1,
    MLXWEB_DIGEST_SHA224 = 224,
    MLXWEB_DIGEST_SHA256 = 256,
    MLXWEB_DIGEST_SHA384 = 384,
    MLXWEB_DIGEST_SHA512 = 512,
};
uintptr_t mlxweb_digest_create(uint32_t algorithm);
void mlxweb_digest_update(uintptr_t digest, const void* bytes, size_t length);
// Writes the digest (at most 64 bytes) and gives its length.
size_t mlxweb_digest_finish(uintptr_t digest, uint8_t* out);
void mlxweb_digest_destroy(uintptr_t digest);

// Image decoding (std.png, std.jpeg, std.webp) for the port's image
// decoders: a whole file in memory to 8-bit RGBA rows, not premultiplied;
// decode gives 0 for data that is not a whole image of the format.
enum {
    MLXWEB_IMAGE_PNG = 1,
    MLXWEB_IMAGE_JPEG = 2,
    MLXWEB_IMAGE_WEBP = 3,
};
uintptr_t mlxweb_image_decode(uint32_t format, const void* bytes, size_t length);
size_t mlxweb_image_width(uintptr_t image);
size_t mlxweb_image_height(uintptr_t image);
const uint8_t* mlxweb_image_pixels(uintptr_t image);
// The EXIF orientation, 1 to 8 (1: as stored).
uint32_t mlxweb_image_orientation(uintptr_t image);
void mlxweb_image_destroy(uintptr_t image);

// Fonts (std.fontconfig, with fonts.conf's rules) for the port's font
// manager: the font of a family (a UTF-8 C string, or null for any)
// nearest to a style, or 0 when no font has that family (a generic family,
// "serif", "monospace"..., always has one); the font nearest to a style
// that has a character, for a language (BCP 47, or null). The weight is
// OpenType's (100 to 900); the width and slant Fontconfig's (FC_WIDTH_*:
// 50 to 200, 100 normal; FC_SLANT_*: 0 roman, 100 italic, 110 oblique).
uintptr_t mlxweb_font_match(const char* family, int32_t weight, int32_t width, int32_t slant);
uintptr_t mlxweb_font_fallback(uint32_t character, const char* locale, int32_t weight, int32_t width, int32_t slant);
// The font's file and its index in the file (a collection's).
const char* mlxweb_font_path(uintptr_t font);
int32_t mlxweb_font_index(uintptr_t font);
void mlxweb_font_release(uintptr_t font);

#ifdef __cplusplus
}
#endif
