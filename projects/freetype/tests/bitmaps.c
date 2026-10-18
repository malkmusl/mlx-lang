/* Embedded bitmaps as FreeType loads them: for each strike of the font
   (selected, then requested by its pixel size), every STEP-th glyph's
   bitmap (its mode, size, pitch and a hash of its bytes), metrics and
   advances, in color and in gray, horizontal and vertical, and with the
   metrics only; then a size no strike has. (The glyphs a strike has no
   bitmap for are outlines, hinted by FreeType's TrueType bytecode
   interpreter, which mlx-freetype does not have: their metrics are
   compared, and their renderings only unhinted.)

   bitmaps FONT [FACE-INDEX [STEP]] */
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h>
#include <stdlib.h>

static unsigned long hash_bitmap(const FT_Bitmap *b)
{
    unsigned long h = 0;
    int bytes = b->pitch < 0 ? -b->pitch : b->pitch;
    if (!b->buffer) return 0;
    for (unsigned y = 0; y < b->rows; y++)
        for (int x = 0; x < bytes; x++) h = h * 131 + b->buffer[y * bytes + x];
    return h;
}

static void load(FT_Face face, long g, FT_Int32 flags)
{
    FT_Error error = FT_Load_Glyph(face, g, flags);
    FT_GlyphSlot s = face->glyph;
    if (error) {
        printf("  %ld: error %d\n", g, error);
        return;
    }
    FT_Glyph_Metrics *mt = &s->metrics;
    printf("  %ld: fmt %c%c%c%c m %ld %ld %ld %ld %ld %ld %ld %ld lin %ld %ld adv %ld %ld bm %d %d %u %u %d %d %d %lx\n", g, (char)(s->format >> 24), (char)(s->format >> 16), (char)(s->format >> 8), (char)s->format, mt->width, mt->height, mt->horiBearingX, mt->horiBearingY, mt->horiAdvance, mt->vertBearingX, mt->vertBearingY, mt->vertAdvance, s->linearHoriAdvance, s->linearVertAdvance, s->advance.x, s->advance.y, s->bitmap_left, s->bitmap_top, s->bitmap.width, s->bitmap.rows, s->bitmap.pitch, s->bitmap.pixel_mode, s->bitmap.num_grays, hash_bitmap(&s->bitmap));
}

static void glyphs(FT_Face face, long step)
{
    static const FT_Int32 flag_sets[] = {
        FT_LOAD_COLOR,
        FT_LOAD_DEFAULT,
        FT_LOAD_COLOR | FT_LOAD_VERTICAL_LAYOUT,
        FT_LOAD_COLOR | FT_LOAD_BITMAP_METRICS_ONLY,
        FT_LOAD_RENDER | FT_LOAD_TARGET_MONO | FT_LOAD_NO_HINTING,
        FT_LOAD_SBITS_ONLY | FT_LOAD_COLOR,
    };
    for (unsigned fi = 0; fi < sizeof flag_sets / sizeof flag_sets[0]; fi++) {
        printf(" flags 0x%x\n", flag_sets[fi]);
        for (long g = 0; g < face->num_glyphs; g += step) load(face, g, flag_sets[fi]);
        load(face, face->num_glyphs - 1, flag_sets[fi]);
    }
}

static void print_size(FT_Face face, const char *what, FT_Error error)
{
    FT_Size_Metrics *sm = &face->size->metrics;
    printf("%s: error %d ppem %u %u scale %ld %ld asc %ld desc %ld height %ld max %ld\n", what, error, sm->x_ppem, sm->y_ppem, sm->x_scale, sm->y_scale, sm->ascender, sm->descender, sm->height, sm->max_advance);
}

int main(int argc, char **argv)
{
    if (argc < 2) return 2;
    long index = argc > 2 ? atol(argv[2]) : 0;
    long step = argc > 3 ? atol(argv[3]) : 1;
    FT_Library library;
    FT_Face face;
    FT_Init_FreeType(&library);
    FT_Error error = FT_New_Face(library, argv[1], index, &face);
    printf("open %d\n", error);
    if (error) return 1;
    printf("flags 0x%lx sizes %d glyphs %ld\n", face->face_flags & ~FT_FACE_FLAG_HINTER, face->num_fixed_sizes, face->num_glyphs);
    for (int i = 0; i < face->num_fixed_sizes; i++) {
        FT_Bitmap_Size *b = &face->available_sizes[i];
        printf("strike %d: %d x %d size %ld ppem %ld %ld\n", i, b->width, b->height, b->size, b->x_ppem, b->y_ppem);
        error = FT_Select_Size(face, i);
        print_size(face, " selected", error);
        glyphs(face, step);
        error = FT_Set_Pixel_Sizes(face, (FT_UInt)((b->x_ppem + 32) >> 6), (FT_UInt)((b->y_ppem + 32) >> 6));
        print_size(face, " requested", error);
        load(face, 0, FT_LOAD_COLOR);
        load(face, face->num_glyphs / 2, FT_LOAD_COLOR);
    }
    error = FT_Select_Size(face, face->num_fixed_sizes);
    printf("select beyond: %d\n", error);
    error = FT_Set_Pixel_Sizes(face, 0, 77);
    print_size(face, "unmatched", error);
    load(face, face->num_glyphs / 3, FT_LOAD_COLOR);
    load(face, face->num_glyphs / 3, FT_LOAD_NO_BITMAP);
    FT_Done_Face(face);
    FT_Done_FreeType(library);
    return 0;
}
