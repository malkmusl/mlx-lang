/* The system's configuration (fonts.conf and conf.d) and fonts: its
   configuration files, font directories, the fonts, matches and sorts of
   common queries (families, generic names, weights, languages, character
   needs), and lists. The fonts depend on the machine: tools/
   check_fontconfig.sh compares what this prints with Fontconfig 2.15's
   output for the same machine. Type 1 fonts, which this library does
   not read, are passed over in what is printed. */
#include <fontconfig/fontconfig.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int readable(FcPattern *font)
{
    FcChar8 *format;
    if (FcPatternGetString(font, FC_FONTFORMAT, 0, &format) != FcResultMatch) return 1;
    return strcmp((const char *)format, "Type 1") != 0 && strcmp((const char *)format, "PCF") != 0;
}

static void line(FcPattern *font)
{
    FcChar8 *s;
    int i = 0;
    FcPatternGetString(font, FC_FILE, 0, &s);
    printf("%s", s);
    FcPatternGetInteger(font, FC_INDEX, 0, &i);
    printf("|%d|", i);
    if (FcPatternGetString(font, FC_FAMILY, 0, &s) == FcResultMatch) printf("%s", s);
    printf("|");
    if (FcPatternGetString(font, FC_STYLE, 0, &s) == FcResultMatch) printf("%s", s);
    printf("\n");
}

static int byName(const void *a, const void *b) { return strcmp(*(const char *const *)a, *(const char *const *)b); }

static const char *queries[] = {
    "sans-serif", "serif", "monospace", "sans-serif:bold", "serif:italic", "monospace:bold:italic", "Inter",
    "Inter:weight=600", "DejaVu Sans:bold", "Liberation Serif:italic", "Arial", "Times New Roman", "Courier New",
    "Helvetica", "sans-serif:lang=ja", "sans-serif:lang=zh-cn", "sans-serif:lang=th", "sans-serif:lang=ar",
    "sans-serif:lang=he", "sans-serif:lang=ru", "serif:lang=ja", "monospace:lang=ja", "emoji", "Noto Color Emoji",
    "cursive", "fantasy", "system-ui", "sans-serif:weight=200", "sans-serif:width=75", "UnknownFamily",
    "FreeSans:oblique", "sans-serif:spacing=100", "WenQuanYi Zen Hei", "sans-serif:pixelsize=13",
    "DejaVu Sans Mono:lang=ru", ":lang=hi", ":charset=4e00", ":charset=e01", "sans-serif:color=true",
    "Sans:lang=ko", "Serif:lang=el", "Monospace:lang=uk", NULL
};

int main(void)
{
    if (!FcInit()) {
        printf("FcInit failed\n");
        return 1;
    }
    FcConfig *c = FcConfigGetCurrent();
    FcStrList *l = FcConfigGetConfigFiles(c);
    FcChar8 *s;
    while ((s = FcStrListNext(l))) printf("config file %s\n", s);
    FcStrListDone(l);
    l = FcConfigGetFontDirs(c);
    while ((s = FcStrListNext(l))) printf("font dir %s\n", s);
    FcStrListDone(l);
    FcFontSet *fonts = FcConfigGetFonts(c, FcSetSystem);
    for (int i = 0; i < fonts->nfont; i++)
        if (readable(fonts->fonts[i])) line(fonts->fonts[i]);
    for (int q = 0; queries[q]; q++) {
        printf("match %s -> ", queries[q]);
        FcPattern *p = FcNameParse((const FcChar8 *)queries[q]);
        FcConfigSubstitute(NULL, p, FcMatchPattern);
        FcDefaultSubstitute(p);
        FcResult r;
        FcPattern *font = FcFontMatch(NULL, p, &r);
        if (!font) printf("none\n"); else line(font);
        FcFontSet *sorted = FcFontSort(NULL, p, FcTrue, NULL, &r);
        printf("  sort:");
        int shown = 0;
        for (int k = 0; sorted && k < sorted->nfont && shown < 8; k++) {
            if (!readable(sorted->fonts[k])) continue;
            FcPatternGetString(sorted->fonts[k], FC_FILE, 0, &s);
            const char *base = strrchr((const char *)s, '/');
            printf(" %s", base ? base + 1 : (const char *)s);
            shown++;
        }
        printf("\n");
        FcFontSetSortDestroy(sorted);
        if (font) FcPatternDestroy(font);
        FcPatternDestroy(p);
    }
    FcPattern *all = FcPatternCreate();
    FcObjectSet *os = FcObjectSetBuild(FC_FAMILY, FC_STYLE, FC_FONTFORMAT, (char *)0);
    FcFontSet *listed = FcFontList(NULL, all, os);
    char **names = malloc(sizeof(char *) * (listed->nfont + 1));
    int n = 0;
    for (int i = 0; i < listed->nfont; i++)
        if (readable(listed->fonts[i])) names[n++] = (char *)FcNameUnparse(listed->fonts[i]);
    qsort(names, n, sizeof(char *), byName);
    for (int i = 0; i < n; i++) {
        printf("list %s\n", names[i]);
        free(names[i]);
    }
    free(names);
    FcFontSetDestroy(listed);
    FcObjectSetDestroy(os);
    FcPatternDestroy(all);
    FcFini();
    return 0;
}
