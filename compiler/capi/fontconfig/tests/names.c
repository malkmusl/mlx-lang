/* Patterns, values, names, constants, weights, matrices, ranges,
   FcPatternFormat, iterators and printing; character sets, language sets,
   strings and string sets. What Fontconfig 2.15 prints is names.expected. */
#include <fontconfig/fontconfig.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void show(const char *what, FcPattern *p)
{
    FcChar8 *s = FcNameUnparse(p);
    printf("%s: %s\n", what, s ? (const char *)s : "(null)");
    FcStrFree(s);
}

static void result(const char *what, FcResult r) { printf("%s -> %d\n", what, (int)r); }

static void patterns(void)
{
    FcPattern *p = FcPatternCreate();
    FcPatternAddString(p, FC_FAMILY, (const FcChar8 *)"DejaVu Sans");
    FcPatternAddString(p, FC_FAMILY, (const FcChar8 *)"Inter");
    FcPatternAddInteger(p, FC_WEIGHT, FC_WEIGHT_BOLD);
    FcPatternAddDouble(p, FC_SIZE, 12.5);
    FcPatternAddBool(p, FC_ANTIALIAS, FcTrue);
    FcValue v;
    v.type = FcTypeString;
    v.u.s = (const FcChar8 *)"Liberation Sans";
    FcPatternAdd(p, FC_FAMILY, v, FcFalse);
    v.type = FcTypeInteger;
    v.u.i = 100;
    FcPatternAddWeak(p, FC_SLANT, v, FcTrue);
    v.type = FcTypeString;
    v.u.s = (const FcChar8 *)"wrong";
    printf("add a string to an integer object: %d\n", FcPatternAdd(p, FC_WEIGHT, v, FcTrue));
    FcPatternAddString(p, "mlxcustom", (const FcChar8 *)"custom value");
    show("built", p);
    printf("objects %d\n", FcPatternObjectCount(p));

    FcChar8 *s = NULL;
    int i = 0;
    double d = 0;
    FcBool b = 0;
    result("family 0", FcPatternGetString(p, FC_FAMILY, 0, &s));
    printf("  %s\n", s);
    result("family 2", FcPatternGetString(p, FC_FAMILY, 2, &s));
    printf("  %s\n", s);
    result("family 3", FcPatternGetString(p, FC_FAMILY, 3, &s));
    result("weight as integer", FcPatternGetInteger(p, FC_WEIGHT, 0, &i));
    printf("  %d\n", i);
    result("weight as double", FcPatternGetDouble(p, FC_WEIGHT, 0, &d));
    printf("  %g\n", d);
    result("size as integer", FcPatternGetInteger(p, FC_SIZE, 0, &i));
    printf("  %d\n", i);
    result("size as string", FcPatternGetString(p, FC_SIZE, 0, &s));
    result("antialias", FcPatternGetBool(p, FC_ANTIALIAS, 0, &b));
    printf("  %d\n", b);
    result("pixelsize", FcPatternGetDouble(p, FC_PIXEL_SIZE, 0, &d));
    FcValueBinding binding;
    result("slant with binding", FcPatternGetWithBinding(p, FC_SLANT, 0, &v, &binding));
    printf("  type %d value %d binding %d\n", (int)v.type, v.u.i, (int)binding);
    result("custom", FcPatternGetString(p, "mlxcustom", 0, &s));
    printf("  %s\n", s);

    FcPattern *q = FcPatternDuplicate(p);
    printf("duplicate equal %d hash equal %d\n", FcPatternEqual(p, q), FcPatternHash(p) == FcPatternHash(q));
    FcPatternRemove(q, FC_FAMILY, 1);
    show("family 1 removed", q);
    FcPatternDel(q, FC_FAMILY);
    show("family deleted", q);
    printf("equal now %d\n", FcPatternEqual(p, q));
    FcObjectSet *os = FcObjectSetBuild(FC_WEIGHT, FC_SIZE, (char *)0);
    printf("equal on weight and size %d\n", FcPatternEqualSubset(p, q, os));
    FcPattern *f = FcPatternFilter(p, os);
    show("filtered", f);
    FcPatternDestroy(f);
    FcObjectSetDestroy(os);
    FcPatternDestroy(q);

    FcPatternIter iter;
    FcPatternIterStart(p, &iter);
    do {
        printf("iter %s: %d values\n", FcPatternIterGetObject(p, &iter), FcPatternIterValueCount(p, &iter));
    } while (FcPatternIterNext(p, &iter));
    printf("find size %d\n", FcPatternFindIter(p, &iter, FC_SIZE));
    result("  value 0", FcPatternIterGetValue(p, &iter, 0, &v, &binding));
    printf("  %g %d\n", v.u.d, (int)binding);
    printf("find dpi %d valid %d\n", FcPatternFindIter(p, &iter, FC_DPI), FcPatternIterIsValid(p, &iter));

    FcChar8 *fmt = FcPatternFormat(p, (const FcChar8 *)"%{family} | %{family[1]} | %{=unparse} | %{:weight=} | %{dpi:-none} | %-8{size}| %8{slant}|\\%");
    printf("format: %s\n", fmt ? (const char *)fmt : "(null)");
    FcStrFree(fmt);
    fmt = FcPatternFormat(p, (const FcChar8 *)"%{+family,size{%{=unparse}}} %{-family{%{=unparse}}}");
    printf("format: %s\n", fmt ? (const char *)fmt : "(null)");
    FcStrFree(fmt);

    FcPattern *built = FcPatternBuild(NULL, FC_FAMILY, FcTypeString, "Serif", FC_PIXEL_SIZE, FcTypeDouble, 13.5,
                                      FC_SPACING, FcTypeInteger, FC_MONO, FC_HINTING, FcTypeBool, FcFalse,
                                      FC_STYLE, FcTypeString, "Bold Italic", FC_DPI, FcTypeDouble, 96.0, (char *)0);
    show("FcPatternBuild", built);
    FcPatternPrint(built);
    FcPatternDestroy(built);
    FcPatternPrint(p);
    FcPatternDestroy(p);
    FcPattern *empty = FcPatternCreate();
    FcPatternPrint(empty);
    FcPatternDestroy(empty);
}

