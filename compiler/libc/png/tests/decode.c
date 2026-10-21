/* Decodes PNG files with libpng's progressive reader as WebKit's
 * PNGImageDecoder and Skia's SkPngCodec drive it, and prints what a
 * decoder sees: the header, the info after png_read_update_info, the
 * unknown chunks given to the user chunk function, the warnings and
 * errors, the rows the row function got (how many, with or without data,
 * a hash of their passes and numbers) and a hash of the final pixels.
 *
 *   decode MODE PIECE FILE...
 *
 * MODE is one of
 *   webkit  expand, strip_16, gray_to_rgb, png_set_gamma(2.2, file's or
 *           0.45455), interlace handling with png_progressive_combine_row
 *           into a whole image (WebKit's PNGImageDecoder), APNG's chunks
 *           kept for the user chunk function
 *   raw     no transformations: the rows as the file has them, the
 *           passes of an interlaced image as libpng gives them
 *   skia    expand_gray_1_2_4_to_8, tRNS_to_alpha, palette to RGB(A),
 *           filler after RGB, swap of 16-bit samples, interlace handling
 *           (Skia's SkPngCodec), the maximum inflate window option
 *   scale   scale_16, strip_alpha, gray_to_rgb, packing, bgr, invert_mono
 * PIECE is how many bytes each png_process_data call is given (0: the
 * whole file at once).
 *
 * Built by gcc against the system's libpng and by mlx-cc against
 * mlx-libpng, the outputs must be the same. */
#include <png.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { WEBKIT, RAW, SKIA, SCALE };

typedef struct {
    int mode;
    png_structp png;
    png_infop info;
    unsigned width, height, channels, rowbytes;
    int interlaced;
    unsigned char *image;
    unsigned long rows, blankRows;
    uint64_t sequence;
    int ended;
    char message[256];
} Decoder;

