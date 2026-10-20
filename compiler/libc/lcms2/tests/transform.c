/* Drives Little CMS's API as WebKit's image decoders do (an embedded
 * profile opened from memory, cmsCreate_sRGBProfile, a BGRA_8 transform
 * of relative colorimetry done in place, row by row) and the rest of
 * what mlx-lcms2 implements: every profile named transformed to the
 * built-in sRGB and grey profiles and to every other, in 8- and 16-bit
 * formats, for the four intents; the headers and tags; tone curves;
 * colorimetry. It prints statuses, values and hashes of the pixels:
 * built by gcc against the system's Little CMS, and by mlx-cc against
 * libmlxlcms2.so, it prints the same.
 *
 *     transform [-v CASE] PROFILE...      (-v: CASE's pixels as well)
 */
#include <lcms2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int verboseCase = -1;
static int caseNumber = 0;

static unsigned hash(const unsigned char* data, size_t size)
{
    unsigned h = 2166136261u;
    for (size_t i = 0; i < size; i++) {
        h ^= data[i];
        h *= 16777619u;
    }
    return h;
}

static unsigned seed = 12345;

static unsigned next(void)
{
    seed = seed * 1103515245u + 12345u;
    return (seed >> 8) & 0xffff;
}

/* The test pixels: a grid of each channel's values, greys, and random
 * ones; COUNT pixels of CHANNELS channels (EXTRA more after them). */
enum { GRID = 18, RANDOM = 1500 };

static size_t pixelCount(int channels)
{
    size_t n = 256 + RANDOM;
    if (channels == 3) n += GRID * GRID * GRID;
    if (channels == 1) n += 256;
    return n;
}

static void fill(unsigned char* buffer, int channels, int extra, int bytes)
{
    size_t n = pixelCount(channels);
    size_t at = 0;
    int width = channels + extra;
    for (size_t p = 0; p < n; p++) {
        unsigned values[8];
        if (p < 256) {
            for (int c = 0; c < width; c++)
                values[c] = bytes == 1 ? (unsigned) p : (unsigned) p * 257u;
        } else if (channels == 3 && p < 256 + GRID * GRID * GRID) {
            size_t g = p - 256;
            unsigned r = (unsigned) (g / (GRID * GRID));
            unsigned gg = (unsigned) ((g / GRID) % GRID);
            unsigned b = (unsigned) (g % GRID);
            values[0] = r * 15;
            values[1] = gg * 15;
            values[2] = b * 15;
            if (bytes == 2) {
                values[0] = values[0] * 257 + (r & 1) * 13;
                values[1] = values[1] * 257;
                values[2] = values[2] * 257 + (b & 1) * 7;
            }
            for (int c = 3; c < width; c++)
                values[c] = bytes == 1 ? 77 : 0x4d4d;
        } else if (channels == 1 && p < 512) {
            values[0] = bytes == 1 ? (unsigned) (p - 256) : (unsigned) (p - 256) * 255u + 3;
            for (int c = 1; c < width; c++)
                values[c] = 99;
        } else {
            for (int c = 0; c < width; c++) {
                unsigned v = next();
                values[c] = bytes == 1 ? (v >> 8) : v;
            }
        }
        for (int c = 0; c < width; c++) {
            if (bytes == 1) {
                buffer[at] = (unsigned char) values[c];
                at += 1;
            } else {
                unsigned short s = (unsigned short) values[c];
                memcpy(buffer + at, &s, 2);
                at += 2;
            }
        }
    }
}

static const char* intentName(int intent)
{
    if (intent == INTENT_PERCEPTUAL) return "perceptual";
    if (intent == INTENT_RELATIVE_COLORIMETRIC) return "relative";
    if (intent == INTENT_SATURATION) return "saturation";
    return "absolute";
}

