/* Writes PNG files with libpng's writer as Skia's SkPngEncoder does
 * (png_set_IHDR, sBIT, filters, the compression level, text, an sRGB or
 * iCCP chunk, unknown chunks before PLTE and after IDAT, png_write_info,
 * a filler stripped from RGBX rows, 16-bit samples swapped, the rows one
 * at a time, png_write_end) and prints each file's size and a hash of its
 * bytes; then reads the file back with the progressive reader and prints
 * a hash of the rows.
 *
 *   encode DIRECTORY
 *
 * Built by gcc against the system's libpng and by mlx-cc against
 * mlx-libpng, the outputs must be the same: the files byte for byte. */
#include <png.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned char *data;
    size_t length, capacity;
} Buffer;

static uint64_t fnv(uint64_t hash, const unsigned char *data, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        hash ^= data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static void onWrite(png_structp png, png_bytep data, png_size_t length)
{
    Buffer *b = (Buffer *)png_get_io_ptr(png);
    if (b->length + length > b->capacity) {
        b->capacity = (b->length + length) * 2;
        b->data = realloc(b->data, b->capacity);
    }
    memcpy(b->data + b->length, data, length);
    b->length += length;
}

static void onError(png_structp png, png_const_charp message)
{
    printf("  error: %s\n", message);
    longjmp(png_jmpbuf(png), 1);
}

static void onWarning(png_structp png, png_const_charp message)
{
    printf("  warning: %s\n", message);
}

static uint64_t readSum;
static unsigned readRows;

static void onInfo(png_structp png, png_infop info)
{
    png_set_interlace_handling(png);
    png_read_update_info(png, info);
}

static void onRow(png_structp png, png_bytep row, png_uint_32 number, int pass)
{
    if (!row)
        return;
    readRows++;
    readSum = fnv(readSum, row, png_get_rowbytes(png, (png_infop)png_get_progressive_ptr(png)));
}

static void readBack(const Buffer *b)
{
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, onError, onWarning);
    png_infop info = png_create_info_struct(png);
    readSum = 14695981039346656037ULL;
    readRows = 0;
    if (!setjmp(png_jmpbuf(png))) {
        png_set_progressive_read_fn(png, info, onInfo, onRow, 0);
        png_process_data(png, info, b->data, b->length);
    }
    printf("  read %u rows %016llx\n", readRows, (unsigned long long)readSum);
    png_destroy_read_struct(&png, &info, 0);
}

static unsigned pseudo = 1;
static unsigned next(void)
{
    pseudo ^= pseudo << 13;
    pseudo ^= pseudo >> 17;
    pseudo ^= pseudo << 5;
    return pseudo;
}

/* A smooth picture with noise, SOURCE channels a pixel of DEPTH bits. */
static unsigned char *picture(unsigned width, unsigned height, unsigned channels, unsigned depth, int noise)
{
    size_t bytes = (size_t)width * height * channels * (depth / 8);
    unsigned char *pixels = malloc(bytes);
    for (unsigned y = 0; y < height; y++)
        for (unsigned x = 0; x < width; x++)
            for (unsigned c = 0; c < channels; c++) {
                unsigned value = (x * 3 + y * 5 + c * 50) % 256;
                if (noise)
                    value = (value + next() % noise) % 256;
                size_t at = (((size_t)y * width + x) * channels + c) * (depth / 8);
                pixels[at] = (unsigned char)value;
                if (depth == 16)
                    pixels[at + 1] = (unsigned char)(next() & 255);
            }
    return pixels;
}

