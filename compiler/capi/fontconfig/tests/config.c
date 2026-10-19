/* A configuration read from memory (two font directories of the tree,
   aliases, rules of the three targets, a rejected file) and its fonts:
   listed, matched, sorted (with the coverage), prepared for rendering,
   listed by object sets; application fonts; files and directories scanned;
   FcFreeTypeQuery. What Fontconfig 2.15 prints is config.expected.

     config REPOSITORY-ROOT */
#include <fontconfig/fontconfig.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *base(const FcChar8 *f)
{
    const char *b = strrchr((const char *)f, '/');
    return b ? b + 1 : (const char *)f;
}

static void line(FcPattern *font)
{
    FcChar8 *s;
    int i = 0;
    double d = 0;
    FcBool b = 0;
    if (FcPatternGetString(font, FC_FILE, 0, &s) == FcResultMatch) printf("%s", base(s));
    FcPatternGetInteger(font, FC_INDEX, 0, &i);
    printf("|%d|", i);
    if (FcPatternGetString(font, FC_FAMILY, 0, &s) == FcResultMatch) printf("%s", s);
    printf("|");
    if (FcPatternGetString(font, FC_STYLE, 0, &s) == FcResultMatch) printf("%s", s);
    if (FcPatternGetDouble(font, FC_WEIGHT, 0, &d) == FcResultMatch) printf("|%g", d); else printf("|-");
    if (FcPatternGetDouble(font, FC_WIDTH, 0, &d) == FcResultMatch) printf("|%g", d); else printf("|-");
    if (FcPatternGetBool(font, FC_EMBOLDEN, 0, &b) == FcResultMatch) printf("|%d", b); else printf("|-");
    printf("\n");
}

static void sets(const char *what, FcFontSet *fs)
{
    printf("%s %d\n", what, fs ? fs->nfont : -1);
    for (int i = 0; fs && i < fs->nfont; i++) line(fs->fonts[i]);
}

static const char *queries[] = {
    "sans-serif", "monospace", "Testy", "MlxVarTest:weight=200", "MlxVarTest:bold", "MlxVarTest:width=75",
    "MlxCff2Test:weight=210", "MlxColrTest", "MlxColrOneTest", "MlxSbixTest", ":lang=en", "serif",
    "MlxVarTest:variable=true", "MlxNamesTest:fontformat=CFF", "MlxVarTest:slant=italic", "DejaVu Sans:charset=41-5a",
    ":charset=20-7e 400-45f", "MlxNamesTest:weight=bold:size=48", NULL
};

static void queries_of(FcConfig *c)
{
    for (int q = 0; queries[q]; q++) {
        printf("match %s -> ", queries[q]);
        FcPattern *p = FcNameParse((const FcChar8 *)queries[q]);
        FcConfigSubstitute(c, p, FcMatchPattern);
        FcDefaultSubstitute(p);
        FcResult r;
        FcPattern *font = FcFontMatch(c, p, &r);
        if (!font) printf("none (%d)\n", (int)r); else line(font);
        FcCharSet *cs = NULL;
        FcFontSet *sorted = FcFontSort(c, p, FcTrue, &cs, &r);
        printf("sort (%d) covering %u:", (int)r, cs ? FcCharSetCount(cs) : 0);
        for (int k = 0; sorted && k < sorted->nfont; k++) {
            FcChar8 *f;
            int i = 0;
            FcPatternGetString(sorted->fonts[k], FC_FILE, 0, &f);
            FcPatternGetInteger(sorted->fonts[k], FC_INDEX, 0, &i);
            printf(" %s/%d", base(f), i);
        }
        printf("\n");
        FcFontSet *untrimmed = FcFontSort(c, p, FcFalse, NULL, &r);
        printf("untrimmed %d\n", untrimmed ? untrimmed->nfont : -1);
        FcFontSetSortDestroy(untrimmed);
        if (sorted && sorted->nfont > 0) {
            FcPattern *prepared = FcFontRenderPrepare(c, p, sorted->fonts[0]);
            FcChar8 *fmt = FcPatternFormat(prepared, (const FcChar8 *)"%{family[0]}/%{style[0]:-none}/%{weight}/%{slant}/%{size}/%{pixelsize}/%{embolden:-none}");
            printf("prepared %s\n", fmt);
            FcStrFree(fmt);
            FcPatternDestroy(prepared);
        }
        if (cs) FcCharSetDestroy(cs);
        FcFontSetSortDestroy(sorted);
        if (font) FcPatternDestroy(font);
        FcPatternDestroy(p);
    }
}

