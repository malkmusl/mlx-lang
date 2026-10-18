/* Variable fonts as FreeType sees them: the axes and named instances of
   FT_Get_MM_Var, the faces of the named instances, design and normalized
   coordinates set and read back, and the glyphs, advances and global
   metrics at several places of the design space (gvar, HVAR, MVAR and
   avar for TrueType outlines, the CFF2 blend for CFF2 ones).

   The face flags are printed without FT_FACE_FLAG_HINTER, which FreeType
   sets for TrueType faces when it has its bytecode interpreter. */
#define FLAGS(face) ((face)->face_flags & ~FT_FACE_FLAG_HINTER)
/*

   variations FONT [STEP]   (every STEP-th glyph, default 1) */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H
#include FT_TRUETYPE_TABLES_H
#include FT_ADVANCES_H
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

static unsigned long hash_bitmap(const FT_Bitmap *b)
{
    unsigned long h = 0;
    int bytes = b->pitch < 0 ? -b->pitch : b->pitch;
    for (unsigned y = 0; y < b->rows; y++)
        for (int x = 0; x < bytes; x++) h = h * 131 + b->buffer[y * bytes + x];
    return h;
}

static void print_coords(FT_Face face, unsigned n)
{
    FT_Fixed design[16], blend[16];
    memset(design, 0x55, sizeof design);
    memset(blend, 0x55, sizeof blend);
    FT_Error e1 = FT_Get_Var_Design_Coordinates(face, n, design);
    FT_Error e2 = FT_Get_Var_Blend_Coordinates(face, n, blend);
    printf("  coords %d %d:", e1, e2);
    for (unsigned i = 0; i < n; i++) printf(" %ld/%ld", design[i], blend[i]);
    printf("\n");
}

