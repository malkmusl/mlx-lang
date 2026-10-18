/* Glyph objects, bitmaps, synthetic styles and outline functions as
   FreeType gives them: FT_Get_Glyph, FT_Glyph_Copy/Transform/Get_CBox/
   To_Bitmap, FT_Bitmap_Copy/Convert/Embolden/Blend, FT_GlyphSlot_*
   (Embolden, Oblique, Slant, AdjustWeight, Own_Bitmap), FT_Outline_*
   (Decompose, Get_BBox, Embolden, Reverse, Get_Orientation, Check).

   glyphapi FONT [FACE-INDEX [STEP]] */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H
#include FT_BITMAP_H
#include FT_SYNTHESIS_H
#include FT_BBOX_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long hash_bitmap(const FT_Bitmap *b)
{
    unsigned long h = 0;
    int bytes = b->pixel_mode == FT_PIXEL_MODE_MONO ? (int)(b->width + 7) / 8 : b->pixel_mode == FT_PIXEL_MODE_BGRA ? (int)b->width * 4 : (int)b->width;
    if (!b->buffer) return 0;
    for (unsigned r = 0; r < b->rows; r++) {
        const unsigned char *row = b->buffer + (b->pitch >= 0 ? (long)r * b->pitch : (long)(b->rows - 1 - r) * -b->pitch);
        for (int i = 0; i < bytes; i++) h = h * 31 + row[i];
    }
    return h;
}

static unsigned long hash_outline(const FT_Outline *o)
{
    unsigned long h = 0;
    for (int i = 0; i < o->n_points; i++) {
        h = h * 1000003 + (unsigned long)o->points[i].x;
        h = h * 1000003 + (unsigned long)o->points[i].y;
        h = h * 31 + (unsigned char)o->tags[i];
    }
    for (int i = 0; i < o->n_contours; i++) h = h * 31 + (unsigned short)o->contours[i];
    return h * 31 + (unsigned)o->flags;
}

struct walk { unsigned long hash; int moves, lines, conics, cubics; };

