/* Writes the ICC profiles tools/check_lcms2.sh transforms with: made by
 * the system's Little CMS (built with gcc against liblcms2), so that
 * they are what real profiles are made of (version 2 and 4 headers,
 * `curv` gammas and tables, `para` curves of each type, `chad`, LUT-based
 * transforms as lut8, lut16, lutAtoB and lutBtoA with each combination
 * of their elements, XYZ and Lab PCS, grey), and damaged copies.
 *
 *     profiles DIRECTORY
 */
#include <lcms2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* directory;

static void save(cmsHPROFILE profile, const char* name)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/%s.icc", directory, name);
    if (!profile || !cmsSaveProfileToFile(profile, path)) {
        fprintf(stderr, "profiles: %s not written\n", name);
        exit(1);
    }
    cmsCloseProfile(profile);
}

static cmsHPROFILE rgb(double version, const cmsCIExyY* white, const cmsCIExyYTRIPLE* primaries, cmsToneCurve* r, cmsToneCurve* g, cmsToneCurve* b)
{
    cmsToneCurve* curves[3] = { r, g, b };
    cmsHPROFILE profile = cmsCreateRGBProfile(white, primaries, curves);
    cmsSetProfileVersion(profile, version);
    return profile;
}

static const cmsCIExyY D65 = { 0.3127, 0.3290, 1.0 };
static const cmsCIExyY D50 = { 0.3457, 0.3585, 1.0 };
static const cmsCIExyYTRIPLE Rec709 = { { 0.64, 0.33, 1.0 }, { 0.30, 0.60, 1.0 }, { 0.15, 0.06, 1.0 } };
static const cmsCIExyYTRIPLE P3 = { { 0.680, 0.320, 1.0 }, { 0.265, 0.690, 1.0 }, { 0.150, 0.060, 1.0 } };
static const cmsCIExyYTRIPLE Adobe = { { 0.64, 0.33, 1.0 }, { 0.21, 0.71, 1.0 }, { 0.15, 0.06, 1.0 } };
static const cmsCIExyYTRIPLE ProPhoto = { { 0.7347, 0.2653, 1.0 }, { 0.1596, 0.8404, 1.0 }, { 0.0366, 0.0001, 1.0 } };
static const cmsCIExyYTRIPLE Rec2020 = { { 0.708, 0.292, 1.0 }, { 0.170, 0.797, 1.0 }, { 0.131, 0.046, 1.0 } };

static cmsToneCurve* srgbCurve(void)
{
    double p[5] = { 2.4, 1.0 / 1.055, 0.055 / 1.055, 1.0 / 12.92, 0.04045 };
    return cmsBuildParametricToneCurve(NULL, 4, p);
}

/* A curve of N table entries: gamma G, with a toe. */
static cmsToneCurve* table(int n, double gamma, double toe)
{
    cmsUInt16Number* values = malloc(sizeof(cmsUInt16Number) * n);
    for (int i = 0; i < n; i++) {
        double x = (double) i / (n - 1);
        double y = x < toe ? x / 12.0 : pow((x + 0.05) / 1.05, gamma);
        if (y > 1) y = 1;
        values[i] = (cmsUInt16Number) (y * 65535.0 + 0.5);
    }
    cmsToneCurve* curve = cmsBuildTabulatedToneCurve16(NULL, n, values);
    free(values);
    return curve;
}

/* An RGB -> PCS (XYZ or Lab) transform of a matrix-shaper profile, the
 * sampler of the LUT-based profiles' grids. */
static cmsHTRANSFORM sampleTransform;

static int sampleToPCS(const cmsUInt16Number in[], cmsUInt16Number out[], void* cargo)
{
    cmsDoTransform(sampleTransform, in, out, 1);
    return 1;
}

/* A transform PCS -> RGB, the sampler of B2A grids. */
static int sampleFromPCS(const cmsUInt16Number in[], cmsUInt16Number out[], void* cargo)
{
    cmsDoTransform(sampleTransform, in, out, 1);
    return 1;
}

