/* Glyphs rendered as FreeType renders them: gray, monochrome and LCD
   (both directions, with the default, light, legacy and no filter, and
   a face's own weights) bitmaps of unhinted glyphs at several sizes, as
   hashes of their rows; FT_Outline_Get_Bitmap and FT_Outline_Render with
   spans; even-odd filling, overlapping contours, origins.

   render FONT [FACE-INDEX [STEP]] */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_LCD_FILTER_H
#include FT_BITMAP_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long hash_bitmap(const FT_Bitmap *b)
{
    unsigned long h = 0;
    int bytes = b->pixel_mode == FT_PIXEL_MODE_MONO ? (int)(b->width + 7) / 8 : b->pixel_mode == FT_PIXEL_MODE_BGRA ? (int)b->width * 4 : (int)b->width;
    for (unsigned r = 0; r < b->rows; r++) {
        const unsigned char *row = b->buffer + (b->pitch >= 0 ? (long)r * b->pitch : (long)(b->rows - 1 - r) * -b->pitch);
        for (int i = 0; i < bytes; i++) h = h * 31 + row[i];
    }
    return h;
}

static void show(const FT_Bitmap *b)
{
    for (unsigned r = 0; r < b->rows; r++) {
        for (unsigned c = 0; c < b->width; c++) {
            int v = b->pixel_mode == FT_PIXEL_MODE_MONO ? ((b->buffer[r * b->pitch + c / 8] >> (7 - c % 8)) & 1) * 255 : b->buffer[r * b->pitch + c];
            putchar(" .:-=+*#%@"[v * 9 / 255]);
        }
        putchar('\n');
    }
}

static void render(FT_Face face, long g, FT_Int32 flags, FT_Render_Mode mode)
{
    FT_Error error = FT_Load_Glyph(face, g, flags);
    if (!error) error = FT_Render_Glyph(face->glyph, mode);
    FT_GlyphSlot s = face->glyph;
    if (error) {
        printf("  %ld: error %d\n", g, error);
        return;
    }
    printf("  %ld: %d %d %u %u %d %d %lx\n", g, s->bitmap_left, s->bitmap_top, s->bitmap.width, s->bitmap.rows, s->bitmap.pitch, s->bitmap.pixel_mode, hash_bitmap(&s->bitmap));
    if (getenv("SHOW")) show(&s->bitmap);
}

struct spans { unsigned long hash; int calls; };

