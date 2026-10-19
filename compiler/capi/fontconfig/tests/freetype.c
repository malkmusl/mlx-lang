/* fcfreetype.h over FreeType faces: a face's glyphs of characters, its
   character set and spacing, a pattern of a face, faces kept in patterns.
   What Fontconfig 2.15 over FreeType 2.13 prints is freetype.expected.

     freetype REPOSITORY-ROOT */
#include <fontconfig/fontconfig.h>
#include <fontconfig/fcfreetype.h>
#include <stdio.h>
#include <string.h>

static const char *fonts[] = {
    "compiler/capi/freetype/tests/fonts/names.ttf", "compiler/capi/freetype/tests/fonts/names.otf",
    "compiler/capi/freetype/tests/fonts/var-tt.ttf", "compiler/capi/freetype/tests/fonts/colr.ttf",
    "tests/support/fonts/DejaVuSans-subset.ttf", NULL
};

int main(int argc, char **argv)
{
    if (argc < 2) return 2;
    FT_Library library;
    if (FT_Init_FreeType(&library)) return 3;
    for (int f = 0; fonts[f]; f++) {
        char path[4096];
        snprintf(path, sizeof path, "%s/%s", argv[1], fonts[f]);
        FT_Face face;
        if (FT_New_Face(library, path, 0, &face)) {
            printf("%s: cannot open\n", fonts[f]);
            continue;
        }
        printf("%s:", fonts[f]);
        const FcChar32 chars[] = { 0x20, 0x41, 0x61, 0xe9, 0x416, 0x20ac, 0xf041, 0x1f600 };
        for (unsigned i = 0; i < sizeof chars / sizeof chars[0]; i++)
            printf(" %x->%u", chars[i], FcFreeTypeCharIndex(face, chars[i]));
        printf("\n");
        int spacing = -1;
        FcCharSet *cs = FcFreeTypeCharSetAndSpacing(face, NULL, &spacing);
        printf("  charset %u spacing %d\n", FcCharSetCount(cs), spacing);
        FcCharSet *plain = FcFreeTypeCharSet(face, NULL);
        printf("  same charset %d\n", FcCharSetEqual(cs, plain));
        FcPattern *p = FcFreeTypeQueryFace(face, (const FcChar8 *)path, 0, NULL);
        FcChar8 *fmt = FcPatternFormat(p, (const FcChar8 *)"%{family}|%{style}|%{weight}|%{width}|%{slant}|%{spacing:-none}|%{fontformat}|%{outline}|%{index}");
        printf("  query %s\n", fmt);
        FcStrFree(fmt);
        FcPattern *k = FcPatternCreate();
        FcPatternAddFTFace(k, FC_FT_FACE, face);
        FT_Face back = NULL;
        printf("  face kept %d %d\n", FcPatternGetFTFace(k, FC_FT_FACE, 0, &back), back == face);
        FcPatternDestroy(k);
        FcPatternDestroy(p);
        FcCharSetDestroy(cs);
        FcCharSetDestroy(plain);
        FT_Done_Face(face);
    }
    FT_Done_FreeType(library);
    return 0;
}