static void names(void)
{
    static const char *list[] = {
        "Sans", "Times New Roman-12", "DejaVu Sans,Inter-10.5:bold:italic", "monospace:spacing=mono:weight=medium",
        ":lang=ja", ":charset=20-7e a0", "Foo\\-Bar\\:Baz-8:style=Semi\\-Bold", ":size=[8 14]:weight=[100 200]",
        ":matrix=1 0.2 0 1:antialias=false:hinting=yes", ":pixelsize=13:dpi=96:rgba=rgb:lcdfilter=lcddefault",
        "Sans:slant=oblique:width=condensed", ":unknownobject=whatever:family=x", "-12", ":", "Sans:weight=bogus",
        NULL
    };
    for (int i = 0; list[i]; i++) {
        FcPattern *p = FcNameParse((const FcChar8 *)list[i]);
        printf("parse %s\n", list[i]);
        if (!p) {
            printf("  (null)\n");
            continue;
        }
        show("  unparse", p);
        FcPatternDestroy(p);
    }
    int value = -1;
    printf("constant bold %d %d\n", FcNameConstant((const FcChar8 *)"bold", &value), value);
    printf("constant DemiBold %d %d\n", FcNameConstant((const FcChar8 *)"DemiBold", &value), value);
    printf("constant nothing %d\n", FcNameConstant((const FcChar8 *)"nothing", &value));
    const FcConstant *c = FcNameGetConstant((const FcChar8 *)"Italic");
    printf("FcNameGetConstant italic: %s %s %d\n", c->name, c->object, c->value);
    c = FcNameGetConstantFor((const FcChar8 *)"mono", "spacing");
    printf("FcNameGetConstantFor mono spacing: %s %s %d\n", c->name, c->object, c->value);
    printf("FcNameGetConstantFor mono weight: %p\n", (void *)FcNameGetConstantFor((const FcChar8 *)"mono", "weight"));
    const FcObjectType *t = FcNameGetObjectType("pixelsize");
    printf("object type pixelsize: %s %d\n", t->object, (int)t->type);
    t = FcNameGetObjectType("charset");
    printf("object type charset: %s %d\n", t->object, (int)t->type);
    printf("weights:");
    for (int w = 100; w <= 1000; w += 50)
        printf(" %d->%d", w, FcWeightFromOpenType(w));
    printf("\nback:");
    for (int w = 0; w <= 215; w += 15)
        printf(" %d->%d", w, FcWeightToOpenType(w));
    printf("\ndouble: %g %g %g %g\n", FcWeightFromOpenTypeDouble(450), FcWeightFromOpenTypeDouble(375.5),
           FcWeightToOpenTypeDouble(90), FcWeightToOpenTypeDouble(201));
}