static cmsStage* curvesStage(int channels, double gamma)
{
    cmsToneCurve* c[8];
    for (int i = 0; i < channels; i++)
        c[i] = cmsBuildGamma(NULL, gamma + 0.1 * i);
    cmsStage* stage = cmsStageAllocToneCurves(NULL, channels, c);
    for (int i = 0; i < channels; i++)
        cmsFreeToneCurve(c[i]);
    return stage;
}

static cmsStage* tableStage(int channels, int entries, double gamma)
{
    cmsToneCurve* c[8];
    for (int i = 0; i < channels; i++)
        c[i] = table(entries, gamma, 0.0);
    cmsStage* stage = cmsStageAllocToneCurves(NULL, channels, c);
    for (int i = 0; i < channels; i++)
        cmsFreeToneCurve(c[i]);
    return stage;
}

static cmsStage* grid(int points, int inputs, int outputs, cmsSAMPLER16 sampler)
{
    cmsStage* clut = cmsStageAllocCLut16bit(NULL, points, inputs, outputs, NULL);
    cmsStageSampleCLut16bit(clut, sampler, NULL, 0);
    return clut;
}

/* A display profile of version VERSION whose A2B0 (and B2A0 when BACK)
 * are the pipelines the arguments make; PCS XYZ or Lab. */
enum { A_CURVES = 1, A_CLUT = 2, M_CURVES = 4, MATRIX = 8, B_CURVES = 16, LUT8 = 32 };

static cmsHPROFILE lutProfile(double version, cmsColorSpaceSignature pcs, int shape, int points, int back)
{
    cmsHPROFILE source = cmsCreate_sRGBProfile();
    cmsHPROFILE pcsProfile = pcs == cmsSigLabData ? cmsCreateLab4Profile(NULL) : cmsCreateXYZProfile();
    cmsUInt32Number pcsFormat = pcs == cmsSigLabData ? (version < 4 ? TYPE_LabV2_16 : TYPE_Lab_16) : TYPE_XYZ_16;
    cmsHPROFILE profile = cmsCreateProfilePlaceholder(NULL);
    cmsSetProfileVersion(profile, version);
    cmsSetDeviceClass(profile, cmsSigDisplayClass);
    cmsSetColorSpace(profile, cmsSigRgbData);
    cmsSetPCS(profile, pcs);
    cmsWriteTag(profile, cmsSigMediaWhitePointTag, (void*) cmsD50_XYZ());

    sampleTransform = cmsCreateTransform(source, TYPE_RGB_16, pcsProfile, pcsFormat, INTENT_RELATIVE_COLORIMETRIC, cmsFLAGS_NOOPTIMIZE);
    cmsPipeline* a2b = cmsPipelineAlloc(NULL, 3, 3);
    if (shape & A_CURVES) cmsPipelineInsertStage(a2b, cmsAT_END, tableStage(3, 256, 1.0));
    if (shape & A_CLUT) cmsPipelineInsertStage(a2b, cmsAT_END, grid(points, 3, 3, sampleToPCS));
    if (shape & M_CURVES) cmsPipelineInsertStage(a2b, cmsAT_END, curvesStage(3, 1.0));
    if (shape & MATRIX) {
        double m[9] = { 0.4361, 0.3851, 0.1431, 0.2225, 0.7169, 0.0606, 0.0139, 0.0971, 0.7141 };
        double o[3] = { 0, 0, 0 };
        for (int i = 0; i < 9; i++) m[i] /= 1.999969482421875;
        cmsPipelineInsertStage(a2b, cmsAT_END, cmsStageAllocMatrix(NULL, 3, 3, m, version >= 4 ? o : NULL));
    }
    if (shape & B_CURVES) cmsPipelineInsertStage(a2b, cmsAT_END, shape & LUT8 ? tableStage(3, 256, 1.1) : curvesStage(3, 1.0));
    if (shape & LUT8) cmsPipelineSetSaveAs8bitsFlag(a2b, 1);
    cmsWriteTag(profile, cmsSigAToB0Tag, a2b);
    cmsPipelineFree(a2b);
    cmsDeleteTransform(sampleTransform);

    if (back) {
        sampleTransform = cmsCreateTransform(pcsProfile, pcsFormat, source, TYPE_RGB_16, INTENT_RELATIVE_COLORIMETRIC, cmsFLAGS_NOOPTIMIZE);
        cmsPipeline* b2a = cmsPipelineAlloc(NULL, 3, 3);
        cmsPipelineInsertStage(b2a, cmsAT_END, shape & LUT8 ? tableStage(3, 256, 1.0) : curvesStage(3, 1.0));
        cmsPipelineInsertStage(b2a, cmsAT_END, grid(points, 3, 3, sampleFromPCS));
        cmsPipelineInsertStage(b2a, cmsAT_END, tableStage(3, shape & LUT8 ? 256 : 1024, 1.0));
        if (shape & LUT8) cmsPipelineSetSaveAs8bitsFlag(b2a, 1);
        cmsWriteTag(profile, cmsSigBToA0Tag, b2a);
        cmsPipelineFree(b2a);
        cmsDeleteTransform(sampleTransform);
    }
    cmsCloseProfile(source);
    cmsCloseProfile(pcsProfile);
    return profile;
}

