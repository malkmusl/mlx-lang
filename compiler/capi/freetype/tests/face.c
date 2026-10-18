/* The faces of the fonts named on the command line, as FreeType opens
   them: flags, names, sizes, charmaps, global metrics, the SFNT tables
   and names, glyph names, the PostScript name, `gasp`, kerning, the
   characters of the Unicode charmap and its variation selectors.

   The face flags are printed without FT_FACE_FLAG_HINTER, which FreeType
   sets for TrueType faces when it has its bytecode interpreter. */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H
#include FT_SFNT_NAMES_H
#include FT_GASP_H
#include FT_FONT_FORMATS_H
#include FT_TYPE1_TABLES_H
#include FT_CID_H
#include <stdio.h>
#include <string.h>

static unsigned long hash_bytes(const unsigned char *p, unsigned long n, unsigned long h)
{
    while (n--) h = h * 31 + *p++;
    return h;
}

static void print_face(FT_Library library, const char *path, long index)
{
    FT_Face face;
    FT_Error error = FT_New_Face(library, path, index, &face);
    printf("face %s %ld: error %d\n", strrchr(path, '/') + 1, index, error);
    if (error) return;
    printf("  faces %ld index 0x%lx flags 0x%lx style 0x%lx glyphs %ld\n", face->num_faces, face->face_index, face->face_flags & ~FT_FACE_FLAG_HINTER, face->style_flags, face->num_glyphs);
    printf("  family '%s' style '%s'\n", face->family_name ? face->family_name : "(null)", face->style_name ? face->style_name : "(null)");
    printf("  bbox %ld %ld %ld %ld em %u asc %d desc %d height %d maxadv %d %d underline %d %d\n", face->bbox.xMin, face->bbox.yMin, face->bbox.xMax, face->bbox.yMax, face->units_per_EM, face->ascender, face->descender, face->height, face->max_advance_width, face->max_advance_height, face->underline_position, face->underline_thickness);
    printf("  format %s ps '%s' fstype 0x%x ps-names %d\n", FT_Get_Font_Format(face), FT_Get_Postscript_Name(face) ? FT_Get_Postscript_Name(face) : "(null)", FT_Get_FSType_Flags(face), FT_Has_PS_Glyph_Names(face));
    for (int i = 0; i < face->num_fixed_sizes; i++) {
        FT_Bitmap_Size *s = &face->available_sizes[i];
        printf("  strike %d: %d x %d size %ld ppem %ld %ld\n", i, s->width, s->height, s->size, s->x_ppem, s->y_ppem);
    }
    for (int i = 0; i < face->num_charmaps; i++) {
        FT_CharMap c = face->charmaps[i];
        printf("  charmap %d: platform %d encoding %d tag 0x%x format %ld language %lu index %d%s\n", i, c->platform_id, c->encoding_id, c->encoding, FT_Get_CMap_Format(c), FT_Get_CMap_Language_ID(c), FT_Get_Charmap_Index(c), c == face->charmap ? " (selected)" : "");
    }
    TT_Header *head = FT_Get_Sfnt_Table(face, FT_SFNT_HEAD);
    if (head) printf("  head version 0x%lx revision 0x%lx flags 0x%x em %u created %lx %lx box %d %d %d %d mac %u lowest %u loc %d\n", head->Table_Version, head->Font_Revision, head->Flags, head->Units_Per_EM, head->Created[0], head->Created[1], head->xMin, head->yMin, head->xMax, head->yMax, head->Mac_Style, head->Lowest_Rec_PPEM, head->Index_To_Loc_Format);
    TT_HoriHeader *hhea = FT_Get_Sfnt_Table(face, FT_SFNT_HHEA);
    if (hhea) printf("  hhea asc %d desc %d gap %d adv %u minl %d minr %d ext %d metrics %u\n", hhea->Ascender, hhea->Descender, hhea->Line_Gap, hhea->advance_Width_Max, hhea->min_Left_Side_Bearing, hhea->min_Right_Side_Bearing, hhea->xMax_Extent, hhea->number_Of_HMetrics);
    TT_VertHeader *vhea = FT_Get_Sfnt_Table(face, FT_SFNT_VHEA);
    if (vhea) printf("  vhea asc %d desc %d adv %u metrics %u\n", vhea->Ascender, vhea->Descender, vhea->advance_Height_Max, vhea->number_Of_VMetrics);
    TT_OS2 *os2 = FT_Get_Sfnt_Table(face, FT_SFNT_OS2);
    if (os2) printf("  os2 version %u avg %d weight %u width %u type 0x%x sel 0x%x typo %d %d %d win %u %u ranges %lx %lx %lx %lx cp %lx %lx x %d cap %d first %u last %u vendor %.4s\n", os2->version, os2->xAvgCharWidth, os2->usWeightClass, os2->usWidthClass, os2->fsType, os2->fsSelection, os2->sTypoAscender, os2->sTypoDescender, os2->sTypoLineGap, os2->usWinAscent, os2->usWinDescent, os2->ulUnicodeRange1, os2->ulUnicodeRange2, os2->ulUnicodeRange3, os2->ulUnicodeRange4, os2->ulCodePageRange1, os2->ulCodePageRange2, os2->sxHeight, os2->sCapHeight, os2->usFirstCharIndex, os2->usLastCharIndex, (const char *)os2->achVendID);
    TT_Postscript *post = FT_Get_Sfnt_Table(face, FT_SFNT_POST);
    if (post) printf("  post format 0x%lx angle 0x%lx underline %d %d fixed %lu\n", post->FormatType, post->italicAngle, post->underlinePosition, post->underlineThickness, post->isFixedPitch);
    TT_MaxProfile *maxp = FT_Get_Sfnt_Table(face, FT_SFNT_MAXP);
    if (maxp) printf("  maxp version 0x%lx glyphs %u points %u contours %u zones %u fdefs %u depth %u\n", maxp->version, maxp->numGlyphs, maxp->maxPoints, maxp->maxContours, maxp->maxZones, maxp->maxFunctionDefs, maxp->maxComponentDepth);
    TT_PCLT *pclt = FT_Get_Sfnt_Table(face, FT_SFNT_PCLT);
    if (pclt) printf("  pclt version 0x%lx font %lu pitch %u\n", pclt->Version, pclt->FontNumber, pclt->Pitch);

    FT_ULong count = 0;
    FT_Sfnt_Table_Info(face, 0, NULL, &count);
    printf("  tables %lu:", count);
    for (FT_UInt i = 0; i < count; i++) {
        FT_ULong tag, length;
        if (!FT_Sfnt_Table_Info(face, i, &tag, &length)) printf(" %c%c%c%c=%lu", (char)(tag >> 24), (char)(tag >> 16), (char)(tag >> 8), (char)tag, length);
    }
    printf("\n");
    {
        FT_ULong length = 0;
        FT_Error e = FT_Load_Sfnt_Table(face, FT_MAKE_TAG('h', 'e', 'a', 'd'), 0, NULL, &length);
        unsigned char buffer[64];
        FT_ULong four = 4;
        FT_Error e2 = FT_Load_Sfnt_Table(face, FT_MAKE_TAG('h', 'e', 'a', 'd'), 12, buffer, &four);
        FT_Error e3 = FT_Load_Sfnt_Table(face, FT_MAKE_TAG('x', 'y', 'z', 'w'), 0, NULL, &length);
        printf("  load head %d %lu %d %02x%02x%02x%02x missing %d\n", e, length, e2, buffer[0], buffer[1], buffer[2], buffer[3], e3);
    }
    FT_UInt names = FT_Get_Sfnt_Name_Count(face);
    printf("  names %u\n", names);
    for (FT_UInt i = 0; i < names; i++) {
        FT_SfntName name;
        if (FT_Get_Sfnt_Name(face, i, &name)) continue;
        printf("   %u: %u %u 0x%x %u len %u hash %lx\n", i, name.platform_id, name.encoding_id, name.language_id, name.name_id, name.string_len, hash_bytes(name.string, name.string_len, 0));
    }
    {
        FT_SfntLangTag tag;
        printf("  langtag %d\n", FT_Get_Sfnt_LangTag(face, 0x8000, &tag));
    }
    printf("  gasp");
    for (int ppem = 0; ppem < 100; ppem += 7) printf(" %d", FT_Get_Gasp(face, ppem));
    printf("\n");
    {
        char name[64];
        unsigned long h = 0;
        int errors = 0;
        for (long g = 0; g < face->num_glyphs + 2; g++) {
            FT_Error e = FT_Get_Glyph_Name(face, g, name, sizeof name);
            if (e) errors++;
            else h = hash_bytes((unsigned char *)name, strlen(name) + 1, h);
            if (g < 8 || g == face->num_glyphs - 1) printf("  glyph %ld name %d '%s'\n", g, e, e ? "" : name);
        }
        char tiny[4];
        FT_Error e = FT_Get_Glyph_Name(face, face->num_glyphs > 3 ? 3 : 0, tiny, sizeof tiny);
        printf("  names hash %lx errors %d tiny %d '%s' index-of-A %u index-of-space %u\n", h, errors, e, tiny, FT_Get_Name_Index(face, "A"), FT_Get_Name_Index(face, "space"));
    }
    {
        FT_Vector k;
        FT_UInt a = FT_Get_Char_Index(face, 'A'), v = FT_Get_Char_Index(face, 'V'), t = FT_Get_Char_Index(face, 'T'), o = FT_Get_Char_Index(face, 'o');
        FT_Error e = FT_Get_Kerning(face, a, v, FT_KERNING_UNSCALED, &k);
        printf("  kern AV %d %ld %ld", e, k.x, k.y);
        e = FT_Get_Kerning(face, t, o, FT_KERNING_UNSCALED, &k);
        printf(" To %d %ld %ld\n", e, k.x, k.y);
    }
    for (int i = 0; i < face->num_charmaps; i++) {
        FT_CharMap c = face->charmaps[i];
        if (FT_Get_CMap_Format(c) == 14) continue;
        FT_Set_Charmap(face, c);
        FT_UInt gindex;
        unsigned long h = 0, n = 0;
        FT_ULong code = FT_Get_First_Char(face, &gindex);
        FT_ULong first = code;
        while (gindex != 0) {
            h = h * 31 + code;
            h = h * 31 + gindex;
            n++;
            code = FT_Get_Next_Char(face, code, &gindex);
        }
        unsigned long h2 = 0;
        for (FT_ULong k = 0; k < 0x30000; k += (k < 0x3000 ? 1 : 7)) h2 = h2 * 31 + FT_Get_Char_Index(face, k);
        printf("  chars of charmap %d: first 0x%lx count %lu hash %lx lookups %lx\n", i, first, n, h, h2);
    }
    printf("  select unicode %d", FT_Select_Charmap(face, FT_ENCODING_UNICODE));
    printf(" symbol %d", FT_Select_Charmap(face, FT_ENCODING_MS_SYMBOL));
    printf(" none %d\n", FT_Select_Charmap(face, FT_ENCODING_NONE));
    FT_Select_Charmap(face, FT_ENCODING_UNICODE);
    {
        FT_UInt32 *selectors = FT_Face_GetVariantSelectors(face);
        printf("  selectors:");
        for (int i = 0; selectors && selectors[i]; i++) {
            printf(" %x", selectors[i]);
            FT_UInt32 *chars = FT_Face_GetCharsOfVariant(face, selectors[i]);
            unsigned long h = 0, n = 0;
            for (int k = 0; chars && chars[k]; k++) {
                h = h * 31 + chars[k];
                h = h * 31 + FT_Face_GetCharVariantIndex(face, chars[k], selectors[i]);
                h = h * 31 + (unsigned long)(FT_Face_GetCharVariantIsDefault(face, chars[k], selectors[i]) + 1);
                n++;
            }
            printf("(%lu %lx)", n, h);
        }
        printf("\n");
    }
    {
        FT_Bool cid = 9;
        const char *registry = NULL, *ordering = NULL;
        FT_Int supplement = -1;
        FT_UInt id = 77;
        FT_Error e1 = FT_Get_CID_Is_Internally_CID_Keyed(face, &cid);
        FT_Error e2 = FT_Get_CID_Registry_Ordering_Supplement(face, &registry, &ordering, &supplement);
        FT_Error e3 = FT_Get_CID_From_Glyph_Index(face, 1, &id);
        PS_FontInfoRec info;
        FT_Error e4 = FT_Get_PS_Font_Info(face, &info);
        printf("  cid %d %d ros %d %s %s %d from %d %u psinfo %d", e1, cid, e2, registry ? registry : "-", ordering ? ordering : "-", supplement, e3, id, e4);
        if (!e4) printf(" '%s' '%s' '%s' '%s' '%s' %ld %d %d %u", info.version ? info.version : "-", info.notice ? info.notice : "-", info.full_name ? info.full_name : "-", info.family_name ? info.family_name : "-", info.weight ? info.weight : "-", info.italic_angle, info.is_fixed_pitch, info.underline_position, info.underline_thickness);
        printf("\n");
    }
    FT_Done_Face(face);
}

int main(int argc, char **argv)
{
    FT_Library library;
    FT_Error error = FT_Init_FreeType(&library);
    printf("init %d\n", error);
    for (int i = 1; i < argc; i++) {
        FT_Face face;
        error = FT_New_Face(library, argv[i], -1, &face);
        printf("%s: faces error %d\n", strrchr(argv[i], '/') + 1, error);
        if (error) continue;
        long faces = face->num_faces;
        long instances = face->style_flags >> 16;
        printf("  num_faces %ld instances %ld index %ld\n", faces, instances, face->face_index);
        FT_Done_Face(face);
        for (long f = 0; f < faces; f++) {
            print_face(library, argv[i], f);
            for (long n = 1; n <= instances; n++) print_face(library, argv[i], f | (n << 16));
        }
    }
    printf("missing %d\n", FT_New_Face(library, "/nonexistent/font.ttf", 0, &(FT_Face){0}));
    FT_Done_FreeType(library);
    return 0;
}