static void matrices_ranges_values(void)
{
    FcMatrix m;
    FcMatrixInit(&m);
    FcMatrixRotate(&m, 0.6, 0.8);
    FcMatrixScale(&m, 2, 3);
    FcMatrixShear(&m, 0.5, 0.25);
    printf("matrix %g %g %g %g\n", m.xx, m.xy, m.yx, m.yy);
    FcMatrix *copy = FcMatrixCopy(&m);
    printf("copy equal %d\n", FcMatrixEqual(&m, copy));
    FcMatrix r;
    FcMatrixMultiply(&r, &m, copy);
    printf("product %g %g %g %g\n", r.xx, r.xy, r.yx, r.yy);
    free(copy);

    FcRange *range = FcRangeCreateDouble(8.5, 14);
    FcRange *ints = FcRangeCreateInteger(100, 200);
    double a, z;
    FcRangeGetDouble(range, &a, &z);
    printf("range %g %g\n", a, z);
    FcRangeGetDouble(ints, &a, &z);
    printf("integer range %g %g\n", a, z);
    FcPattern *p = FcPatternCreate();
    FcPatternAddRange(p, FC_SIZE, range);
    FcPatternAddRange(p, FC_WEIGHT, ints);
    FcPatternAddMatrix(p, FC_MATRIX, &m);
    FcRange *got = NULL;
    FcMatrix *gm = NULL;
    result("range back", FcPatternGetRange(p, FC_SIZE, 0, &got));
    FcRangeGetDouble(got, &a, &z);
    printf("  %g %g\n", a, z);
    result("matrix back", FcPatternGetMatrix(p, FC_MATRIX, 0, &gm));
    printf("  %g %g %g %g\n", gm->xx, gm->xy, gm->yx, gm->yy);
    show("ranges", p);
    FcRange *rc = FcRangeCopy(range);
    FcRangeDestroy(range);
    FcRangeDestroy(ints);
    FcRangeDestroy(rc);

    FcValue v1, v2;
    v1.type = FcTypeDouble;
    v1.u.d = 12;
    v2.type = FcTypeInteger;
    v2.u.i = 12;
    printf("value equal 12.0 12: %d\n", FcValueEqual(v1, v2));
    v2.type = FcTypeDouble;
    v2.u.d = 12;
    printf("value equal 12.0 12.0: %d\n", FcValueEqual(v1, v2));
    v1.type = FcTypeString;
    v1.u.s = (const FcChar8 *)"abc";
    FcValue saved = FcValueSave(v1);
    printf("saved %d %s %d\n", (int)saved.type, saved.u.s, saved.u.s != v1.u.s);
    printf("saved equal %d\n", FcValueEqual(v1, saved));
    FcValuePrint(saved);
    printf("\n");
    FcValueDestroy(saved);
    FcPatternDestroy(p);
}

