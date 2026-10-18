/* Glyphs as FreeType loads them: for the font and face index given, at
   several sizes and with several load flags, each glyph's outline (its
   points, tags and contours as a hash), metrics, advances and the size
   of the bitmap it would be rendered to; FT_Get_Advance(s) and FT_Load_Char.

   glyphs FONT [FACE-INDEX [STEP]]   (every STEP-th glyph, default 1) */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ADVANCES_H
#include FT_OUTLINE_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void load(FT_Face face, long g, FT_Int32 flags, int verbose)
{
    FT_Error error = FT_Load_Glyph(face, g, flags);
    FT_GlyphSlot s = face->glyph;
    if (error) {
        printf("  %ld: error %d\n", g, error);
        return;
    }
    FT_Glyph_Metrics *mt = &s->metrics;
    printf("  %ld: fmt %c%c%c%c m %ld %ld %ld %ld %ld %ld %ld %ld lin %ld %ld adv %ld %ld", g, (char)(s->format >> 24), (char)(s->format >> 16), (char)(s->format >> 8), (char)s->format, mt->width, mt->height, mt->horiBearingX, mt->horiBearingY, mt->horiAdvance, mt->vertBearingX, mt->vertBearingY, mt->vertAdvance, s->linearHoriAdvance, s->linearVertAdvance, s->advance.x, s->advance.y);
    if (s->format == FT_GLYPH_FORMAT_OUTLINE) {
        printf(" o %d %d 0x%x %lx", s->outline.n_points, s->outline.n_contours, s->outline.flags, hash_outline(&s->outline));
        if (verbose) {
            for (int i = 0; i < s->outline.n_points; i++) printf(" (%ld,%ld,%d)", s->outline.points[i].x, s->outline.points[i].y, s->outline.tags[i]);
        }
    }
    if (s->format == FT_GLYPH_FORMAT_COMPOSITE) {
        printf(" sub %u", s->num_subglyphs);
        for (FT_UInt i = 0; i < s->num_subglyphs; i++) {
            FT_Int index, arg1, arg2;
            FT_UInt sflags;
            FT_Matrix m;
            FT_Get_SubGlyph_Info(s, i, &index, &sflags, &arg1, &arg2, &m);
            printf(" [%d 0x%x %d %d %ld %ld %ld %ld]", index, sflags, arg1, arg2, m.xx, m.xy, m.yx, m.yy);
        }
    }
    printf(" bm %d %d %u %u %d %d\n", s->bitmap_left, s->bitmap_top, s->bitmap.width, s->bitmap.rows, s->bitmap.pitch, s->bitmap.pixel_mode);
}

int main(int argc, char **argv)
{
    if (argc < 2) return 2;
    long index = argc > 2 ? atol(argv[2]) : 0;
    long step = argc > 3 ? atol(argv[3]) : 1;
    int verbose = getenv("VERBOSE") != NULL;
    FT_Library library;
    FT_Face face;
    FT_Init_FreeType(&library);
    FT_Error error = FT_New_Face(library, argv[1], index, &face);
    printf("open %d\n", error);
    if (error) return 1;
    static const FT_Int32 flag_sets[] = {
        FT_LOAD_NO_SCALE,
        FT_LOAD_NO_HINTING,
        FT_LOAD_NO_HINTING | FT_LOAD_VERTICAL_LAYOUT,
        FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_TARGET_MONO,
        FT_LOAD_NO_HINTING | FT_LOAD_LINEAR_DESIGN | FT_LOAD_TARGET_LCD,
        FT_LOAD_NO_RECURSE,
        FT_LOAD_NO_HINTING | FT_LOAD_IGNORE_TRANSFORM | FT_LOAD_TARGET_LCD_V,
    };
    /* sizes: char size (26.6 points) and resolution, or pixel sizes */
    static const struct { long w, h; int xres, yres; int pixels; } sizes[] = {
        { 0, 12 * 64, 96, 96, 0 },
        { 0, 0, 16, 16, 1 },
        { 37 * 64 + 32, 0, 72, 72, 0 },
        { 10 * 64, 14 * 64, 300, 150, 0 },
        { 0, 0, 100, 100, 1 },
        { 0, 0, 7, 9, 1 },
    };
    for (unsigned si = 0; si < sizeof sizes / sizeof sizes[0]; si++) {
        if (sizes[si].pixels) error = FT_Set_Pixel_Sizes(face, sizes[si].xres, sizes[si].yres);
        else error = FT_Set_Char_Size(face, sizes[si].w, sizes[si].h, sizes[si].xres, sizes[si].yres);
        FT_Size_Metrics *sm = &face->size->metrics;
        printf("size %u: error %d ppem %u %u scale %ld %ld asc %ld desc %ld height %ld max %ld\n", si, error, sm->x_ppem, sm->y_ppem, sm->x_scale, sm->y_scale, sm->ascender, sm->descender, sm->height, sm->max_advance);
        for (unsigned fi = 0; fi < sizeof flag_sets / sizeof flag_sets[0]; fi++) {
            if (si > 0 && (flag_sets[fi] == FT_LOAD_NO_SCALE || flag_sets[fi] == FT_LOAD_NO_RECURSE)) continue;
            printf(" flags 0x%x\n", flag_sets[fi]);
            for (long g = 0; g < face->num_glyphs; g += step) load(face, g, flag_sets[fi], verbose);
            load(face, face->num_glyphs, flag_sets[fi], 0);
            {
                FT_Fixed adv[64];
                long n = face->num_glyphs < 64 ? face->num_glyphs : 64;
                FT_Error e = FT_Get_Advances(face, 0, n, flag_sets[fi], adv);
                unsigned long h = 0;
                for (long i = 0; i < n; i++) h = h * 31 + (unsigned long)adv[i];
                FT_Fixed one = 0;
                FT_Error e1 = FT_Get_Advance(face, n - 1, flag_sets[fi], &one);
                FT_Error e2 = FT_Get_Advance(face, face->num_glyphs, flag_sets[fi], &one);
                printf("  advances %d %lx %d %ld %d\n", e, h, e1, one, e2);
            }
        }
        /* a transform: rotation and translation */
        FT_Matrix matrix = { 0xB505, -0xB505, 0xB505, 0xB505 };
        FT_Vector delta = { 100, -50 };
        FT_Set_Transform(face, &matrix, &delta);
        printf(" transformed\n");
        for (long g = 0; g < face->num_glyphs; g += step * 7) load(face, g, FT_LOAD_NO_HINTING, 0);
        FT_Set_Transform(face, NULL, NULL);
        printf(" chars\n");
        static const unsigned long chars[] = { 'A', 'g', 0x20AC, 0x4E00, 0x1F600, 0xFFFF, 0x10FFFF };
        for (unsigned c = 0; c < sizeof chars / sizeof chars[0]; c++) {
            error = FT_Load_Char(face, chars[c], FT_LOAD_NO_HINTING);
            printf("  char %lx: %d glyph %u adv %ld\n", chars[c], error, face->glyph->glyph_index, face->glyph->advance.x);
        }
    }
    FT_Done_Face(face);
    FT_Done_FreeType(library);
    return 0;
}
