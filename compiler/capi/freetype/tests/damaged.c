/* Damaged fonts: a font's bytes changed at places of a fixed pseudo-random
   sequence (or the font cut short), then opened from memory and every
   STEP-th glyph loaded and rendered with several load flags, the
   charmaps walked and the advances asked for. What FreeType returns for
   each (its errors, metrics and a hash of what it renders) is printed;
   nothing may crash or hang.

   damaged FONT SEED COUNT [STEP]
     COUNT > 0: COUNT bytes changed; COUNT < 0: the font cut to -COUNT
     thousandths of its size */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H
#include FT_COLOR_H
#include FT_ADVANCES_H
#include <stdio.h>
#include <stdlib.h>

static unsigned long long state;

static unsigned long next(void)
{
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return (unsigned long)state;
}

static unsigned long hash_bitmap(const FT_Bitmap *b)
{
    unsigned long h = 0;
    int bytes = b->pitch < 0 ? -b->pitch : b->pitch;
    if (!b->buffer) return 0;
    for (unsigned y = 0; y < b->rows; y++)
        for (int x = 0; x < bytes; x++) h = h * 131 + b->buffer[y * bytes + x];
    return h;
}

int main(int argc, char **argv)
{
    if (argc < 4) return 2;
    FILE *file = fopen(argv[1], "rb");
    if (!file) return 2;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    unsigned char *data = malloc((size_t)size);
    if (fread(data, 1, (size_t)size, file) != (size_t)size) return 2;
    fclose(file);
    state = 88172645463325252ULL ^ (unsigned long long)atol(argv[2]);
    long count = atol(argv[3]);
    long step = argc > 4 ? atol(argv[4]) : 1;
    if (count > 0) {
        for (long i = 0; i < count; i++) {
            unsigned long r = next();
            long at = (long)(r % (unsigned long)size);
            /* Mostly near the start, where the tables' headers are. */
            if (i % 2 == 0) at = (long)(r % (unsigned long)(size < 4096 ? size : 4096));
            data[at] = (unsigned char)(next() >> 24);
        }
    } else {
        size = size * (-count) / 1000;
    }
    if (getenv("DAMAGED_OUT")) {
        FILE *out = fopen(getenv("DAMAGED_OUT"), "wb");
        if (out) {
            fwrite(data, 1, (size_t)size, out);
            fclose(out);
        }
    }
    FT_Library library;
    FT_Face face;
    FT_Init_FreeType(&library);
    FT_Error error = FT_New_Memory_Face(library, data, size, 0, &face);
    printf("open %d\n", error);
    if (error) return 0;
    printf("faces %ld glyphs %ld flags 0x%lx charmaps %d sizes %d\n", face->num_faces, face->num_glyphs, face->face_flags & ~FT_FACE_FLAG_HINTER, face->num_charmaps, face->num_fixed_sizes);
    error = FT_Set_Pixel_Sizes(face, 0, 17);
    printf("size %d\n", error);
    if (FT_HAS_MULTIPLE_MASTERS(face)) {
        FT_MM_Var *mm;
        error = FT_Get_MM_Var(face, &mm);
        printf("mm %d\n", error);
        if (!error) {
            FT_Fixed coords[4] = { 0x8000, -0x4000, 0x2000, 0 };
            printf("blend %d\n", FT_Set_Var_Blend_Coordinates(face, mm->num_axis < 4 ? mm->num_axis : 4, coords));
            FT_Done_MM_Var(library, mm);
        }
    }
    static const FT_Int32 flag_sets[] = {
        FT_LOAD_NO_HINTING | FT_LOAD_RENDER,
        FT_LOAD_NO_SCALE,
        FT_LOAD_NO_HINTING | FT_LOAD_RENDER | FT_LOAD_TARGET_MONO,
        FT_LOAD_COLOR | FT_LOAD_RENDER | FT_LOAD_NO_HINTING,
        FT_LOAD_NO_HINTING | FT_LOAD_VERTICAL_LAYOUT,
    };
    unsigned long total = 0;
    for (unsigned fi = 0; fi < sizeof flag_sets / sizeof flag_sets[0]; fi++) {
        unsigned long h = 0;
        int errors = 0;
        for (long g = 0; g < face->num_glyphs && g < 3000 * step; g += step) {
            error = FT_Load_Glyph(face, (FT_UInt)g, flag_sets[fi]);
            if (error) {
                errors++;
                h = h * 31 + (unsigned long)error;
                continue;
            }
            FT_GlyphSlot s = face->glyph;
            h = h * 1000003 + (unsigned long)s->metrics.width;
            h = h * 1000003 + (unsigned long)s->metrics.horiAdvance;
            h = h * 1000003 + (unsigned long)s->outline.n_points;
            h = h * 1000003 + hash_bitmap(&s->bitmap);
        }
        printf("flags 0x%x: errors %d hash %lx\n", flag_sets[fi], errors, h);
        total += h;
    }
    for (int i = 0; i < face->num_charmaps; i++) {
        FT_Set_Charmap(face, face->charmaps[i]);
        FT_UInt gindex;
        unsigned long h = 0;
        long n = 0;
        FT_ULong c = FT_Get_First_Char(face, &gindex);
        while (gindex && n < 100000) {
            h = h * 31 + c * 7 + gindex;
            c = FT_Get_Next_Char(face, c, &gindex);
            n++;
        }
        printf("charmap %d: %ld %lx\n", i, n, h);
    }
    FT_Fixed advances[64];
    long n = face->num_glyphs < 64 ? face->num_glyphs : 64;
    error = FT_Get_Advances(face, 0, (FT_UInt)n, FT_LOAD_NO_HINTING, advances);
    printf("advances %d\n", error);
    printf("ps %s\n", FT_Get_Postscript_Name(face) ? "yes" : "no");
    FT_Done_Face(face);
    FT_Done_FreeType(library);
    free(data);
    return 0;
}
