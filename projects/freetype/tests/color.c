/* Color fonts as FreeType reads them: the palettes of `CPAL` (selected,
   with their names and flags), the layers of `COLR` version 0 and the
   glyphs rendered with them in BGRA (also in a foreground color), and
   the paints of `COLR` version 1 walked from each glyph (gradients with
   their color stops, transforms, compositions, layers, clip boxes).

   color FONT [FACE-INDEX] */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_COLOR_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long hash_bitmap(const FT_Bitmap *b)
{
    unsigned long h = 0;
    int bytes = b->pitch < 0 ? -b->pitch : b->pitch;
    for (unsigned y = 0; y < b->rows; y++)
        for (int x = 0; x < bytes; x++) h = h * 131 + b->buffer[y * bytes + x];
    return h;
}

static void print_line(FT_Face face, FT_ColorLine *line)
{
    printf(" extend %d stops %u:", line->extend, line->color_stop_iterator.num_color_stops);
    FT_ColorStop stop;
    while (FT_Get_Colorline_Stops(face, &stop, &line->color_stop_iterator))
        printf(" (%ld %u %d)", stop.stop_offset, stop.color.palette_index, stop.color.alpha);
}

static void walk(FT_Face face, FT_OpaquePaint opaque, int depth)
{
    FT_COLR_Paint paint;
    printf("%*s", depth * 2 + 2, "");
    if (depth > 12) {
        printf("(too deep)\n");
        return;
    }
    memset(&paint, 0x55, sizeof paint);
    if (!FT_Get_Paint(face, opaque, &paint)) {
        printf("no paint\n");
        return;
    }
    switch (paint.format) {
    case FT_COLR_PAINTFORMAT_COLR_LAYERS: {
        printf("layers %u\n", paint.u.colr_layers.layer_iterator.num_layers);
        FT_OpaquePaint layer = { NULL, 0 };
        while (FT_Get_Paint_Layers(face, &paint.u.colr_layers.layer_iterator, &layer)) walk(face, layer, depth + 1);
        break;
    }
    case FT_COLR_PAINTFORMAT_SOLID:
        printf("solid %u %d\n", paint.u.solid.color.palette_index, paint.u.solid.color.alpha);
        break;
    case FT_COLR_PAINTFORMAT_LINEAR_GRADIENT:
        printf("linear %ld %ld %ld %ld %ld %ld", paint.u.linear_gradient.p0.x, paint.u.linear_gradient.p0.y, paint.u.linear_gradient.p1.x, paint.u.linear_gradient.p1.y, paint.u.linear_gradient.p2.x, paint.u.linear_gradient.p2.y);
        print_line(face, &paint.u.linear_gradient.colorline);
        printf("\n");
        break;
    case FT_COLR_PAINTFORMAT_RADIAL_GRADIENT:
        printf("radial %ld %ld %ld %ld %ld %ld", paint.u.radial_gradient.c0.x, paint.u.radial_gradient.c0.y, paint.u.radial_gradient.r0, paint.u.radial_gradient.c1.x, paint.u.radial_gradient.c1.y, paint.u.radial_gradient.r1);
        print_line(face, &paint.u.radial_gradient.colorline);
        printf("\n");
        break;
    case FT_COLR_PAINTFORMAT_SWEEP_GRADIENT:
        printf("sweep %ld %ld %ld %ld", paint.u.sweep_gradient.center.x, paint.u.sweep_gradient.center.y, paint.u.sweep_gradient.start_angle, paint.u.sweep_gradient.end_angle);
        print_line(face, &paint.u.sweep_gradient.colorline);
        printf("\n");
        break;
    case FT_COLR_PAINTFORMAT_GLYPH:
        printf("glyph %u\n", paint.u.glyph.glyphID);
        walk(face, paint.u.glyph.paint, depth + 1);
        break;
    case FT_COLR_PAINTFORMAT_COLR_GLYPH: {
        printf("colr glyph %u\n", paint.u.colr_glyph.glyphID);
        FT_OpaquePaint sub = { NULL, 0 };
        if (FT_Get_Color_Glyph_Paint(face, paint.u.colr_glyph.glyphID, FT_COLOR_NO_ROOT_TRANSFORM, &sub)) walk(face, sub, depth + 1);
        break;
    }
    case FT_COLR_PAINTFORMAT_TRANSFORM:
        printf("transform %ld %ld %ld %ld %ld %ld\n", paint.u.transform.affine.xx, paint.u.transform.affine.xy, paint.u.transform.affine.dx, paint.u.transform.affine.yx, paint.u.transform.affine.yy, paint.u.transform.affine.dy);
        walk(face, paint.u.transform.paint, depth + 1);
        break;
    case FT_COLR_PAINTFORMAT_TRANSLATE:
        printf("translate %ld %ld\n", paint.u.translate.dx, paint.u.translate.dy);
        walk(face, paint.u.translate.paint, depth + 1);
        break;
    case FT_COLR_PAINTFORMAT_SCALE:
        printf("scale %ld %ld %ld %ld\n", paint.u.scale.scale_x, paint.u.scale.scale_y, paint.u.scale.center_x, paint.u.scale.center_y);
        walk(face, paint.u.scale.paint, depth + 1);
        break;
    case FT_COLR_PAINTFORMAT_ROTATE:
        printf("rotate %ld %ld %ld\n", paint.u.rotate.angle, paint.u.rotate.center_x, paint.u.rotate.center_y);
        walk(face, paint.u.rotate.paint, depth + 1);
        break;
    case FT_COLR_PAINTFORMAT_SKEW:
        printf("skew %ld %ld %ld %ld\n", paint.u.skew.x_skew_angle, paint.u.skew.y_skew_angle, paint.u.skew.center_x, paint.u.skew.center_y);
        walk(face, paint.u.skew.paint, depth + 1);
        break;
    case FT_COLR_PAINTFORMAT_COMPOSITE:
        printf("composite %d\n", paint.u.composite.composite_mode);
        walk(face, paint.u.composite.source_paint, depth + 1);
        walk(face, paint.u.composite.backdrop_paint, depth + 1);
        break;
    default:
        printf("format %d\n", paint.format);
    }
}