static void print_state(FT_Face face, long step)
{
    printf("  face flags 0x%lx style 0x%lx index 0x%lx asc %d desc %d height %d maxadv %d underline %d %d ps '%s'\n", FLAGS(face), face->style_flags, face->face_index, face->ascender, face->descender, face->height, face->max_advance_width, face->underline_position, face->underline_thickness, FT_Get_Postscript_Name(face) ? FT_Get_Postscript_Name(face) : "(null)");
    TT_OS2 *os2 = FT_Get_Sfnt_Table(face, FT_SFNT_OS2);
    if (os2) printf("  os2 x %d cap %d strike %d %d sub %d %d %d %d sup %d %d %d %d typo %d %d %d win %u %u\n", os2->sxHeight, os2->sCapHeight, os2->yStrikeoutSize, os2->yStrikeoutPosition, os2->ySubscriptXSize, os2->ySubscriptYSize, os2->ySubscriptXOffset, os2->ySubscriptYOffset, os2->ySuperscriptXSize, os2->ySuperscriptYSize, os2->ySuperscriptXOffset, os2->ySuperscriptYOffset, os2->sTypoAscender, os2->sTypoDescender, os2->sTypoLineGap, os2->usWinAscent, os2->usWinDescent);
    TT_HoriHeader *hhea = FT_Get_Sfnt_Table(face, FT_SFNT_HHEA);
    if (hhea) printf("  hhea %d %d %d caret %d %d %d\n", hhea->Ascender, hhea->Descender, hhea->Line_Gap, hhea->caret_Slope_Rise, hhea->caret_Slope_Run, hhea->caret_Offset);
    TT_Postscript *post = FT_Get_Sfnt_Table(face, FT_SFNT_POST);
    if (post) printf("  post %d %d\n", post->underlinePosition, post->underlineThickness);
    static const FT_Int32 flag_sets[] = { FT_LOAD_NO_SCALE, FT_LOAD_NO_HINTING, FT_LOAD_DEFAULT, FT_LOAD_NO_HINTING | FT_LOAD_LINEAR_DESIGN };
    static const int pixel_sizes[] = { 0, 13, 40 };
    for (unsigned si = 0; si < sizeof pixel_sizes / sizeof pixel_sizes[0]; si++) {
        if (pixel_sizes[si]) {
            FT_Error error = FT_Set_Pixel_Sizes(face, pixel_sizes[si], pixel_sizes[si]);
            FT_Size_Metrics *sm = &face->size->metrics;
            printf("  size %d: %d ppem %u %u scale %ld %ld asc %ld desc %ld height %ld max %ld\n", pixel_sizes[si], error, sm->x_ppem, sm->y_ppem, sm->x_scale, sm->y_scale, sm->ascender, sm->descender, sm->height, sm->max_advance);
        }
        for (unsigned fi = 0; fi < sizeof flag_sets / sizeof flag_sets[0]; fi++) {
            FT_Int32 flags = flag_sets[fi];
            if ((pixel_sizes[si] == 0) != (flags == FT_LOAD_NO_SCALE)) continue;
            /* TrueType hinting is the bytecode interpreter's: not compared. */
            if (flags == FT_LOAD_DEFAULT && !(face->face_flags & FT_FACE_FLAG_CID_KEYED) && FT_Get_Sfnt_Table(face, FT_SFNT_MAXP) && ((TT_MaxProfile *)FT_Get_Sfnt_Table(face, FT_SFNT_MAXP))->version == 0x10000) continue;
            printf("  flags 0x%x\n", flags);
            for (long g = 0; g < face->num_glyphs; g += step) {
                FT_Error error = FT_Load_Glyph(face, g, flags);
                FT_GlyphSlot s = face->glyph;
                if (error) {
                    printf("   %ld: error %d\n", g, error);
                    continue;
                }
                FT_Glyph_Metrics *mt = &s->metrics;
                printf("   %ld: m %ld %ld %ld %ld %ld %ld %ld %ld lin %ld %ld adv %ld %ld o %d %d %lx", g, mt->width, mt->height, mt->horiBearingX, mt->horiBearingY, mt->horiAdvance, mt->vertBearingX, mt->vertBearingY, mt->vertAdvance, s->linearHoriAdvance, s->linearVertAdvance, s->advance.x, s->advance.y, s->outline.n_points, s->outline.n_contours, hash_outline(&s->outline));
                if (flags != FT_LOAD_NO_SCALE && s->format == FT_GLYPH_FORMAT_OUTLINE) {
                    error = FT_Render_Glyph(s, FT_RENDER_MODE_NORMAL);
                    printf(" r %d %d %d %u %u %lx", error, s->bitmap_left, s->bitmap_top, s->bitmap.width, s->bitmap.rows, hash_bitmap(&s->bitmap));
                }
                printf("\n");
            }
            FT_Fixed adv[64];
            long n = face->num_glyphs < 64 ? face->num_glyphs : 64;
            FT_Error e = FT_Get_Advances(face, 0, n, flags, adv);
            printf("   advances %d:", e);
            for (long i = 0; i < n; i++) printf(" %ld", adv[i]);
            printf("\n");
        }
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) return 2;
    long step = argc > 2 ? atol(argv[2]) : 1;
    FT_Library library;
    FT_Face face;
    FT_Init_FreeType(&library);
    FT_Error error = FT_New_Face(library, argv[1], 0, &face);
    printf("open %d\n", error);
    if (error) return 1;
    printf("faces %ld flags 0x%lx style 0x%lx mm %d named %d variation %d\n", face->num_faces, FLAGS(face), face->style_flags, FT_HAS_MULTIPLE_MASTERS(face) != 0, FT_IS_NAMED_INSTANCE(face) != 0, FT_IS_VARIATION(face) != 0);
    FT_MM_Var *mm = NULL;
    error = FT_Get_MM_Var(face, &mm);
    printf("mm var %d\n", error);
    if (error) return 1;
    printf("axes %u designs %u named %u\n", mm->num_axis, mm->num_designs, mm->num_namedstyles);
    for (unsigned i = 0; i < mm->num_axis; i++) {
        FT_Var_Axis *a = &mm->axis[i];
        printf(" axis %u '%s' %ld %ld %ld tag %lx strid %u\n", i, a->name, a->minimum, a->def, a->maximum, a->tag, a->strid);
    }
    for (unsigned i = 0; i < mm->num_namedstyles; i++) {
        FT_Var_Named_Style *s = &mm->namedstyle[i];
        printf(" style %u strid %u psid %u:", i, s->strid, s->psid);
        for (unsigned j = 0; j < mm->num_axis; j++) printf(" %ld", s->coords[j]);
        printf("\n");
    }
    FT_UInt flags = 0;
    for (unsigned i = 0; i < mm->num_axis; i++) {
        error = FT_Get_Var_Axis_Flags(mm, i, &flags);
        printf(" axis flags %u: %d 0x%x\n", i, error, flags);
    }
    error = FT_Get_Var_Axis_Flags(mm, mm->num_axis, &flags);
    printf(" axis flags beyond: %d\n", error);
    FT_UInt def = 99;
    error = FT_Get_Default_Named_Instance(face, &def);
    printf("default instance %d %u\n", error, def);
    FT_Multi_Master master;
    error = FT_Get_Multi_Master(face, &master);
    printf("multi master %d\n", error);
    unsigned n = mm->num_axis;

    printf("default\n");
    print_coords(face, n);
    print_state(face, step);

    /* named instances opened as faces */
    for (unsigned i = 0; i <= mm->num_namedstyles + 1; i++) {
        FT_Face f;
        error = FT_New_Face(library, argv[1], (long)i << 16, &f);
        printf("instance face %u: %d", i, error);
        if (!error) {
            printf(" family '%s' style '%s' index 0x%lx style flags 0x%lx named %d variation %d ps '%s'\n", f->family_name, f->style_name, f->face_index, f->style_flags, FT_IS_NAMED_INSTANCE(f) != 0, FT_IS_VARIATION(f) != 0, FT_Get_Postscript_Name(f) ? FT_Get_Postscript_Name(f) : "(null)");
            print_coords(f, n);
            if (i == 3) print_state(f, step);
            FT_Done_Face(f);
        } else printf("\n");
    }

    /* design coordinates */
    static const FT_Fixed design_sets[][4] = {
        { 650 << 16, 100 << 16, 0, 0 },
        { 250 << 16, 80 << 16, 0, 0 },
        { 2000 << 16, 10 << 16, 0, 0 },
        { 400 << 16, 100 << 16, 0, 0 },
        { 900 << 16, 75 << 16, 0, 0 },
        { 0x2A3B5C7, 0x5F0000, 0, 0 },
    };
    for (unsigned i = 0; i < sizeof design_sets / sizeof design_sets[0]; i++) {
        FT_Fixed c[4];
        memcpy(c, design_sets[i], sizeof c);
        error = FT_Set_Var_Design_Coordinates(face, n, c);
        printf("design set %u: %d\n", i, error);
        print_coords(face, n);
        printf("  flags 0x%lx variation %d named %d ps '%s'\n", FLAGS(face), FT_IS_VARIATION(face) != 0, FT_IS_NAMED_INSTANCE(face) != 0, FT_Get_Postscript_Name(face) ? FT_Get_Postscript_Name(face) : "(null)");
        if (i == 0 || i == 2 || i == 5) print_state(face, step);
    }
    FT_Fixed one = 300 << 16;
    error = FT_Set_Var_Design_Coordinates(face, 1, &one);
    printf("design one axis: %d\n", error);
    print_coords(face, n);
    error = FT_Set_Var_Design_Coordinates(face, 0, NULL);
    printf("design none: %d\n", error);
    print_coords(face, n);

    /* normalized coordinates */
    static const FT_Fixed blend_sets[][4] = {
        { 0x8000, -0x4000, 0, 0 },
        { -0x10000, 0, 0, 0 },
        { 0x6666, 0, 0, 0 },
        { 0x30000, -0x30000, 0, 0 },
    };
    for (unsigned i = 0; i < sizeof blend_sets / sizeof blend_sets[0]; i++) {
        FT_Fixed c[4];
        memcpy(c, blend_sets[i], sizeof c);
        error = FT_Set_Var_Blend_Coordinates(face, n, c);
        printf("blend set %u: %d\n", i, error);
        print_coords(face, n);
        printf("  ps '%s'\n", FT_Get_Postscript_Name(face) ? FT_Get_Postscript_Name(face) : "(null)");
        if (i == 0 || i == 2) print_state(face, step);
    }
    FT_Fixed mmc[4] = { 0x4000, 0x8000, 0, 0 };
    error = FT_Set_MM_Blend_Coordinates(face, n, mmc);
    printf("mm blend set: %d\n", error);
    print_coords(face, n);
    memset(mmc, 0, sizeof mmc);
    error = FT_Get_MM_Blend_Coordinates(face, n, mmc);
    printf("mm blend get: %d %ld %ld\n", error, mmc[0], mmc[1]);

    /* named instances set on the face */
    for (unsigned i = 0; i <= mm->num_namedstyles + 1; i++) {
        error = FT_Set_Named_Instance(face, i);
        printf("set named %u: %d index 0x%lx named %d variation %d ps '%s'\n", i, error, face->face_index, FT_IS_NAMED_INSTANCE(face) != 0, FT_IS_VARIATION(face) != 0, FT_Get_Postscript_Name(face) ? FT_Get_Postscript_Name(face) : "(null)");
        print_coords(face, n);
    }
    FT_Set_Named_Instance(face, 3);
    print_state(face, step);
    FT_Done_MM_Var(library, mm);
    FT_Done_Face(face);
    FT_Done_FreeType(library);
    return 0;
}