static void lists(FcConfig *c, const char *query, FcObjectSet *os)
{
    FcPattern *p = FcNameParse((const FcChar8 *)query);
    FcFontSet *listed = FcFontList(c, p, os);
    printf("list %s:", query);
    for (int i = 0; i < listed->nfont; i++) {
        FcChar8 *s = FcNameUnparse(listed->fonts[i]);
        printf(" %s", s);
        FcStrFree(s);
    }
    printf("\n");
    FcFontSetDestroy(listed);
    FcPatternDestroy(p);
}

int main(int argc, char **argv)
{
    if (argc < 2) return 2;
    const char *root = argv[1];
    char cwd[4096];
    if (!getcwd(cwd, sizeof cwd)) return 2;
    char cache[4200];
    snprintf(cache, sizeof cache, "%s/xdg-cache", cwd);
    setenv("XDG_CACHE_HOME", cache, 1);
    setenv("FC_LANG", "en", 1);
    char conf[16384];
    snprintf(conf, sizeof conf,
             "<fontconfig>\n"
             "  <dir>%s/compiler/capi/freetype/tests/fonts</dir>\n"
             "  <dir>%s/tests/support/fonts</dir>\n"
             "  <cachedir>%s/fc-cache</cachedir>\n"
             "  <alias><family>sans-serif</family><prefer><family>DejaVu Sans</family></prefer></alias>\n"
             "  <alias><family>monospace</family><prefer><family>MlxNamesTest</family></prefer></alias>\n"
             "  <match target=\"pattern\"><test name=\"family\"><string>Testy</string></test>"
             "<edit name=\"family\" mode=\"assign\" binding=\"strong\"><string>MlxVarTest</string></edit></match>\n"
             "  <match target=\"font\"><test name=\"family\"><string>MlxColrTest</string></test>"
             "<edit name=\"embolden\" mode=\"assign\"><bool>true</bool></edit></match>\n"
             "  <match target=\"font\"><test name=\"weight\" compare=\"more\"><const>medium</const></test>"
             "<test target=\"pattern\" name=\"weight\" compare=\"less\"><const>medium</const></test>"
             "<edit name=\"embolden\" mode=\"assign\"><bool>false</bool></edit></match>\n"
             "  <match target=\"scan\"><test name=\"family\"><string>MlxSbixTest</string></test>"
             "<edit name=\"weight\" mode=\"assign\"><const>bold</const></edit></match>\n"
             "  <match><test name=\"size\" compare=\"more_eq\"><double>40</double></test>"
             "<edit name=\"family\" mode=\"prepend\"><string>MlxVarTest</string></edit>"
             "<edit name=\"pixelsize\" mode=\"assign\"><times><name>size</name><double>2</double></times></edit></match>\n"
             "  <selectfont><rejectfont><glob>*/colr1.ttf</glob></rejectfont></selectfont>\n"
             "</fontconfig>\n",
             root, root, cwd);
    FcConfig *c = FcConfigCreate();
    printf("parse %d\n", FcConfigParseAndLoadFromMemory(c, (const FcChar8 *)conf, FcTrue));
    printf("build %d\n", FcConfigBuildFonts(c));
    FcStrList *dirs = FcConfigGetFontDirs(c);
    FcChar8 *dir;
    while ((dir = FcStrListNext(dirs))) printf("font dir %s\n", strncmp((const char *)dir, root, strlen(root)) == 0 ? (const char *)dir + strlen(root) : (const char *)dir);
    FcStrListDone(dirs);
    sets("fonts", FcConfigGetFonts(c, FcSetSystem));
    sets("application fonts", FcConfigGetFonts(c, FcSetApplication));
    queries_of(c);

    FcObjectSet *os = FcObjectSetBuild(FC_FAMILY, FC_STYLE, FC_WEIGHT, (char *)0);
    lists(c, ":", os);
    lists(c, "MlxVarTest", os);
    lists(c, ":weight=200", os);
    lists(c, ":lang=en", os);
    FcObjectSetDestroy(os);
    os = FcObjectSetBuild(FC_FAMILY, FC_WEIGHT, FC_WIDTH, FC_VARIABLE, FC_INDEX, FC_NAMED_INSTANCE, (char *)0);
    lists(c, "MlxVarTest", os);
    FcObjectSetDestroy(os);
    os = FcObjectSetBuild(FC_FAMILY, FC_FONTFORMAT, FC_COLOR, FC_SPACING, FC_CAPABILITY, (char *)0);
    lists(c, ":", os);
    FcObjectSetDestroy(os);

    /* A font pattern edited by the configuration's font rules. */
    FcPattern *pat = FcNameParse((const FcChar8 *)"MlxColrTest:weight=light");
    FcPattern *font = FcNameParse((const FcChar8 *)"MlxColrTest:weight=bold");
    FcConfigSubstituteWithPat(c, font, pat, FcMatchFont);
    FcChar8 *s = FcNameUnparse(font);
    printf("font rules: %s\n", s);
    FcStrFree(s);
    FcPatternDestroy(pat);
    FcPatternDestroy(font);

    /* FcFontSetMatch and FcFontSetList over the configuration's set. */
    FcFontSet *system = FcConfigGetFonts(c, FcSetSystem);
    FcPattern *p = FcNameParse((const FcChar8 *)"MlxCff2Test:weight=bold");
    FcConfigSubstitute(c, p, FcMatchPattern);
    FcDefaultSubstitute(p);
    FcResult r;
    FcPattern *best = FcFontSetMatch(c, &system, 1, p, &r);
    printf("set match: ");
    line(best);
    FcPatternDestroy(best);
    FcFontSet *sorted = FcFontSetSort(c, &system, 1, p, FcTrue, NULL, &r);
    printf("set sort %d first ", sorted->nfont);
    line(sorted->fonts[0]);
    FcFontSetSortDestroy(sorted);
    FcPatternDestroy(p);

    /* Application fonts. */
    char file[4300];
    snprintf(file, sizeof file, "%s/compiler/capi/freetype/tests/fonts/colr1.ttf", root);
    printf("app font file %d\n", FcConfigAppFontAddFile(c, (const FcChar8 *)file));
    snprintf(file, sizeof file, "%s/tests/support/fonts", root);
    printf("app font dir %d\n", FcConfigAppFontAddDir(c, (const FcChar8 *)file));
    sets("application fonts", FcConfigGetFonts(c, FcSetApplication));
    p = FcNameParse((const FcChar8 *)"MlxColrOneTest");
    FcConfigSubstitute(c, p, FcMatchPattern);
    FcDefaultSubstitute(p);
    best = FcFontMatch(c, p, &r);
    printf("with application fonts: ");
    line(best);
    FcPatternDestroy(best);
    FcPatternDestroy(p);
    FcConfigAppFontClear(c);
    sets("cleared", FcConfigGetFonts(c, FcSetApplication));

    /* Scanning. */
    FcFontSet *scanned = FcFontSetCreate();
    FcStrSet *subdirs = FcStrSetCreate();
    snprintf(file, sizeof file, "%s/compiler/capi/freetype/tests/fonts/var-tt.ttf", root);
    int count = -1;
    FcPattern *q = FcFreeTypeQuery((const FcChar8 *)file, 0x30000, NULL, &count);
    printf("query 0x30000 of %d: ", count);
    line(q);
    FcPatternDestroy(q);
    printf("query all %u\n", FcFreeTypeQueryAll((const FcChar8 *)file, (unsigned)-1, NULL, &count, scanned));
    sets("queried", scanned);
    FcFontSetDestroy(scanned);
    FcStrSetDestroy(subdirs);
    printf("is dir %d %d\n", FcFileIsDir((const FcChar8 *)root), FcFileIsDir((const FcChar8 *)file));
    FcConfigDestroy(c);
    return 0;
}