static int move_to(const FT_Vector *to, void *user) { struct walk *w = user; w->moves++; w->hash = w->hash * 31 + (unsigned long)to->x * 7 + (unsigned long)to->y; return 0; }
static int line_to(const FT_Vector *to, void *user) { struct walk *w = user; w->lines++; w->hash = w->hash * 37 + (unsigned long)to->x * 7 + (unsigned long)to->y; return 0; }
static int conic_to(const FT_Vector *c, const FT_Vector *to, void *user) { struct walk *w = user; w->conics++; w->hash = w->hash * 41 + (unsigned long)c->x * 11 + (unsigned long)c->y * 13 + (unsigned long)to->x * 7 + (unsigned long)to->y; return 0; }
static int cubic_to(const FT_Vector *c1, const FT_Vector *c2, const FT_Vector *to, void *user) { struct walk *w = user; w->cubics++; w->hash = w->hash * 43 + (unsigned long)c1->x * 3 + (unsigned long)c1->y * 5 + (unsigned long)c2->x * 11 + (unsigned long)c2->y * 13 + (unsigned long)to->x * 7 + (unsigned long)to->y; return 0; }

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
    FT_Set_Char_Size(face, 0, 15 * 64, 96, 96);
    FT_Outline_Funcs funcs = { move_to, line_to, conic_to, cubic_to, 0, 0 };
    FT_Outline_Funcs scaled = { move_to, line_to, conic_to, cubic_to, 2, 7 };
    for (long g = 0; g < face->num_glyphs; g += step) {
        error = FT_Load_Glyph(face, g, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP);
        if (error) { printf("%ld: load %d\n", g, error); continue; }
        FT_Outline *o = &face->glyph->outline;
        struct walk w = { 0, 0, 0, 0, 0 }, w2 = { 0, 0, 0, 0, 0 };
        FT_Error ed = FT_Outline_Decompose(o, &funcs, &w);
        FT_Error ed2 = FT_Outline_Decompose(o, &scaled, &w2);
        FT_BBox bbox, cbox;
        FT_Error eb = FT_Outline_Get_BBox(o, &bbox);
        FT_Outline_Get_CBox(o, &cbox);
        printf("%ld: walk %d %d %d %d %d %lx %d %lx bbox %d %ld %ld %ld %ld cbox %ld %ld %ld %ld orient %d check %d\n", g, ed, w.moves, w.lines, w.conics, w.cubics, w.hash, ed2, w2.hash, eb, bbox.xMin, bbox.yMin, bbox.xMax, bbox.yMax, cbox.xMin, cbox.yMin, cbox.xMax, cbox.yMax, FT_Outline_Get_Orientation(o), FT_Outline_Check(o));

        FT_Glyph glyph, copy;
        error = FT_Get_Glyph(face->glyph, &glyph);
        printf("  get %d format %x advance %ld %ld\n", error, glyph ? glyph->format : 0, glyph ? glyph->advance.x : 0, glyph ? glyph->advance.y : 0);
        if (error) continue;
        FT_Glyph_Copy(glyph, &copy);
        FT_Matrix matrix = { 0x10000, 0x5000, -0x3000, 0xF000 };
        FT_Vector delta = { 33, -17 };
        FT_Glyph_Transform(copy, &matrix, &delta);
        for (unsigned mode = 0; mode < 4; mode++) {
            FT_Glyph_Get_CBox(copy, mode, &cbox);
            printf("  cbox %u: %ld %ld %ld %ld\n", mode, cbox.xMin, cbox.yMin, cbox.xMax, cbox.yMax);
        }
        printf("  transformed %lx advance %ld %ld\n", hash_outline(&((FT_OutlineGlyph)copy)->outline), copy->advance.x, copy->advance.y);
        FT_Vector origin = { 20, 45 };
        FT_Glyph bm = copy;
        error = FT_Glyph_To_Bitmap(&bm, FT_RENDER_MODE_NORMAL, &origin, 0);
        if (!error) {
            FT_BitmapGlyph b = (FT_BitmapGlyph)bm;
            printf("  to bitmap %d %d %d %u %u %lx advance %ld after %lx\n", error, b->left, b->top, b->bitmap.width, b->bitmap.rows, hash_bitmap(&b->bitmap), bm->advance.x, hash_outline(&((FT_OutlineGlyph)copy)->outline));
            FT_Glyph_Get_CBox(bm, FT_GLYPH_BBOX_PIXELS, &cbox);
            printf("  bitmap cbox %ld %ld %ld %ld\n", cbox.xMin, cbox.yMin, cbox.xMax, cbox.yMax);
            FT_Bitmap converted, emboldened, flipped;
            FT_Bitmap_Init(&converted);
            FT_Bitmap_Init(&emboldened);
            FT_Bitmap_Init(&flipped);
            FT_Error e1 = FT_Bitmap_Copy(library, &b->bitmap, &emboldened);
            FT_Error e2 = FT_Bitmap_Embolden(library, &emboldened, 100, 70);
            flipped.pitch = -1;
            FT_Error e3 = FT_Bitmap_Copy(library, &emboldened, &flipped);
            FT_Error e4 = FT_Bitmap_Convert(library, &flipped, &converted, 4);
            printf("  bitmaps %d %d %u %u %d %lx %d %d %lx %d %u %d %lx\n", e1, e2, emboldened.width, emboldened.rows, emboldened.pitch, hash_bitmap(&emboldened), e3, flipped.pitch, hash_bitmap(&flipped), e4, converted.width, converted.pitch, hash_bitmap(&converted));
            FT_Bitmap target;
            FT_Bitmap_Init(&target);
            FT_Vector toff = { 0, 0 };
            FT_Color color = { 0x20, 0x80, 0xC0, 0xFF };
            FT_Vector soff = { 64 * 3, 64 * 10 };
            FT_Error e5 = FT_Bitmap_Blend(library, &b->bitmap, soff, &target, &toff, color);
            FT_Vector soff2 = { -64 * 2, 64 * 4 };
            FT_Color color2 = { 0xFF, 0x10, 0x10, 0x80 };
            FT_Error e6 = FT_Bitmap_Blend(library, &emboldened, soff2, &target, &toff, color2);
            printf("  blend %d %d %u %u %d %ld %ld %lx\n", e5, e6, target.width, target.rows, target.pitch, toff.x, toff.y, hash_bitmap(&target));
            FT_Bitmap_Done(library, &converted);
            FT_Bitmap_Done(library, &emboldened);
            FT_Bitmap_Done(library, &flipped);
            FT_Bitmap_Done(library, &target);
            FT_Done_Glyph(bm);
        } else
            printf("  to bitmap %d\n", error);
        bm = glyph;
        error = FT_Glyph_To_Bitmap(&bm, FT_RENDER_MODE_MONO, NULL, 1);
        if (!error) {
            FT_BitmapGlyph b = (FT_BitmapGlyph)bm;
            FT_Bitmap mono;
            FT_Bitmap_Init(&mono);
            FT_Bitmap_Copy(library, &b->bitmap, &mono);
            FT_Error e = FT_Bitmap_Embolden(library, &mono, 128, 64);
            printf("  mono %d %d %u %u %lx embolden %d %u %u %d %lx\n", b->left, b->top, b->bitmap.width, b->bitmap.rows, hash_bitmap(&b->bitmap), e, mono.width, mono.rows, mono.pitch, hash_bitmap(&mono));
            FT_Bitmap_Done(library, &mono);
            FT_Done_Glyph(bm);
        } else
            FT_Done_Glyph(glyph);
        FT_Done_Glyph(copy);

        /* synthetic styles */
        FT_Load_Glyph(face, g, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP);
        FT_GlyphSlot_Embolden(face->glyph);
        printf("  embolden %lx %ld %ld %ld %ld\n", hash_outline(&face->glyph->outline), face->glyph->metrics.width, face->glyph->metrics.horiAdvance, face->glyph->advance.x, face->glyph->metrics.horiBearingY);
        FT_GlyphSlot_Oblique(face->glyph);
        FT_GlyphSlot_Slant(face->glyph, 0x1000, 0x800);
        FT_GlyphSlot_AdjustWeight(face->glyph, -0x400, 0x200);
        printf("  slanted %lx\n", hash_outline(&face->glyph->outline));
        FT_Load_Glyph(face, g, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_RENDER);
        FT_GlyphSlot_Own_Bitmap(face->glyph);
        FT_GlyphSlot_Embolden(face->glyph);
        printf("  bitmap embolden %u %u %d %lx %d %ld\n", face->glyph->bitmap.width, face->glyph->bitmap.rows, face->glyph->bitmap_top, hash_bitmap(&face->glyph->bitmap), face->glyph->bitmap.pitch, face->glyph->advance.x);
        FT_Load_Glyph(face, g, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP);
        FT_Error e1 = FT_Outline_EmboldenXY(&face->glyph->outline, 90, -40);
        unsigned long h1 = hash_outline(&face->glyph->outline);
        FT_Outline_Reverse(&face->glyph->outline);
        FT_Error e2 = FT_Outline_Embolden(&face->glyph->outline, 64);
        printf("  outline embolden %d %lx %d %lx orient %d\n", e1, h1, e2, hash_outline(&face->glyph->outline), FT_Outline_Get_Orientation(&face->glyph->outline));
    }
    FT_Done_Face(face);
    FT_Done_FreeType(library);
    return 0;
}