static void render_all(FT_Face face, const char *what)
{
    printf("%s\n", what);
    for (long g = 0; g < face->num_glyphs; g++) {
        FT_Error error = FT_Load_Glyph(face, g, FT_LOAD_COLOR | FT_LOAD_RENDER | FT_LOAD_NO_HINTING);
        FT_GlyphSlot s = face->glyph;
        printf(" %ld: %d fmt %c%c%c%c mode %d %d %d %u %u %d %lx\n", g, error, (char)(s->format >> 24), (char)(s->format >> 16), (char)(s->format >> 8), (char)s->format, s->bitmap.pixel_mode, s->bitmap_left, s->bitmap_top, s->bitmap.width, s->bitmap.rows, s->bitmap.pitch, hash_bitmap(&s->bitmap));
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) return 2;
    long index = argc > 2 ? atol(argv[2]) : 0;
    FT_Library library;
    FT_Face face;
    FT_Init_FreeType(&library);
    FT_Error error = FT_New_Face(library, argv[1], index, &face);
    printf("open %d color %d\n", error, error ? 0 : FT_HAS_COLOR(face) != 0);
    if (error) return 1;
    FT_Palette_Data data;
    memset(&data, 0x55, sizeof data);
    error = FT_Palette_Data_Get(face, &data);
    printf("palette data %d: palettes %u entries %u\n", error, data.num_palettes, data.num_palette_entries);
    for (unsigned i = 0; i < data.num_palettes; i++)
        printf(" palette %u name %d flags %d\n", i, data.palette_name_ids ? data.palette_name_ids[i] : -1, data.palette_flags ? data.palette_flags[i] : -1);
    for (unsigned i = 0; i < data.num_palette_entries; i++)
        printf(" entry %u name %d\n", i, data.palette_entry_name_ids ? data.palette_entry_name_ids[i] : -1);
    for (unsigned i = 0; i <= data.num_palettes; i++) {
        FT_Color *palette = NULL;
        error = FT_Palette_Select(face, (FT_UShort)i, &palette);
        printf(" select %u: %d", i, error);
        for (unsigned j = 0; palette && j < data.num_palette_entries; j++) printf(" %02x%02x%02x%02x", palette[j].red, palette[j].green, palette[j].blue, palette[j].alpha);
        printf("\n");
    }
    FT_Palette_Select(face, 0, NULL);
    printf("layers\n");
    for (long g = 0; g <= face->num_glyphs; g++) {
        FT_LayerIterator it;
        it.p = NULL;
        FT_UInt glyph, color;
        int n = 0;
        while (FT_Get_Color_Glyph_Layer(face, (FT_UInt)g, &glyph, &color, &it)) {
            if (!n++) printf(" %ld:", g);
            printf(" (%u %u)", glyph, color);
        }
        if (n) printf(" [%u %u]\n", it.num_layers, it.layer);
    }
    FT_Set_Pixel_Sizes(face, 0, 40);
    render_all(face, "render");
    FT_Palette_Select(face, 1, NULL);
    render_all(face, "render palette 1");
    FT_Color fg = { 0x10, 0x80, 0xF0, 0xC0 };
    FT_Palette_Set_Foreground_Color(face, fg);
    render_all(face, "render foreground");
    FT_Set_Pixel_Sizes(face, 0, 13);
    printf("paints\n");
    for (long g = 0; g < face->num_glyphs; g++) {
        for (int root = 0; root < 2; root++) {
            FT_OpaquePaint opaque = { NULL, 0 };
            if (!FT_Get_Color_Glyph_Paint(face, (FT_UInt)g, root ? FT_COLOR_NO_ROOT_TRANSFORM : FT_COLOR_INCLUDE_ROOT_TRANSFORM, &opaque)) continue;
            printf(" %ld root %d: insert %d\n", g, root, opaque.insert_root_transform);
            walk(face, opaque, 0);
        }
        FT_ClipBox box;
        if (FT_Get_Color_Glyph_ClipBox(face, (FT_UInt)g, &box))
            printf(" %ld clip %ld %ld %ld %ld %ld %ld %ld %ld\n", g, box.bottom_left.x, box.bottom_left.y, box.top_left.x, box.top_left.y, box.top_right.x, box.top_right.y, box.bottom_right.x, box.bottom_right.y);
    }
    FT_Matrix matrix = { 0xB505, -0xB505, 0xB505, 0xB505 };
    FT_Vector delta = { 100, -50 };
    FT_Set_Transform(face, &matrix, &delta);
    printf("transformed\n");
    for (long g = 0; g < face->num_glyphs; g++) {
        FT_OpaquePaint opaque = { NULL, 0 };
        if (FT_Get_Color_Glyph_Paint(face, (FT_UInt)g, FT_COLOR_INCLUDE_ROOT_TRANSFORM, &opaque)) {
            printf(" %ld:\n", g);
            walk(face, opaque, 0);
        }
        FT_ClipBox box;
        if (FT_Get_Color_Glyph_ClipBox(face, (FT_UInt)g, &box))
            printf(" %ld clip %ld %ld %ld %ld %ld %ld %ld %ld\n", g, box.bottom_left.x, box.bottom_left.y, box.top_left.x, box.top_left.y, box.top_right.x, box.top_right.y, box.bottom_right.x, box.bottom_right.y);
    }
    FT_Done_Face(face);
    FT_Done_FreeType(library);
    return 0;
}