static void encode(const char *name, unsigned width, unsigned height, int color, int depth, unsigned sourceChannels, int filters, int level, int profile, int noise)
{
    printf("%s\n", name);
    Buffer b = { 0, 0, 0 };
    unsigned char *pixels = picture(width, height, sourceChannels, depth, noise);
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, onError, onWarning);
    png_infop info = png_create_info_struct(png);
    png_set_write_fn(png, &b, onWrite, 0);
    if (setjmp(png_jmpbuf(png))) {
        printf("  failed\n");
        png_destroy_write_struct(&png, &info);
        free(pixels);
        free(b.data);
        return;
    }
    png_set_IHDR(png, info, width, height, depth, color, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    png_color_8 sig;
    sig.red = sig.green = sig.blue = sig.gray = (png_byte)(depth == 16 ? 12 : 8);
    sig.alpha = (png_byte)depth;
    png_set_sBIT(png, info, &sig);
    if (filters >= 0)
        png_set_filter(png, PNG_FILTER_TYPE_BASE, filters);
    if (level >= 0)
        png_set_compression_level(png, level);
    png_text texts[2];
    memset(texts, 0, sizeof texts);
    texts[0].compression = PNG_TEXT_COMPRESSION_NONE;
    texts[0].key = (png_charp)"Software";
    texts[0].text = (png_charp)"encode.c";
    texts[1].compression = PNG_TEXT_COMPRESSION_NONE;
    texts[1].key = (png_charp)"Comment";
    texts[1].text = (png_charp)"a test image";
    png_set_text(png, info, texts, 2);
    if (profile) {
        unsigned char icc[200];
        memset(icc, 0, sizeof icc);
        png_save_uint_32(icc, sizeof icc);
        icc[8] = 4;
        memcpy(icc + 12, "mntr", 4);
        memcpy(icc + 16, (color & PNG_COLOR_MASK_COLOR) ? "RGB " : "GRAY", 4);
        memcpy(icc + 20, "XYZ ", 4);
        memcpy(icc + 36, "acsp", 4);
        if (profile == 2) {
            static const unsigned char d50[12] = { 0, 0, 0xf6, 0xd6, 0, 1, 0, 0, 0, 0, 0xd3, 0x2d };
            memcpy(icc + 68, d50, 12);
        }
        png_set_iCCP(png, info, "test profile", 0, icc, sizeof icc);
    } else
        png_set_sRGB(png, info, PNG_sRGB_INTENT_PERCEPTUAL);
    png_byte names[] = "skIA\0cHUN\0";
    png_set_keep_unknown_chunks(png, PNG_HANDLE_CHUNK_ALWAYS, names, 2);
    png_unknown_chunk chunks[2] = {
        { { 's', 'k', 'I', 'A', 0 }, (png_byte *)"before", 6, PNG_HAVE_IHDR },
        { { 'c', 'H', 'U', 'N', 0 }, (png_byte *)"after the image", 15, PNG_AFTER_IDAT },
    };
    png_set_unknown_chunks(png, info, chunks, 2);
    png_write_info(png, info);
    unsigned channels = png_get_channels(png, info);
    if (sourceChannels > channels)
        png_set_filler(png, 0, PNG_FILLER_AFTER);
    size_t stride = (size_t)width * sourceChannels * (depth / 8);
    for (unsigned y = 0; y < height; y++) {
        if (png_get_bit_depth(png, info) == 16)
            png_set_swap(png);
        png_bytep row = pixels + y * stride;
        png_write_rows(png, &row, 1);
    }
    png_write_end(png, info);
    printf("  %zu bytes %016llx\n", b.length, (unsigned long long)fnv(14695981039346656037ULL, b.data, b.length));
    png_destroy_write_struct(&png, &info);
    readBack(&b);
    free(pixels);
    free(b.data);
}

int main(void)
{
    encode("rgba8", 64, 48, PNG_COLOR_TYPE_RGB_ALPHA, 8, 4, -1, -1, 0, 20);
    encode("rgbx8 filler", 33, 17, PNG_COLOR_TYPE_RGB, 8, 4, -1, 6, 0, 8);
    encode("rgb8 smooth", 100, 80, PNG_COLOR_TYPE_RGB, 8, 3, PNG_ALL_FILTERS, 9, 2, 0);
    encode("gray8 sub", 50, 50, PNG_COLOR_TYPE_GRAY, 8, 1, PNG_FILTER_SUB, 6, 2, 4);
    encode("gray8 none", 50, 50, PNG_COLOR_TYPE_GRAY, 8, 1, PNG_FILTER_NONE, 1, 0, 4);
    encode("gray-alpha8 paeth", 20, 30, PNG_COLOR_TYPE_GRAY_ALPHA, 8, 2, PNG_FILTER_PAETH, 3, 0, 30);
    encode("rgba16", 21, 13, PNG_COLOR_TYPE_RGB_ALPHA, 16, 4, -1, -1, 0, 50);
    encode("rgb16 avg up", 40, 9, PNG_COLOR_TYPE_RGB, 16, 3, PNG_FILTER_AVG | PNG_FILTER_UP, 5, 1, 10);
    encode("rgba8 level 0", 30, 30, PNG_COLOR_TYPE_RGB_ALPHA, 8, 4, PNG_FAST_FILTERS, 0, 0, 255);
    encode("rgba8 large noisy", 300, 200, PNG_COLOR_TYPE_RGB_ALPHA, 8, 4, -1, -1, 0, 255);
    encode("one row", 77, 1, PNG_COLOR_TYPE_RGB_ALPHA, 8, 4, -1, -1, 0, 9);
    encode("one column", 1, 77, PNG_COLOR_TYPE_RGB, 8, 3, -1, -1, 0, 9);
    return 0;
}