/* One transform: IN's pixels of INFORMAT to OUT's of OUTFORMAT. */
static void run(const char* inName, cmsHPROFILE in, cmsUInt32Number inFormat, const char* formatName, const char* outName, cmsHPROFILE out, cmsUInt32Number outFormat, int intent, cmsUInt32Number flags, int inPlace)
{
    int number = caseNumber;
    caseNumber++;
    printf("%d %s -> %s %s %s%s%s: ", number, inName, outName, formatName, intentName(intent), flags ? " flags" : "", inPlace ? " in place" : "");
    cmsHTRANSFORM t = cmsCreateTransform(in, inFormat, out, outFormat, intent, flags);
    if (!t) {
        printf("no transform\n");
        return;
    }
    int inChannels = T_CHANNELS(inFormat);
    int inExtra = T_EXTRA(inFormat);
    int inBytes = T_BYTES(inFormat);
    int outChannels = T_CHANNELS(outFormat);
    int outExtra = T_EXTRA(outFormat);
    int outBytes = T_BYTES(outFormat);
    size_t n = pixelCount(inChannels);
    size_t inSize = n * (size_t) (inChannels + inExtra) * (size_t) inBytes;
    size_t outSize = n * (size_t) (outChannels + outExtra) * (size_t) outBytes;
    unsigned char* input = malloc(inSize);
    unsigned char* output = malloc(outSize > inSize ? outSize : inSize);
    seed = 12345;
    fill(input, inChannels, inExtra, inBytes);
    if (inPlace) {
        memcpy(output, input, inSize);
        /* Row by row, as WebKit's decoders call it. */
        for (size_t row = 0; row < n; row += 97) {
            size_t count = n - row < 97 ? n - row : 97;
            size_t offset = row * (size_t) (inChannels + inExtra) * (size_t) inBytes;
            cmsDoTransform(t, output + offset, output + offset, (cmsUInt32Number) count);
        }
    } else {
        memset(output, 0x5a, outSize);
        cmsDoTransform(t, input, output, (cmsUInt32Number) n);
    }
    printf("%08x\n", hash(output, outSize));
    if (number == verboseCase) {
        for (size_t i = 0; i < outSize; i += (size_t) outBytes) {
            unsigned v = outBytes == 1 ? output[i] : (unsigned) (output[i] | (output[i + 1] << 8));
            printf("%zu: %u\n", i / (size_t) outBytes, v);
        }
    }
    free(input);
    free(output);
    cmsDeleteTransform(t);
}

static void describe(cmsHPROFILE p)
{
    printf("  space %08x pcs %08x class %08x version %08x (%.2f) intent %u tags %d", (unsigned) cmsGetColorSpace(p), (unsigned) cmsGetPCS(p), (unsigned) cmsGetDeviceClass(p), cmsGetEncodedICCversion(p), cmsGetProfileVersion(p), cmsGetHeaderRenderingIntent(p), cmsGetTagCount(p));
    printf(" matrix-shaper %d", cmsIsMatrixShaper(p));
    for (int intent = 0; intent < 4; intent++) {
        int a = cmsIsCLUT(p, intent, LCMS_USED_AS_INPUT);
        int b = cmsIsCLUT(p, intent, LCMS_USED_AS_OUTPUT);
        int c = cmsIsIntentSupported(p, intent, LCMS_USED_AS_INPUT);
        printf(" %d%d%d", a, b, c);
    }
    printf("\n");
    cmsCIEXYZ* white = cmsReadTag(p, cmsSigMediaWhitePointTag);
    if (white)
        printf("  wtpt %a %a %a\n", white->X, white->Y, white->Z);
    cmsCIEXYZ* red = cmsReadTag(p, cmsSigRedColorantTag);
    if (red)
        printf("  rXYZ %a %a %a\n", red->X, red->Y, red->Z);
    double* chad = cmsReadTag(p, cmsSigChromaticAdaptationTag);
    if (chad)
        printf("  chad %a %a %a %a\n", chad[0], chad[1], chad[4], chad[8]);
    cmsToneCurve* trc = cmsReadTag(p, cmsGetColorSpace(p) == cmsSigGrayData ? cmsSigGrayTRCTag : cmsSigRedTRCTag);
    if (trc) {
        printf("  trc type %d entries %u linear %d monotonic %d descending %d", cmsGetToneCurveParametricType(trc), cmsGetToneCurveEstimatedTableEntries(trc), cmsIsToneCurveLinear(trc), cmsIsToneCurveMonotonic(trc), cmsIsToneCurveDescending(trc));
        printf(" at 0.5 %a 16-bit %u\n", (double) cmsEvalToneCurveFloat(trc, 0.5f), cmsEvalToneCurve16(trc, 0x8000));
    }
    if (cmsGetColorSpace(p) == cmsSigRgbData)
        printf("  gamma %.4f\n", cmsDetectRGBProfileGamma(p, 0.1));
}