static void charsets(void)
{
    FcCharSet *a = FcCharSetCreate();
    FcCharSet *b = FcCharSetCreate();
    for (FcChar32 c = 0x20; c < 0x7f; c++) FcCharSetAddChar(a, c);
    for (FcChar32 c = 0x400; c < 0x460; c++) FcCharSetAddChar(a, c);
    for (FcChar32 c = 0x60; c < 0x100; c++) FcCharSetAddChar(b, c);
    FcCharSetAddChar(b, 0x1F600);
    printf("counts %u %u\n", FcCharSetCount(a), FcCharSetCount(b));
    printf("has 0x41 %d 0x400 %d 0x1f600 %d\n", FcCharSetHasChar(a, 0x41), FcCharSetHasChar(a, 0x400), FcCharSetHasChar(b, 0x1F600));
    printf("intersect %u subtract %u subset %d\n", FcCharSetIntersectCount(a, b), FcCharSetSubtractCount(a, b), FcCharSetIsSubset(a, b));
    FcCharSet *u = FcCharSetUnion(a, b);
    FcCharSet *i = FcCharSetIntersect(a, b);
    FcCharSet *s = FcCharSetSubtract(a, b);
    printf("union %u intersection %u difference %u\n", FcCharSetCount(u), FcCharSetCount(i), FcCharSetCount(s));
    printf("subset %d %d equal %d\n", FcCharSetIsSubset(i, a), FcCharSetIsSubset(a, i), FcCharSetEqual(a, a));
    FcBool changed = 7;
    printf("merge %d", FcCharSetMerge(s, b, &changed));
    printf(" changed %d count %u\n", changed, FcCharSetCount(s));
    FcCharSetDelChar(a, 0x41);
    printf("deleted 0x41: %d count %u\n", FcCharSetHasChar(a, 0x41), FcCharSetCount(a));
    FcChar32 map[FC_CHARSET_MAP_SIZE];
    FcChar32 next;
    for (FcChar32 page = FcCharSetFirstPage(u, map, &next); page != FC_CHARSET_DONE; page = FcCharSetNextPage(u, map, &next))
        printf("page %x: %08x %08x %08x %08x %08x %08x %08x %08x next %x\n", page, map[0], map[1], map[2], map[3], map[4], map[5], map[6], map[7], next);
    FcChar32 cov[8];
    FcChar32 after = FcCharSetCoverage(u, 0x404, cov);
    printf("coverage of 0x404: next %x %08x %08x\n", after, cov[0], cov[3]);
    after = FcCharSetCoverage(u, 0x500, cov);
    printf("coverage of 0x500: next %x %08x\n", after, cov[0]);
    FcPattern *p = FcPatternCreate();
    FcPatternAddCharSet(p, FC_CHARSET, b);
    show("charset", p);
    FcPatternPrint(p);
    FcCharSet *back = NULL;
    result("charset back", FcPatternGetCharSet(p, FC_CHARSET, 0, &back));
    printf("  equal %d\n", FcCharSetEqual(back, b));
    FcPatternDestroy(p);
    FcCharSet *copy = FcCharSetCopy(a);
    FcCharSetDestroy(copy);
    FcCharSetDestroy(a);
    FcCharSetDestroy(b);
    FcCharSetDestroy(u);
    FcCharSetDestroy(i);
    FcCharSetDestroy(s);
}