static void span_func(int y, int count, const FT_Span *spans, void *user)
{
    struct spans *s = user;
    s->calls++;
    s->hash = s->hash * 31 + (unsigned)y;
    s->hash = s->hash * 31 + (unsigned)count;
    for (int i = 0; i < count; i++) {
        s->hash = s->hash * 31 + (unsigned short)spans[i].x;
        s->hash = s->hash * 31 + spans[i].len;
        s->hash = s->hash * 31 + spans[i].coverage;
    }
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
    static const int pixel_sizes[] = { 9, 13, 16, 24, 50, 160 };
    for (unsigned si = 0; si < sizeof pixel_sizes / sizeof pixel_sizes[0]; si++) {
        FT_Set_Pixel_Sizes(face, 0, pixel_sizes[si]);
        printf("size %d\n", pixel_sizes[si]);
        long n = face->num_glyphs;
        long gstep = pixel_sizes[si] > 60 ? step * 5 : step;
        printf(" gray\n");
        for (long g = 0; g < n; g += gstep) render(face, g, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP, FT_RENDER_MODE_NORMAL);
        printf(" mono\n");
        for (long g = 0; g < n; g += gstep) render(face, g, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_TARGET_MONO, FT_RENDER_MODE_MONO);
        printf(" lcd\n");
        for (long g = 0; g < n; g += gstep * 3) render(face, g, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_TARGET_LCD, FT_RENDER_MODE_LCD);
        printf(" lcd-v\n");
        for (long g = 0; g < n; g += gstep * 3) render(face, g, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_TARGET_LCD_V, FT_RENDER_MODE_LCD_V);
        printf(" render-flag\n");
        for (long g = 0; g < n; g += gstep * 5) {
            error = FT_Load_Glyph(face, g, FT_LOAD_NO_HINTING | FT_LOAD_RENDER);
            printf("  %ld: %d %d %d %u %u %lx\n", g, error, face->glyph->bitmap_left, face->glyph->bitmap_top, face->glyph->bitmap.width, face->glyph->bitmap.rows, hash_bitmap(&face->glyph->bitmap));
        }
    }
    FT_Set_Pixel_Sizes(face, 0, 20);
    static const FT_LcdFilter filters[] = { FT_LCD_FILTER_NONE, FT_LCD_FILTER_LIGHT, FT_LCD_FILTER_LEGACY, FT_LCD_FILTER_LEGACY1, FT_LCD_FILTER_DEFAULT };
    for (unsigned fi = 0; fi < sizeof filters / sizeof filters[0]; fi++) {
        printf("filter %d: %d\n", filters[fi], FT_Library_SetLcdFilter(library, filters[fi]));
        for (long g = 0; g < face->num_glyphs; g += step * 9) {
            render(face, g, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP, FT_RENDER_MODE_LCD);
            render(face, g, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP, FT_RENDER_MODE_LCD_V);
        }
    }
    {
        unsigned char weights[5] = { 0x10, 0x40, 0x70, 0x40, 0x10 };
        printf("weights %d\n", FT_Library_SetLcdFilterWeights(library, weights));
        for (long g = 0; g < face->num_glyphs; g += step * 9) render(face, g, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP, FT_RENDER_MODE_LCD);
        unsigned char face_weights[5] = { 0x00, 0x30, 0xA0, 0x30, 0x00 };
        FT_Parameter property = { FT_PARAM_TAG_LCD_FILTER_WEIGHTS, face_weights };
        printf("face weights %d\n", FT_Face_Properties(face, 1, &property));
        for (long g = 0; g < face->num_glyphs; g += step * 9) render(face, g, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP, FT_RENDER_MODE_LCD);
        FT_Library_SetLcdFilter(library, FT_LCD_FILTER_DEFAULT);
    }
    /* outlines rendered directly: a bitmap of FT_Outline_Get_Bitmap, the spans
       of FT_Outline_Render, even-odd, and an outline with overlapping contours */
    FT_Set_Pixel_Sizes(face, 0, 30);
    printf("outline rendering\n");
    for (long g = 0; g < face->num_glyphs; g += step * 4) {
        if (FT_Load_Glyph(face, g, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP) || face->glyph->format != FT_GLYPH_FORMAT_OUTLINE) continue;
        FT_Outline *o = &face->glyph->outline;
        FT_BBox box;
        FT_Outline_Get_CBox(o, &box);
        FT_Bitmap b;
        FT_Bitmap_Init(&b);
        b.width = (unsigned)((box.xMax - box.xMin) >> 6) + 3;
        b.rows = (unsigned)((box.yMax - box.yMin) >> 6) + 3;
        b.pitch = (int)b.width;
        b.pixel_mode = FT_PIXEL_MODE_GRAY;
        b.num_grays = 256;
        b.buffer = calloc(b.rows, b.width);
        FT_Outline_Translate(o, -box.xMin + 32, -box.yMin + 40);
        FT_Error e1 = FT_Outline_Get_Bitmap(library, o, &b);
        unsigned long h1 = hash_bitmap(&b);
        memset(b.buffer, 0, b.rows * b.width);
        o->flags |= FT_OUTLINE_EVEN_ODD_FILL;
        FT_Error e2 = FT_Outline_Get_Bitmap(library, o, &b);
        unsigned long h2 = hash_bitmap(&b);
        o->flags &= ~FT_OUTLINE_EVEN_ODD_FILL;
        FT_Bitmap mono = b;
        mono.pixel_mode = FT_PIXEL_MODE_MONO;
        mono.pitch = (int)((b.width + 15) / 16 * 2);
        mono.buffer = calloc(mono.rows, (size_t)mono.pitch);
        FT_Error e3 = FT_Outline_Get_Bitmap(library, o, &mono);
        unsigned long h3 = hash_bitmap(&mono);
        free(mono.buffer);
        b.pitch = -b.pitch;
        memset(b.buffer, 0, b.rows * b.width);
        FT_Error e4 = FT_Outline_Get_Bitmap(library, o, &b);
        unsigned long h4 = hash_bitmap(&b);
        free(b.buffer);
        struct spans spans = { 0, 0 };
        FT_Raster_Params params;
        memset(&params, 0, sizeof params);
        params.flags = FT_RASTER_FLAG_AA | FT_RASTER_FLAG_DIRECT;
        params.gray_spans = span_func;
        params.user = &spans;
        FT_Error e5 = FT_Outline_Render(library, o, &params);
        struct spans clipped = { 0, 0 };
        params.flags = FT_RASTER_FLAG_AA | FT_RASTER_FLAG_DIRECT | FT_RASTER_FLAG_CLIP;
        params.user = &clipped;
        params.clip_box.xMin = 5;
        params.clip_box.yMin = 3;
        params.clip_box.xMax = 15;
        params.clip_box.yMax = 12;
        FT_Error e6 = FT_Outline_Render(library, o, &params);
        printf("  %ld: %d %lx %d %lx %d %lx %d %lx spans %d %d %lx clipped %d %d %lx\n", g, e1, h1, e2, h2, e3, h3, e4, h4, e5, spans.calls, spans.hash, e6, clipped.calls, clipped.hash);
    }
    printf("overlap\n");
    for (long g = 0; g < face->num_glyphs; g += step * 11) {
        if (FT_Load_Glyph(face, g, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP) || face->glyph->format != FT_GLYPH_FORMAT_OUTLINE) continue;
        face->glyph->outline.flags |= FT_OUTLINE_OVERLAP;
        error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
        printf("  %ld: %d %u %u %lx\n", g, error, face->glyph->bitmap.width, face->glyph->bitmap.rows, hash_bitmap(&face->glyph->bitmap));
    }
    printf("origin\n");
    for (long g = 0; g < face->num_glyphs; g += step * 11) {
        FT_Glyph_Metrics m;
        if (FT_Load_Glyph(face, g, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP)) continue;
        m = face->glyph->metrics;
        FT_Outline_Translate(&face->glyph->outline, 17, -23);
        error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_LIGHT);
        printf("  %ld: %d %ld %d %d %u %u %lx\n", g, error, m.width, face->glyph->bitmap_left, face->glyph->bitmap_top, face->glyph->bitmap.width, face->glyph->bitmap.rows, hash_bitmap(&face->glyph->bitmap));
    }
    FT_Done_Face(face);
    FT_Done_FreeType(library);
    return 0;
}