static void curves(void)
{
    double params[7][10] = {
        { 2.2 },
        { 2.4, 1.0 / 1.055, 0.055 / 1.055 },
        { 2.4, 1.0 / 1.055, 0.055 / 1.055, 0.1 },
        { 2.4, 1.0 / 1.055, 0.055 / 1.055, 1.0 / 12.92, 0.04045 },
        { 2.2, 0.9, 0.1, 0.05, 0.08, 0.01, 0.02 },
        { 1.8, 1.0, 0.0, 0.0 },
        { 0.5, 1.2, 1.5, 0.1, 0.05 },
    };
    int types[7] = { 1, 2, 3, 4, 5, 6, 7 };
    for (int k = 0; k < 7; k++) {
        cmsToneCurve* c = cmsBuildParametricToneCurve(NULL, types[k], params[k]);
        cmsToneCurve* r = cmsReverseToneCurve(c);
        cmsToneCurve* r2 = cmsReverseToneCurveEx(256, c);
        printf("curve %d: type %d entries %u linear %d monotonic %d gamma %.6f\n", types[k], cmsGetToneCurveParametricType(c), cmsGetToneCurveEstimatedTableEntries(c), cmsIsToneCurveLinear(c), cmsIsToneCurveMonotonic(c), cmsEstimateGamma(c, 0.1));
        for (int i = 0; i <= 16; i++) {
            float x = (float) i / 16.0f - 0.0625f;
            unsigned short x16 = (unsigned short) (i * 4095);
            float y = cmsEvalToneCurveFloat(c, x);
            float ry = cmsEvalToneCurveFloat(r, x);
            printf("  %a %a %u %u %u\n", (double) y, (double) ry, cmsEvalToneCurve16(c, x16), cmsEvalToneCurve16(r, x16), cmsEvalToneCurve16(r2, x16));
        }
        cmsFreeToneCurve(c);
        cmsFreeToneCurve(r);
        cmsFreeToneCurve(r2);
    }
    unsigned short values[9] = { 0, 1000, 4000, 9000, 16000, 25000, 36000, 49000, 65535 };
    cmsToneCurve* t = cmsBuildTabulatedToneCurve16(NULL, 9, values);
    cmsToneCurve* tr = cmsReverseToneCurve(t);
    float floats[5] = { 0.0f, 0.1f, 0.3f, 0.6f, 1.0f };
    cmsToneCurve* f = cmsBuildTabulatedToneCurveFloat(NULL, 5, floats);
    printf("tabulated: linear %d gamma %.6f float gamma %.6f\n", cmsIsToneCurveLinear(t), cmsEstimateGamma(t, 1.0), cmsEstimateGamma(f, 1.0));
    for (int i = 0; i <= 20; i++) {
        float x = (float) i / 20.0f;
        unsigned short x16 = (unsigned short) (i * 3276);
        printf("  %u %u %a %a\n", cmsEvalToneCurve16(t, x16), cmsEvalToneCurve16(tr, x16), (double) cmsEvalToneCurveFloat(t, x), (double) cmsEvalToneCurveFloat(f, x));
    }
    cmsFreeToneCurve(t);
    cmsFreeToneCurve(tr);
    cmsFreeToneCurve(f);
}

static void colorimetry(void)
{
    const cmsCIEXYZ* d50 = cmsD50_XYZ();
    const cmsCIExyY* d50xyY = cmsD50_xyY();
    printf("D50 %a %a %a xyY %a %a %a\n", d50->X, d50->Y, d50->Z, d50xyY->x, d50xyY->y, d50xyY->Y);
    cmsCIExyY w;
    int ok = cmsWhitePointFromTemp(&w, 6504);
    printf("6504K %d %a %a\n", ok, w.x, w.y);
    cmsCIEXYZ xyz = { 0.3, 0.4, 0.2 };
    cmsCIELab lab;
    cmsXYZ2Lab(NULL, &lab, &xyz);
    cmsCIEXYZ back;
    cmsLab2XYZ(NULL, &back, &lab);
    printf("Lab %a %a %a back %a %a %a\n", lab.L, lab.a, lab.b, back.X, back.Y, back.Z);
    cmsCIExyY xyY;
    cmsXYZ2xyY(&xyY, &xyz);
    cmsxyY2XYZ(&back, &xyY);
    printf("xyY %a %a %a back %a %a %a\n", xyY.x, xyY.y, xyY.Y, back.X, back.Y, back.Z);
    cmsCIEXYZ d65 = { 0.95047, 1.0, 1.08883 };
    cmsCIEXYZ adapted;
    cmsAdaptToIlluminant(&adapted, &d65, d50, &xyz);
    printf("adapted %a %a %a\n", adapted.X, adapted.Y, adapted.Z);
}

static cmsHPROFILE openFile(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    static unsigned char data[1 << 21];
    size_t size = fread(data, 1, sizeof data, f);
    fclose(f);
    /* From memory, as WebKit's decoders open the profiles of images. */
    return cmsOpenProfileFromMem(data, (cmsUInt32Number) size);
}