static void langs(void)
{
    const char *names[] = { "ja", "zh-cn", "zh_TW", "en", "de", "sr", "ar", "he", "hi", "th", "ko", "ru", "x-unknown", NULL };
    for (int i = 0; names[i]; i++) {
        const FcCharSet *cs = FcLangGetCharSet((const FcChar8 *)names[i]);
        printf("lang %s: %u\n", names[i], cs ? FcCharSetCount(cs) : 0);
    }
    const char *norm[] = { "ja_JP.UTF-8", "zh_TW", "en_US@euro", "sr@latin", "C", "POSIX", "pt-br", "x-bogus", "iu", "ku_TR", NULL };
    for (int i = 0; norm[i]; i++) {
        FcChar8 *n = FcLangNormalize((const FcChar8 *)norm[i]);
        printf("normalize %s: %s\n", norm[i], n ? (const char *)n : "(null)");
        FcStrFree(n);
    }
    FcLangSet *a = FcLangSetCreate();
    FcLangSet *b = FcLangSetCreate();
    FcLangSetAdd(a, (const FcChar8 *)"en");
    FcLangSetAdd(a, (const FcChar8 *)"ja");
    FcLangSetAdd(a, (const FcChar8 *)"x-klingon");
    FcLangSetAdd(b, (const FcChar8 *)"en-gb");
    FcLangSetAdd(b, (const FcChar8 *)"fr");
    printf("has en %d en-us %d fr %d x-klingon %d\n", FcLangSetHasLang(a, (const FcChar8 *)"en"), FcLangSetHasLang(a, (const FcChar8 *)"en-us"),
           FcLangSetHasLang(a, (const FcChar8 *)"fr"), FcLangSetHasLang(a, (const FcChar8 *)"x-klingon"));
    printf("compare %d contains %d %d equal %d\n", FcLangSetCompare(a, b), FcLangSetContains(a, b), FcLangSetContains(b, b), FcLangSetEqual(a, a));
    FcLangSet *u = FcLangSetUnion(a, b);
    FcLangSet *s = FcLangSetSubtract(u, a);
    FcStrSet *langs = FcLangSetGetLangs(u);
    FcStrList *l = FcStrListCreate(langs);
    FcChar8 *name;
    printf("union:");
    while ((name = FcStrListNext(l))) printf(" %s", name);
    printf("\n");
    FcStrListDone(l);
    FcStrSetDestroy(langs);
    FcPattern *p = FcPatternCreate();
    FcPatternAddLangSet(p, FC_LANG, s);
    show("difference", p);
    FcLangSetDel(u, (const FcChar8 *)"ja");
    FcPatternAddLangSet(p, FC_LANG, u);
    show("ja deleted", p);
    FcPatternPrint(p);
    FcPatternDestroy(p);
    printf("hash equal %d\n", FcLangSetHash(a) == FcLangSetHash(a));
    FcLangSet *c = FcLangSetCopy(a);
    printf("copy equal %d\n", FcLangSetEqual(a, c));
    FcLangSetDestroy(c);
    FcLangSetDestroy(a);
    FcLangSetDestroy(b);
    FcLangSetDestroy(u);
    FcLangSetDestroy(s);
    FcStrSet *all = FcGetLangs();
    l = FcStrListCreate(all);
    int count = 0;
    while ((name = FcStrListNext(l))) count++;
    FcStrListDone(l);
    printf("FcGetLangs: %d languages, member en %d, member xx %d\n", count, FcStrSetMember(all, (const FcChar8 *)"en"), FcStrSetMember(all, (const FcChar8 *)"xx"));
    FcStrSetDestroy(all);
}