static uint64_t fnv(uint64_t hash, const unsigned char *data, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        hash ^= data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static void onError(png_structp png, png_const_charp message)
{
    Decoder *d = (Decoder *)png_get_error_ptr(png);
    snprintf(d->message, sizeof d->message, "%s", message);
    longjmp(png_jmpbuf(png), 1);
}

static void onWarning(png_structp png, png_const_charp message)
{
    printf("  warning: %s\n", message);
}

static void onInfo(png_structp png, png_infop info)
{
    Decoder *d = (Decoder *)png_get_progressive_ptr(png);
    png_uint_32 width, height;
    int depth, color, interlace, compression, filter;
    png_get_IHDR(png, info, &width, &height, &depth, &color, &interlace, &compression, &filter);
    printf("  header %ux%u depth %d color %d interlace %d\n", (unsigned)width, (unsigned)height, depth, color, interlace);
    printf("  valid %08x\n", (unsigned)png_get_valid(png, info, 0xffffffff));
    double gamma = 0;
    png_fixed_point fixedGamma = 0;
    if (png_get_gAMA(png, info, &gamma))
        printf("  gAMA %.5f\n", gamma);
    if (png_get_gAMA_fixed(png, info, &fixedGamma))
        printf("  gAMA fixed %d\n", (int)fixedGamma);
    int intent;
    if (png_get_sRGB(png, info, &intent))
        printf("  sRGB %d\n", intent);
    png_fixed_point wx, wy, rx, ry, gx, gy, bx, by;
    if (png_get_cHRM_fixed(png, info, &wx, &wy, &rx, &ry, &gx, &gy, &bx, &by))
        printf("  cHRM %d %d %d %d %d %d %d %d\n", wx, wy, rx, ry, gx, gy, bx, by);
    png_charp name;
    png_bytep profile;
    png_uint_32 length;
    int kind;
    if (png_get_iCCP(png, info, &name, &kind, &profile, &length))
        printf("  iCCP '%s' %u bytes hash %016llx\n", name, (unsigned)length, (unsigned long long)fnv(14695981039346656037ULL, profile, length));
    png_color_8p sig;
    if (png_get_sBIT(png, info, &sig))
        printf("  sBIT %d %d %d %d %d\n", sig->red, sig->green, sig->blue, sig->gray, sig->alpha);
    png_colorp palette;
    int count = 0;
    if (png_get_PLTE(png, info, &palette, &count))
        printf("  PLTE %d hash %016llx\n", count, (unsigned long long)fnv(14695981039346656037ULL, (unsigned char *)palette, count * 3));
    png_bytep alpha = 0;
    int trans = 0;
    png_color_16p transColor = 0;
    if (png_get_valid(png, info, PNG_INFO_tRNS) && png_get_tRNS(png, info, &alpha, &trans, &transColor)) {
        printf("  tRNS %d", trans);
        if (alpha)
            printf(" alpha hash %016llx", (unsigned long long)fnv(14695981039346656037ULL, alpha, trans));
        else if (transColor)
            printf(" color %u %u %u %u", transColor->red, transColor->green, transColor->blue, transColor->gray);
        printf("\n");
    }

    if (d->mode == WEBKIT) {
        if (color == PNG_COLOR_TYPE_PALETTE)
            png_set_expand(png);
        if (color == PNG_COLOR_TYPE_GRAY && depth < 8)
            png_set_expand(png);
        if (png_get_valid(png, info, PNG_INFO_tRNS))
            png_set_expand(png);
        if (depth == 16)
            png_set_strip_16(png);
        if (color == PNG_COLOR_TYPE_GRAY || color == PNG_COLOR_TYPE_GRAY_ALPHA)
            png_set_gray_to_rgb(png);
        double fileGamma;
        if (png_get_gAMA(png, info, &fileGamma)) {
            if (fileGamma <= 0.0 || fileGamma > 21474.83) {
                fileGamma = 0.45455;
                png_set_gAMA(png, info, fileGamma);
            }
            png_set_gamma(png, 2.2, fileGamma);
        } else
            png_set_gamma(png, 2.2, 0.45455);
    } else if (d->mode == SKIA) {
        if (color == PNG_COLOR_TYPE_PALETTE)
            png_set_palette_to_rgb(png);
        if (png_get_valid(png, info, PNG_INFO_tRNS))
            png_set_tRNS_to_alpha(png);
        if (color == PNG_COLOR_TYPE_GRAY && depth < 8)
            png_set_expand_gray_1_2_4_to_8(png);
        if (color == PNG_COLOR_TYPE_RGB && !png_get_valid(png, info, PNG_INFO_tRNS))
            png_set_filler(png, 0xff, PNG_FILLER_AFTER);
        if (depth == 16)
            png_set_swap(png);
    } else if (d->mode == SCALE) {
        if (depth == 16)
            png_set_scale_16(png);
        png_set_strip_alpha(png);
        if (color == PNG_COLOR_TYPE_GRAY || color == PNG_COLOR_TYPE_GRAY_ALPHA)
            png_set_gray_to_rgb(png);
        png_set_packing(png);
        png_set_bgr(png);
        png_set_invert_mono(png);
    }
    int passes = 1;
    if (d->mode != RAW || interlace == PNG_INTERLACE_ADAM7)
        passes = png_set_interlace_handling(png);
    png_read_update_info(png, info);
    d->width = width;
    d->height = height;
    d->channels = png_get_channels(png, info);
    d->rowbytes = png_get_rowbytes(png, info);
    d->interlaced = interlace == PNG_INTERLACE_ADAM7;
    printf("  passes %d channels %u rowbytes %u depth %d color %d\n", passes, d->channels, d->rowbytes, png_get_bit_depth(png, info), png_get_color_type(png, info));
    d->image = calloc((size_t)d->rowbytes * height + 1, 1);
}

static void onRow(png_structp png, png_bytep row, png_uint_32 number, int pass)
{
    Decoder *d = (Decoder *)png_get_progressive_ptr(png);
    unsigned char key[8] = { (unsigned char)number, (unsigned char)(number >> 8), (unsigned char)(number >> 16), (unsigned char)pass, row ? 1 : 0 };
    d->sequence = fnv(d->sequence, key, 5);
    if (!row) {
        d->blankRows++;
        return;
    }
    d->rows++;
    if (number >= d->height)
        return;
    unsigned char *target = d->image + (size_t)number * d->rowbytes;
    if (d->interlaced)
        png_progressive_combine_row(png, target, row);
    else
        memcpy(target, row, d->rowbytes);
}

static void onEnd(png_structp png, png_infop info)
{
    Decoder *d = (Decoder *)png_get_progressive_ptr(png);
    d->ended = 1;
}

static int onChunk(png_structp png, png_unknown_chunkp chunk)
{
    printf("  chunk %.4s size %u location %d hash %016llx\n", (const char *)chunk->name, (unsigned)chunk->size, chunk->location, (unsigned long long)fnv(14695981039346656037ULL, chunk->data, chunk->size));
    return 1;
}

static void decode(int mode, size_t piece, const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file) {
        printf("%s: cannot open\n", path);
        return;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    unsigned char *data = malloc(size + 1);
    if (fread(data, 1, size, file) != (size_t)size)
        size = 0;
    fclose(file);
    const char *base = strrchr(path, '/');
    printf("%s\n", base ? base + 1 : path);

    static Decoder d;
    memset(&d, 0, sizeof d);
    d.mode = mode;
    d.sequence = 14695981039346656037ULL;
    d.png = png_create_read_struct(PNG_LIBPNG_VER_STRING, &d, onError, onWarning);
    d.info = png_create_info_struct(d.png);
    if (setjmp(png_jmpbuf(d.png))) {
        printf("  error: %s\n", d.message);
    } else {
        if (mode == SKIA)
            png_set_option(d.png, PNG_MAXIMUM_INFLATE_WINDOW, PNG_OPTION_ON);
        png_set_progressive_read_fn(d.png, &d, onInfo, onRow, onEnd);
        if (mode == WEBKIT) {
            png_byte apngChunks[] = { "acTL\0fcTL\0fdAT\0" };
            png_set_keep_unknown_chunks(d.png, 1, apngChunks, 3);
            png_set_read_user_chunk_fn(d.png, &d, onChunk);
        } else if (mode == SKIA) {
            png_set_keep_unknown_chunks(d.png, PNG_HANDLE_CHUNK_ALWAYS, (png_const_bytep)"", 0);
            png_set_read_user_chunk_fn(d.png, &d, onChunk);
        }
        size_t at = 0;
        while (at < (size_t)size) {
            size_t count = piece ? piece : (size_t)size;
            if (count > (size_t)size - at)
                count = (size_t)size - at;
            png_process_data(d.png, d.info, data + at, count);
            at += count;
        }
    }
    printf("  rows %lu blank %lu sequence %016llx ended %d\n", d.rows, d.blankRows, (unsigned long long)d.sequence, d.ended);
    if (d.image)
        printf("  pixels %016llx\n", (unsigned long long)fnv(14695981039346656037ULL, d.image, (size_t)d.rowbytes * d.height));
    png_destroy_read_struct(&d.png, &d.info, 0);
    free(d.image);
    free(data);
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: decode MODE PIECE FILE...\n");
        return 2;
    }
    int mode = !strcmp(argv[1], "webkit") ? WEBKIT : !strcmp(argv[1], "raw") ? RAW : !strcmp(argv[1], "skia") ? SKIA : SCALE;
    size_t piece = (size_t)atol(argv[2]);
    for (int i = 3; i < argc; i++)
        decode(mode, piece, argv[i]);
    return 0;
}
