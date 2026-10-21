/* A sweep of a whole font: every face of it (and every named instance of
   a variable one), its records, names and charmaps, and every STEP-th
   glyph (STEP chosen so that about 1500 glyphs of a face are loaded)
   loaded at several sizes and with several load flags, its metrics,
   outline and the bitmaps it renders to (gray, monochrome, LCD; color
   glyphs in color), as one hash a glyph. tools/check_freetype.sh runs it
   on every font of the system, built against the system's FreeType and
   against the library, and the two must print the same. (The library
   has no TrueType bytecode interpreter: TrueType glyphs are loaded
   unhinted here, and the face flag of the hinter is left out for them;
   CFF glyphs are loaded hinted too.)

   sweep FONT [MAX-GLYPHS] */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_TABLES_H
#include FT_ADVANCES_H
#include FT_GLYPH_H
#include FT_FONT_FORMATS_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned long long u64;

static u64 mix(u64 h, u64 v)
{
    h ^= v;
    return h * 1099511628211ULL;
}

static u64 hash_bytes(u64 h, const unsigned char *p, long n)
{
    for (long i = 0; i < n; i++) h = mix(h, p[i]);
    return h;
}

static u64 hash_bitmap(u64 h, const FT_Bitmap *b)
{
    int bytes = b->pitch < 0 ? -b->pitch : b->pitch;
    h = mix(h, b->rows);
    h = mix(h, b->width);
    h = mix(h, (u64)(long)b->pitch);
    h = mix(h, b->pixel_mode);
    h = mix(h, b->num_grays);
    if (!b->buffer) return mix(h, 7);
    for (unsigned y = 0; y < b->rows; y++) h = hash_bytes(h, b->buffer + (long)y * bytes, bytes);
    return h;
}

static u64 hash_outline(u64 h, const FT_Outline *o)
{
    h = mix(h, (u64)o->n_points);
    h = mix(h, (u64)o->n_contours);
    h = mix(h, (u64)o->flags);
    for (int i = 0; i < o->n_points; i++) {
        h = mix(h, (u64)o->points[i].x);
        h = mix(h, (u64)o->points[i].y);
        h = mix(h, o->tags[i]);
    }
    for (int i = 0; i < o->n_contours; i++) h = mix(h, (u64)o->contours[i]);
    return h;
}

static u64 hash_slot(u64 h, FT_GlyphSlot s)
{
    const FT_Glyph_Metrics *m = &s->metrics;
    h = mix(h, s->format);
    h = mix(h, (u64)m->width);
    h = mix(h, (u64)m->height);
    h = mix(h, (u64)m->horiBearingX);
    h = mix(h, (u64)m->horiBearingY);
    h = mix(h, (u64)m->horiAdvance);
    h = mix(h, (u64)m->vertBearingX);
    h = mix(h, (u64)m->vertBearingY);
    h = mix(h, (u64)m->vertAdvance);
    h = mix(h, (u64)s->linearHoriAdvance);
    h = mix(h, (u64)s->linearVertAdvance);
    h = mix(h, (u64)s->advance.x);
    h = mix(h, (u64)s->advance.y);
    h = mix(h, (u64)s->lsb_delta);
    h = mix(h, (u64)s->rsb_delta);
    h = mix(h, (u64)(long)s->bitmap_left);
    h = mix(h, (u64)(long)s->bitmap_top);
    if (s->format == FT_GLYPH_FORMAT_OUTLINE) h = hash_outline(h, &s->outline);
    if (s->format == FT_GLYPH_FORMAT_BITMAP) h = hash_bitmap(h, &s->bitmap);
    if (s->format == FT_GLYPH_FORMAT_COMPOSITE) h = mix(h, s->num_subglyphs);
    return h;
}

/* What loading glyph `g` with `flags` (then rendering it in `mode`, when
   `mode` >= 0) gives, into `h`. */
static u64 load(FT_Face face, long g, FT_Int32 flags, int mode, u64 h)
{
    FT_Error error = FT_Load_Glyph(face, (FT_UInt)g, flags);
    h = mix(h, (u64)error);
    if (error) return h;
    h = hash_slot(h, face->glyph);
    if (mode >= 0 && face->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
        error = FT_Render_Glyph(face->glyph, (FT_Render_Mode)mode);
        h = mix(h, (u64)error);
        if (!error) {
            h = mix(h, (u64)(long)face->glyph->bitmap_left);
            h = mix(h, (u64)(long)face->glyph->bitmap_top);
            h = hash_bitmap(h, &face->glyph->bitmap);
        }
    }
    return h;
}