static void strings(void)
{
    printf("cmp ignore case %d %d %d\n", FcStrCmpIgnoreCase((const FcChar8 *)"DejaVu", (const FcChar8 *)"dejavu") == 0,
           FcStrCmpIgnoreCase((const FcChar8 *)"Курсив", (const FcChar8 *)"курсив") == 0,
           FcStrCmpIgnoreCase((const FcChar8 *)"Straße", (const FcChar8 *)"STRASSE") == 0);
    printf("cmp %d %d\n", FcStrCmp((const FcChar8 *)"a", (const FcChar8 *)"b") < 0, FcStrCmp((const FcChar8 *)"abc", (const FcChar8 *)"abc"));
    const FcChar8 *hay = (const FcChar8 *)"Liberation SANS Narrow";
    const FcChar8 *found = FcStrStrIgnoreCase(hay, (const FcChar8 *)"sans");
    printf("strstr ignore case %ld\n", found ? (long)(found - hay) : -1L);
    found = FcStrStr(hay, (const FcChar8 *)"SANS");
    printf("strstr %ld\n", found ? (long)(found - hay) : -1L);
    FcChar8 *d = FcStrDowncase((const FcChar8 *)"ÀÉÎ Straße ΣΑΣ");
    printf("downcase %s\n", d);
    FcStrFree(d);
    FcChar8 *plus = FcStrPlus((const FcChar8 *)"/usr/", (const FcChar8 *)"share");
    printf("plus %s\n", plus);
    FcStrFree(plus);
    FcChar8 *copy = FcStrCopy((const FcChar8 *)"copy");
    printf("copy %s\n", copy);
    FcStrFree(copy);
    FcChar8 *dir = FcStrDirname((const FcChar8 *)"/usr/share/fonts/truetype/x.ttf");
    FcChar8 *base = FcStrBasename((const FcChar8 *)"/usr/share/fonts/truetype/x.ttf");
    printf("dirname %s basename %s\n", dir, base);
    FcStrFree(dir);
    FcStrFree(base);
    FcChar8 *built = FcStrBuildFilename((const FcChar8 *)"/usr", "share", "fonts/", (char *)0);
    printf("build %s\n", built);
    FcStrFree(built);
    FcChar8 *canon = FcStrCopyFilename((const FcChar8 *)"/usr/./share//fonts/../fonts/x");
    printf("filename %s\n", canon);
    FcStrFree(canon);
    FcChar32 ucs4;
    int n = FcUtf8ToUcs4((const FcChar8 *)"€uro", &ucs4, 6);
    printf("utf8 %d %x\n", n, ucs4);
    int nchar, wchar;
    printf("utf8len %d", FcUtf8Len((const FcChar8 *)"a€😀", 8, &nchar, &wchar));
    printf(" %d %d\n", nchar, wchar);
    FcChar8 out[FC_UTF8_MAX_LEN];
    n = FcUcs4ToUtf8(0x1F600, out);
    printf("ucs4 to utf8 %d %02x %02x %02x %02x\n", n, out[0], out[1], out[2], out[3]);
    const FcChar8 utf16[] = { 0xD8, 0x3D, 0xDE, 0x00, 0x00, 0x41 };
    n = FcUtf16ToUcs4(utf16, FcEndianBig, &ucs4, 6);
    printf("utf16 %d %x\n", n, ucs4);
    printf("utf16len %d", FcUtf16Len(utf16, FcEndianBig, 6, &nchar, &wchar));
    printf(" %d %d\n", nchar, wchar);
    FcStrSet *set = FcStrSetCreate();
    FcStrSetAdd(set, (const FcChar8 *)"one");
    FcStrSetAdd(set, (const FcChar8 *)"two");
    FcStrSetAdd(set, (const FcChar8 *)"one");
    FcStrSetAddFilename(set, (const FcChar8 *)"/a/./b/../c");
    FcStrSetDel(set, (const FcChar8 *)"two");
    FcStrList *l = FcStrListCreate(set);
    FcChar8 *s;
    printf("set:");
    while ((s = FcStrListNext(l))) printf(" %s", s);
    FcStrListFirst(l);
    printf(" again %s\n", FcStrListNext(l));
    FcStrListDone(l);
    FcStrSet *other = FcStrSetCreate();
    FcStrSetAdd(other, (const FcChar8 *)"/a/c");
    FcStrSetAdd(other, (const FcChar8 *)"one");
    printf("sets equal %d member %d\n", FcStrSetEqual(set, other), FcStrSetMember(set, (const FcChar8 *)"/a/c"));
    FcStrSetDestroy(set);
    FcStrSetDestroy(other);
    printf("version %d %d\n", FcGetVersion(), FC_VERSION);
}

int main(void)
{
    patterns();
    names();
    matrices_ranges_values();
    charsets();
    langs();
    strings();
    return 0;
}
