/* Hinted glyphs of a CFF-based font as FreeType's CFF driver loads them
   (its own hinter, the Adobe engine): for several sizes and hinting
   targets, each glyph's metrics, its outline (as a hash) and the hash of
   the bitmap it renders to, then FT_Get_Advances with hinting.

   A TrueType font is hinted by FreeType's bytecode interpreter or its
   auto-hinter, which mlx-freetype does not have, so this test is for CFF
   and CFF2 fonts only.

   hinted FONT [FACE-INDEX [STEP]]   (every STEP-th glyph, default 1) */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ADVANCES_H
#include <stdio.h>
#include <stdlib.h>

static unsigned long hash_outline(const FT_Outline *o)
{
    unsigned long h = 0;
    for (int i = 0; i < o->n_points; i++) {
        h = h * 1000003 + (unsigned long)o->points[i].x;
        h = h * 1000003 + (unsigned long)o->points[i].y;
        h = h * 31 + (unsigned char)o->tags[i];
    }
    for (int i = 0; i < o->n_contours; i++) h = h * 31 + (unsigned short)o->contours[i];
    return h;
}

static unsigned long hash_bitmap(const FT_Bitmap *b)
{
    unsigned long h = 0;
    int bytes = b->pitch < 0 ? -b->pitch : b->pitch;
    for (unsigned y = 0; y < b->rows; y++)
        for (int x = 0; x < bytes; x++) h = h * 131 + b->buffer[y * bytes + x];
    return h;
}

static void load(FT_Face face, long g, FT_Int32 flags, FT_Render_Mode mode)
{
    FT_Error error = FT_Load_Glyph(face, g, flags);
    FT_GlyphSlot s = face->glyph;
    if (error) {
        printf("  %ld: error %d\n", g, error);
        return;
    }
    FT_Glyph_Metrics *mt = &s->metrics;
    printf("  %ld: m %ld %ld %ld %ld %ld %ld %ld %ld lin %ld %ld adv %ld %ld", g, mt->width, mt->height, mt->horiBearingX, mt->horiBearingY, mt->horiAdvance, mt->vertBearingX, mt->vertBearingY, mt->vertAdvance, s->linearHoriAdvance, s->linearVertAdvance, s->advance.x, s->advance.y);
    if (s->format == FT_GLYPH_FORMAT_OUTLINE) {
        printf(" o %d %d 0x%x %lx", s->outline.n_points, s->outline.n_contours, s->outline.flags, hash_outline(&s->outline));
        error = FT_Render_Glyph(s, mode);
        printf(" r %d %d %d %u %u %lx", error, s->bitmap_left, s->bitmap_top, s->bitmap.width, s->bitmap.rows, hash_bitmap(&s->bitmap));
    }
    printf("\n");
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
    static const struct { FT_Int32 flags; FT_Render_Mode mode; } targets[] = {
        { FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP, FT_RENDER_MODE_NORMAL },
        { FT_LOAD_TARGET_LIGHT | FT_LOAD_NO_BITMAP, FT_RENDER_MODE_LIGHT },
        { FT_LOAD_TARGET_MONO | FT_LOAD_NO_BITMAP, FT_RENDER_MODE_MONO },
        { FT_LOAD_TARGET_LCD | FT_LOAD_NO_BITMAP | FT_LOAD_NO_AUTOHINT, FT_RENDER_MODE_LCD },
        { FT_LOAD_TARGET_NORMAL | FT_LOAD_NO_BITMAP | FT_LOAD_LINEAR_DESIGN | FT_LOAD_VERTICAL_LAYOUT, FT_RENDER_MODE_NORMAL },
    };
    static const struct { long w, h; int xres, yres; int pixels; } sizes[] = {
        { 0, 0, 9, 9, 1 },
        { 0, 12 * 64, 96, 96, 0 },
        { 0, 0, 16, 16, 1 },
        { 0, 0, 23, 23, 1 },
        { 10 * 64, 14 * 64, 300, 150, 0 },
        { 0, 0, 48, 48, 1 },
    };
    for (unsigned si = 0; si < sizeof sizes / sizeof sizes[0]; si++) {
        if (sizes[si].pixels) error = FT_Set_Pixel_Sizes(face, sizes[si].xres, sizes[si].yres);
        else error = FT_Set_Char_Size(face, sizes[si].w, sizes[si].h, sizes[si].xres, sizes[si].yres);
        FT_Size_Metrics *sm = &face->size->metrics;
        printf("size %u: error %d ppem %u %u scale %ld %ld\n", si, error, sm->x_ppem, sm->y_ppem, sm->x_scale, sm->y_scale);
        for (unsigned ti = 0; ti < sizeof targets / sizeof targets[0]; ti++) {
            printf(" flags 0x%x\n", targets[ti].flags);
            for (long g = 0; g < face->num_glyphs; g += step) load(face, g, targets[ti].flags, targets[ti].mode);
            FT_Fixed adv[64];
            long n = face->num_glyphs < 64 ? face->num_glyphs : 64;
            FT_Error e = FT_Get_Advances(face, 0, n, targets[ti].flags, adv);
            unsigned long h = 0;
            for (long i = 0; i < n; i++) h = h * 31 + (unsigned long)adv[i];
            printf("  advances %d %lx\n", e, h);
        }
    }
    FT_Done_Face(face);
    FT_Done_FreeType(library);
    return 0;
}