static void damaged(const char* name, const char* from, int kind)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/%s.icc", directory, from);
    FILE* f = fopen(path, "rb");
    if (!f) exit(1);
    static unsigned char data[1 << 20];
    size_t size = fread(data, 1, sizeof data, f);
    fclose(f);
    if (kind == 0) size = size / 2;                     /* cut short */
    if (kind == 1) data[36] = 'x';                      /* no 'acsp' */
    if (kind == 2) { data[128 + 4 + 4] = 0x7f; }        /* a tag far away */
    if (kind == 3) { data[8] = 0x07; }                  /* version 7 */
    if (kind == 4) size = 100;                          /* no header */
    if (kind == 5) { data[131] = 200; }                 /* too many tags */
    if (kind == 6) { for (size_t i = 132 + 12 * 3; i < size; i += 7) data[i] ^= 0x55; } /* the data damaged */
    snprintf(path, sizeof path, "%s/%s.icc", directory, name);
    f = fopen(path, "wb");
    fwrite(data, 1, size, f);
    fclose(f);
}

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    directory = argv[1];

    save(cmsCreate_sRGBProfile(), "srgb-v4");
    cmsHPROFILE p = cmsCreate_sRGBProfile();
    cmsSetProfileVersion(p, 2.1);
    save(p, "srgb-v2");
    save(rgb(4.3, &D65, &P3, srgbCurve(), srgbCurve(), srgbCurve()), "display-p3");
    save(rgb(2.1, &D65, &Adobe, cmsBuildGamma(NULL, 2.19921875), cmsBuildGamma(NULL, 2.19921875), cmsBuildGamma(NULL, 2.19921875)), "adobe-rgb-v2");
    save(rgb(2.1, &D50, &ProPhoto, cmsBuildGamma(NULL, 1.8), cmsBuildGamma(NULL, 1.8), cmsBuildGamma(NULL, 1.8)), "prophoto-v2");
    save(rgb(4.3, &D65, &Rec2020, table(4096, 2.4, 0.018), table(4096, 2.4, 0.018), table(4096, 2.4, 0.018)), "rec2020-table4096");
    save(rgb(2.1, &D65, &Rec709, table(1024, 2.2, 0.01), table(1024, 2.3, 0.01), table(1024, 2.1, 0.01)), "camera-table1024");
    save(rgb(2.1, &D65, &Rec709, table(256, 1.8, 0.0), table(256, 1.8, 0.0), table(256, 1.8, 0.0)), "table256");
    save(rgb(2.1, &D65, &Rec709, table(26, 2.2, 0.0), table(26, 2.2, 0.0), table(26, 2.2, 0.0)), "table26");
    save(rgb(4.3, &D65, &Rec709, cmsBuildGamma(NULL, 1.0), cmsBuildGamma(NULL, 1.0), cmsBuildGamma(NULL, 1.0)), "linear-v4");
    save(rgb(2.1, &D65, &Rec709, cmsBuildGamma(NULL, 1.0), cmsBuildGamma(NULL, 1.0), cmsBuildGamma(NULL, 1.0)), "linear-v2");
    {
        double p1[1] = { 2.6 };
        double p2[3] = { 2.2, 1.1, -0.1 };
        double p3[4] = { 2.0, 1.05, -0.05, 0.02 };
        double p5[7] = { 2.4, 0.95, 0.05, 0.07, 0.05, 0.01, 0.002 };
        save(rgb(4.3, &D65, &P3, cmsBuildParametricToneCurve(NULL, 1, p1), cmsBuildParametricToneCurve(NULL, 2, p2), cmsBuildParametricToneCurve(NULL, 3, p3)), "para123");
        save(rgb(4.3, &D50, &Adobe, cmsBuildParametricToneCurve(NULL, 5, p5), srgbCurve(), cmsBuildParametricToneCurve(NULL, 2, p2)), "para54");
    }
    {
        cmsCIExyY warm;
        cmsWhitePointFromTemp(&warm, 5000);
        save(rgb(2.1, &warm, &Rec709, srgbCurve(), srgbCurve(), srgbCurve()), "white5000-v2");
        cmsWhitePointFromTemp(&warm, 9300);
        save(rgb(4.3, &warm, &Rec709, srgbCurve(), srgbCurve(), srgbCurve()), "white9300-v4");
    }
    save(cmsCreateGrayProfile(&D65, srgbCurve()), "gray-srgb");
    save(cmsCreateGrayProfile(&D50, cmsBuildGamma(NULL, 2.2)), "gray-gamma22");
    {
        cmsHPROFILE g = cmsCreateGrayProfile(&D65, table(512, 1.9, 0.0));
        cmsSetProfileVersion(g, 2.1);
        save(g, "gray-table-v2");
    }

    save(lutProfile(2.1, cmsSigXYZData, A_CURVES | A_CLUT | B_CURVES, 9, 1), "lut16-xyz");
    save(lutProfile(2.1, cmsSigLabData, A_CURVES | A_CLUT | B_CURVES, 7, 1), "lut16-lab-small");
    save(lutProfile(2.1, cmsSigLabData, A_CURVES | A_CLUT | B_CURVES, 33, 1), "lut16-lab");
    save(lutProfile(2.1, cmsSigLabData, A_CURVES | A_CLUT | B_CURVES | LUT8, 9, 1), "lut8-lab");
    save(lutProfile(2.1, cmsSigXYZData, A_CURVES | A_CLUT | B_CURVES | LUT8, 5, 0), "lut8-xyz");
    save(lutProfile(4.3, cmsSigXYZData, B_CURVES | MATRIX | M_CURVES, 0, 0), "mab-m-matrix-b");
    save(lutProfile(4.3, cmsSigLabData, A_CURVES | A_CLUT | B_CURVES, 17, 1), "mab-a-clut-b-lab");
    save(lutProfile(4.3, cmsSigXYZData, A_CURVES | A_CLUT | B_CURVES, 9, 1), "mab-a-clut-b-xyz");
    save(lutProfile(4.3, cmsSigXYZData, A_CURVES | A_CLUT | M_CURVES | MATRIX | B_CURVES, 11, 0), "mab-full");
    save(lutProfile(4.3, cmsSigLabData, A_CURVES | A_CLUT | B_CURVES, 2, 0), "mab-clut2");

    damaged("bad-cut", "display-p3", 0);
    damaged("bad-magic", "display-p3", 1);
    damaged("bad-tag", "display-p3", 2);
    damaged("bad-version", "display-p3", 3);
    damaged("bad-short", "display-p3", 4);
    damaged("bad-count", "display-p3", 5);
    damaged("bad-data", "lut16-lab-small", 6);
    damaged("bad-data-mab", "mab-full", 6);
    return 0;
}