static void print_string(const char *label, const char *s)
{
    printf(" %s=%s", label, s ? s : "(null)");
}

static void sweep_face(FT_Library library, const char *path, long index, long max_glyphs)
{
    FT_Face face;
    FT_Error error = FT_New_Face(library, path, index, &face);
    printf("face %ld/%ld: open %d\n", index & 0xFFFF, index >> 16, error);
    if (error) return;
    const char *format = FT_Get_Font_Format(face);
    int cff = format && strcmp(format, "CFF") == 0;
    FT_Int32 hint = cff ? 0 : FT_LOAD_NO_HINTING;
    printf("faces %ld glyphs %ld flags 0x%lx style 0x%lx", face->num_faces, face->num_glyphs, cff ? face->face_flags : face->face_flags & ~FT_FACE_FLAG_HINTER, face->style_flags);
    print_string("format", format);
    print_string("family", face->family_name);
    print_string("style", face->style_name);
    print_string("ps", FT_Get_Postscript_Name(face));
    printf("\n");
    printf("units %u asc %d desc %d height %d max %d %d underline %d %d bbox %ld %ld %ld %ld\n", face->units_per_EM, face->ascender, face->descender, face->height, face->max_advance_width, face->max_advance_height, face->underline_position, face->underline_thickness, face->bbox.xMin, face->bbox.yMin, face->bbox.xMax, face->bbox.yMax);
    for (int i = 0; i < face->num_fixed_sizes; i++) {
        FT_Bitmap_Size *b = &face->available_sizes[i];
        printf("strike %d: %d %d %ld %ld %ld\n", i, b->height, b->width, b->size, b->x_ppem, b->y_ppem);
    }
    {
        u64 h = 14695981039346656037ULL;
        FT_UInt count = FT_Get_Sfnt_Name_Count(face);
        for (FT_UInt i = 0; i < count; i++) {
            FT_SfntName name;
            if (FT_Get_Sfnt_Name(face, i, &name)) continue;
            h = mix(h, name.platform_id);
            h = mix(h, name.encoding_id);
            h = mix(h, name.language_id);
            h = mix(h, name.name_id);
            h = hash_bytes(h, name.string, name.string_len);
        }
        printf("names %u %llx\n", count, h);
    }
    for (int i = 0; i < face->num_charmaps; i++) {
        FT_CharMap c = face->charmaps[i];
        if (FT_Set_Charmap(face, c)) {
            printf("charmap %d: %d %d format %ld not set\n", i, c->platform_id, c->encoding_id, FT_Get_CMap_Format(c));
            continue;
        }
        FT_UInt gindex;
        u64 h = 14695981039346656037ULL;
        long n = 0;
        FT_ULong code = FT_Get_First_Char(face, &gindex);
        while (gindex && n < 200000) {
            h = mix(mix(h, code), gindex);
            code = FT_Get_Next_Char(face, code, &gindex);
            n++;
        }
        printf("charmap %d: %d %d 0x%x format %ld lang %lu chars %ld %llx\n", i, c->platform_id, c->encoding_id, c->encoding, FT_Get_CMap_Format(c), FT_Get_CMap_Language_ID(c), n, h);
    }
    FT_Select_Charmap(face, FT_ENCODING_UNICODE);
    long step = face->num_glyphs / max_glyphs + 1;
    const FT_Int32 flag_sets[] = {
        FT_LOAD_NO_HINTING,
        FT_LOAD_DEFAULT | hint,
        FT_LOAD_TARGET_LIGHT | hint,
        FT_LOAD_NO_HINTING | FT_LOAD_VERTICAL_LAYOUT,
        FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_TARGET_MONO,
        FT_LOAD_COLOR | FT_LOAD_RENDER | hint,
    };
    static const int modes[] = { FT_RENDER_MODE_NORMAL, FT_RENDER_MODE_NORMAL, FT_RENDER_MODE_LIGHT, -1, FT_RENDER_MODE_MONO, -1 };
    static const int pixels[] = { 9, 16, 37 };
    u64 sizes = 14695981039346656037ULL;
    for (unsigned si = 0; si < sizeof pixels / sizeof pixels[0]; si++) {
        if (FT_IS_SCALABLE(face)) error = FT_Set_Pixel_Sizes(face, 0, pixels[si]);
        else error = face->num_fixed_sizes > 0 ? FT_Select_Size(face, (FT_Int)(si % face->num_fixed_sizes)) : 1;
        FT_Size_Metrics *m = &face->size->metrics;
        sizes = mix(sizes, (u64)error);
        sizes = mix(mix(mix(sizes, m->x_ppem), m->y_ppem), (u64)m->x_scale);
        sizes = mix(mix(mix(mix(sizes, (u64)m->ascender), (u64)m->descender), (u64)m->height), (u64)m->max_advance);
    }
    printf("sizes %llx step %ld\n", sizes, step);
    u64 total = 14695981039346656037ULL;
    for (long g = 0; g < face->num_glyphs; g += step) {
        u64 h = 14695981039346656037ULL;
        h = load(face, g, FT_LOAD_NO_SCALE, -1, h);
        for (unsigned si = 0; si < sizeof pixels / sizeof pixels[0]; si++) {
            if (FT_IS_SCALABLE(face)) FT_Set_Pixel_Sizes(face, 0, pixels[si]);
            else if (face->num_fixed_sizes > 0) FT_Select_Size(face, (FT_Int)(si % face->num_fixed_sizes));
            for (unsigned fi = 0; fi < sizeof flag_sets / sizeof flag_sets[0]; fi++) {
                /* (The large size: unhinted only.) */
                if (si == 2 && fi != 0 && fi != 5) continue;
                h = load(face, g, flag_sets[fi], modes[fi], h);
            }
            /* LCD, at the middle size. */
            if (si == 1) h = load(face, g, FT_LOAD_NO_HINTING | FT_LOAD_TARGET_LCD, FT_RENDER_MODE_LCD, h);
        }
        {
            char name[128];
            if (FT_HAS_GLYPH_NAMES(face) && !FT_Get_Glyph_Name(face, (FT_UInt)g, name, sizeof name)) h = hash_bytes(h, (const unsigned char *)name, (long)strlen(name));
        }
        printf("g %ld %llx\n", g, h);
        total = mix(total, h);
    }
    {
        FT_Fixed advances[256];
        long n = face->num_glyphs < 256 ? face->num_glyphs : 256;
        u64 h = 14695981039346656037ULL;
        if (FT_IS_SCALABLE(face)) FT_Set_Pixel_Sizes(face, 0, 16);
        const FT_Int32 advance_flags[] = { FT_LOAD_NO_SCALE, FT_LOAD_NO_HINTING, FT_LOAD_DEFAULT | hint, FT_LOAD_NO_HINTING | FT_LOAD_VERTICAL_LAYOUT };
        for (unsigned fi = 0; fi < sizeof advance_flags / sizeof advance_flags[0]; fi++) {
            error = FT_Get_Advances(face, 0, (FT_UInt)n, advance_flags[fi], advances);
            h = mix(h, (u64)error);
            if (!error)
                for (long i = 0; i < n; i++) h = mix(h, (u64)advances[i]);
        }
        /* Kerning of the first glyphs' pairs. */
        long k = face->num_glyphs < 48 ? face->num_glyphs : 48;
        for (long a = 0; a < k; a++)
            for (long b = 0; b < k; b++) {
                FT_Vector v;
                error = FT_Get_Kerning(face, (FT_UInt)a, (FT_UInt)b, FT_KERNING_DEFAULT, &v);
                h = mix(mix(mix(h, (u64)error), (u64)v.x), (u64)v.y);
            }
        printf("advances %llx\n", h);
    }
    printf("total %llx\n", total);
    long instances = face->style_flags >> 16;
    long faces = face->num_faces;
    FT_Done_Face(face);
    /* The named instances of the first face. */
    if ((index & 0xFFFF0000) == 0)
        for (long i = 1; i <= instances && i <= 8; i++) sweep_face(library, path, (i << 16) | index, max_glyphs / 4 + 1);
    if (index == 0)
        for (long i = 1; i < faces && i < 4; i++) sweep_face(library, path, i, max_glyphs);
}

int main(int argc, char **argv)
{
    if (argc < 2) return 2;
    long max_glyphs = argc > 2 ? atol(argv[2]) : 1500;
    FT_Library library;
    FT_Init_FreeType(&library);
    sweep_face(library, argv[1], 0, max_glyphs);
    FT_Done_FreeType(library);
    return 0;
}