static const char* baseName(const char* path)
{
    const char* slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

int main(int argc, char** argv)
{
    int first = 1;
    if (argc > 2 && strcmp(argv[1], "-v") == 0) {
        verboseCase = atoi(argv[2]);
        first = 3;
    }
    printf("version %d\n", cmsGetEncodedCMMversion());
    colorimetry();
    curves();

    int count = argc - first;
    cmsHPROFILE* profiles = calloc((size_t) count + 2, sizeof(cmsHPROFILE));
    const char** names = calloc((size_t) count + 2, sizeof(char*));
    cmsHPROFILE srgb = cmsCreate_sRGBProfile();
    cmsToneCurve* g22 = cmsBuildGamma(NULL, 2.2);
    cmsHPROFILE gray = cmsCreateGrayProfile(cmsD50_xyY(), g22);
    cmsFreeToneCurve(g22);
    printf("== sRGB built-in\n");
    describe(srgb);
    printf("== gray built-in\n");
    describe(gray);
    for (int i = 0; i < count; i++) {
        names[i] = baseName(argv[first + i]);
        profiles[i] = openFile(argv[first + i]);
        printf("== %s\n", names[i]);
        if (!profiles[i]) {
            printf("  not opened\n");
            continue;
        }
        describe(profiles[i]);
    }
    for (int i = 0; i < count; i++) {
        cmsHPROFILE in = profiles[i];
        if (!in) continue;
        cmsColorSpaceSignature space = cmsGetColorSpace(in);
        int isGray = space == cmsSigGrayData;
        if (space != cmsSigRgbData && !isGray) continue;
        cmsUInt32Number in8 = isGray ? TYPE_GRAY_8 : TYPE_BGRA_8;
        cmsUInt32Number in16 = isGray ? TYPE_GRAY_16 : TYPE_RGB_16;
        /* What WebKit does, then the rest. */
        run(names[i], in, in8, "8-bit", "sRGB", srgb, TYPE_BGRA_8, INTENT_RELATIVE_COLORIMETRIC, 0, !isGray);
        run(names[i], in, in16, "16-bit", "sRGB", srgb, TYPE_RGBA_16, INTENT_RELATIVE_COLORIMETRIC, 0, 0);
        run(names[i], in, isGray ? TYPE_GRAY_8 : TYPE_RGB_8, "8-bit", "gray", gray, TYPE_GRAY_8, INTENT_RELATIVE_COLORIMETRIC, 0, 0);
        run(names[i], in, in8, "8-bit", "sRGB", srgb, TYPE_BGRA_8, INTENT_ABSOLUTE_COLORIMETRIC, 0, 0);
        run(names[i], in, in8, "8-bit", "sRGB", srgb, TYPE_BGRA_8, INTENT_SATURATION, 0, 0);
        run(names[i], in, in8, "8-bit", "sRGB", srgb, TYPE_BGRA_8, INTENT_RELATIVE_COLORIMETRIC, cmsFLAGS_NOOPTIMIZE, 0);
        run(names[i], in, in8, "8-bit", "sRGB", srgb, TYPE_BGRA_8, INTENT_RELATIVE_COLORIMETRIC, cmsFLAGS_COPY_ALPHA | cmsFLAGS_FORCE_CLUT, 0);
        for (int j = 0; j < count; j++) {
            cmsHPROFILE out = profiles[j];
            if (!out) continue;
            cmsColorSpaceSignature outSpace = cmsGetColorSpace(out);
            if (outSpace != cmsSigRgbData && outSpace != cmsSigGrayData) continue;
            int outGray = outSpace == cmsSigGrayData;
            for (int intent = 0; intent < 4; intent++) {
                if (intent == INTENT_SATURATION && i != j) continue;
                run(names[i], in, isGray ? TYPE_GRAY_8 : TYPE_RGB_8, "8-bit", names[j], out, outGray ? TYPE_GRAY_8 : TYPE_RGB_8, intent, 0, 0);
            }
            run(names[i], in, isGray ? TYPE_GRAY_16 : TYPE_RGB_16, "16-bit", names[j], out, outGray ? TYPE_GRAY_16 : TYPE_RGB_16, INTENT_RELATIVE_COLORIMETRIC, 0, 0);
            run(names[i], in, isGray ? TYPE_GRAYA_8 : TYPE_ARGB_8, "8 to 16-bit", names[j], out, outGray ? TYPE_GRAYA_16 : TYPE_BGRA_16, INTENT_RELATIVE_COLORIMETRIC, cmsFLAGS_COPY_ALPHA, 0);
        }
    }
    for (int i = 0; i < count; i++) {
        if (profiles[i])
            cmsCloseProfile(profiles[i]);
    }
    cmsCloseProfile(srgb);
    cmsCloseProfile(gray);
    free(profiles);
    free(names);
    return 0;
}
