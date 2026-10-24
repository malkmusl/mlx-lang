// Forced dark mode (MlxDarkMode.h): whether a page designs for dark, the
// colour transform, and the canvas that applies it as the view paints.
#include "config.h"
#include "MlxDarkMode.h"

#include "CSSStyleSheet.h"
#include "Document.h"
#include "GenericMediaQuerySerialization.h"
#include "MediaQuery.h"
#include "StyleRule.h"
#include "StyleRuleImport.h"
#include "StyleSheetContents.h"
#include "StyleSheetList.h"
#include <cmath>
#include <skia/core/SkColorFilter.h>
#include <skia/core/SkPaint.h>
#include <skia/core/SkShader.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

// --- Whether the page designs for dark ---------------------------------

static bool queriesAskForDark(const MQ::MediaQueryList& queries)
{
    for (auto& query : queries) {
        if (!query.condition)
            continue;
        StringBuilder text;
        MQ::serialize(text, *query.condition);
        auto string = text.toString();
        if (string.contains("prefers-color-scheme"_s) && string.contains("dark"_s))
            return true;
    }
    return false;
}

static bool rulesAskForDark(const Vector<Ref<StyleRuleBase>>& rules, unsigned depth)
{
    for (auto& rule : rules) {
        if (RefPtr group = dynamicDowncast<StyleRuleGroup>(rule.get())) {
            if (RefPtr media = dynamicDowncast<StyleRuleMedia>(rule.get()); media && queriesAskForDark(media->mediaQueries()))
                return true;
            if (depth < 16 && rulesAskForDark(group->childRules(), depth + 1))
                return true;
        }
    }
    return false;
}

static bool contentsAskForDark(StyleSheetContents& contents, unsigned depth)
{
    for (auto& import : contents.importRules()) {
        if (queriesAskForDark(import->mediaQueries()))
            return true;
        if (RefPtr imported = import->styleSheet(); imported && depth < 8 && contentsAskForDark(*imported, depth + 1))
            return true;
    }
    return rulesAskForDark(contents.childRules(), 0);
}

bool mlxPageHasDarkDesign(Document& document)
{
#if ENABLE(DARK_MODE_CSS)
    if (document.resolvedColorScheme(nullptr).contains(ColorScheme::Dark))
        return true;
#endif
    Ref sheets = document.styleSheets();
    for (unsigned i = 0; i < sheets->length(); ++i) {
        RefPtr sheet = dynamicDowncast<CSSStyleSheet>(sheets->item(i).get());
        if (!sheet)
            continue;
        if (queriesAskForDark(sheet->mediaQueries()) || contentsAskForDark(sheet->contents(), 0))
            return true;
    }
    return false;
}

// --- The colour transform ----------------------------------------------

// The lightness (OKLab) a white page and black text get: std.ui.theme's
// dark background (#1a1c24) and its text (#ebeef5).
static constexpr float darkestLightness = 0.215f;
static constexpr float lightestLightness = 0.945f;

static float toLinear(float c)
{
    return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

static float toEncoded(float c)
{
    return c <= 0.0031308f ? 12.92f * c : 1.055f * std::pow(c, 1 / 2.4f) - 0.055f;
}

struct Lab {
    float l, a, b;
};

static Lab toLab(float r, float g, float b)
{
    r = toLinear(r);
    g = toLinear(g);
    b = toLinear(b);
    float l = std::cbrt(0.4122214708f * r + 0.5363325363f * g + 0.0514459929f * b);
    float m = std::cbrt(0.2119034982f * r + 0.6806995451f * g + 0.1073969566f * b);
    float s = std::cbrt(0.0883024619f * r + 0.2817188376f * g + 0.6299787005f * b);
    return {
        0.2104542553f * l + 0.7936177850f * m - 0.0040720468f * s,
        1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s,
        0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s,
    };
}

// LAB in linear sRGB (out of [0, 1] when out of gamut).
static void toLinearRGB(const Lab& lab, float rgb[3])
{
    float l = lab.l + 0.3963377774f * lab.a + 0.2158037573f * lab.b;
    float m = lab.l - 0.1055613458f * lab.a - 0.0638541728f * lab.b;
    float s = lab.l - 0.0894841775f * lab.a - 1.2914855480f * lab.b;
    l = l * l * l;
    m = m * m * m;
    s = s * s * s;
    rgb[0] = 4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
    rgb[1] = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
    rgb[2] = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;
}

static bool inGamut(const float rgb[3])
{
    for (int i = 0; i < 3; ++i) {
        if (rgb[i] < -0.0001f || rgb[i] > 1.0001f)
            return false;
    }
    return true;
}

SkColor4f mlxForcedDarkColor(const SkColor4f& color)
{
    Lab lab = toLab(std::clamp(color.fR, 0.f, 1.f), std::clamp(color.fG, 0.f, 1.f), std::clamp(color.fB, 0.f, 1.f));
    Lab dark { darkestLightness + (lightestLightness - darkestLightness) * (1 - std::clamp(lab.l, 0.f, 1.f)), lab.a, lab.b };
    float rgb[3];
    toLinearRGB(dark, rgb);
    // (Out of sRGB: less chroma, the same hue, until it fits.)
    if (!inGamut(rgb)) {
        float low = 0;
        float high = 1;
        for (int step = 0; step < 16; ++step) {
            float middle = (low + high) / 2;
            Lab trial { dark.l, dark.a * middle, dark.b * middle };
            toLinearRGB(trial, rgb);
            if (inGamut(rgb))
                low = middle;
            else
                high = middle;
        }
        toLinearRGB({ dark.l, dark.a * low, dark.b * low }, rgb);
    }
    return { toEncoded(std::clamp(rgb[0], 0.f, 1.f)), toEncoded(std::clamp(rgb[1], 0.f, 1.f)), toEncoded(std::clamp(rgb[2], 0.f, 1.f)), color.fA };
}

// The same inversion for shaders (gradients), linear in encoded sRGB: the
// luma Y of a colour goes to A - B Y, what is not luma (its chroma) is
// kept: c' = c + (A - (1 + B) Y), white to #1b1b1b and black to #ebebeb
// as mlxForcedDarkColor has them.
static sk_sp<SkColorFilter> shaderFilter()
{
    constexpr float a = 0.92f;
    constexpr float b = a - 0.106f;
    constexpr float k = 1 + b;
    constexpr float yr = 0.2126f, yg = 0.7152f, yb = 0.0722f;
    const float matrix[20] = {
        1 - k * yr, -k * yg, -k * yb, 0, a,
        -k * yr, 1 - k * yg, -k * yb, 0, a,
        -k * yr, -k * yg, 1 - k * yb, 0, a,
        0, 0, 0, 1, 0,
    };
    return SkColorFilters::Matrix(matrix);
}

// --- The canvas ----------------------------------------------------------

MlxDarkModeCanvas::MlxDarkModeCanvas(SkCanvas& target)
    : SkNWayCanvas(target.imageInfo().width(), target.imageInfo().height())
    , m_shaderFilter(shaderFilter())
{
    addCanvas(&target);
    // (The target's matrix and clip from the start, as this canvas's own.)
    setMatrix(target.getLocalToDevice());
}

MlxDarkModeCanvas::~MlxDarkModeCanvas()
{
    removeAll();
}

SkColor MlxDarkModeCanvas::darkened(SkColor color)
{
    // (Colours repeat: each worked out once.)
    uint32_t key = color + 1;
    if (key > 1 && key != std::numeric_limits<uint32_t>::max()) {
        auto found = m_colors.find(key);
        if (found != m_colors.end())
            return found->value;
    }
    SkColor result = mlxForcedDarkColor(SkColor4f::FromColor(color)).toSkColor();
    if (key > 1 && key != std::numeric_limits<uint32_t>::max() && m_colors.size() < 4096)
        m_colors.add(key, result);
    return result;
}

SkPaint MlxDarkModeCanvas::filtered(const SkPaint& paint)
{
    SkPaint result(paint);
    if (auto* shader = paint.getShader()) {
        // (An image as a shader is an image background: as it is.)
        if (!shader->isAImage())
            result.setColorFilter(m_shaderFilter->makeComposed(paint.refColorFilter()));
        return result;
    }
    result.setColor(darkened(paint.getColor()));
    return result;
}

void MlxDarkModeCanvas::onDrawPaint(const SkPaint& paint) { SkNWayCanvas::onDrawPaint(filtered(paint)); }
void MlxDarkModeCanvas::onDrawBehind(const SkPaint& paint) { SkNWayCanvas::onDrawBehind(filtered(paint)); }
void MlxDarkModeCanvas::onDrawPoints(PointMode mode, size_t count, const SkPoint points[], const SkPaint& paint) { SkNWayCanvas::onDrawPoints(mode, count, points, filtered(paint)); }
void MlxDarkModeCanvas::onDrawRect(const SkRect& rect, const SkPaint& paint) { SkNWayCanvas::onDrawRect(rect, filtered(paint)); }
void MlxDarkModeCanvas::onDrawRegion(const SkRegion& region, const SkPaint& paint) { SkNWayCanvas::onDrawRegion(region, filtered(paint)); }
void MlxDarkModeCanvas::onDrawOval(const SkRect& rect, const SkPaint& paint) { SkNWayCanvas::onDrawOval(rect, filtered(paint)); }
void MlxDarkModeCanvas::onDrawArc(const SkRect& rect, SkScalar start, SkScalar sweep, bool center, const SkPaint& paint) { SkNWayCanvas::onDrawArc(rect, start, sweep, center, filtered(paint)); }
void MlxDarkModeCanvas::onDrawRRect(const SkRRect& rect, const SkPaint& paint) { SkNWayCanvas::onDrawRRect(rect, filtered(paint)); }
void MlxDarkModeCanvas::onDrawDRRect(const SkRRect& outer, const SkRRect& inner, const SkPaint& paint) { SkNWayCanvas::onDrawDRRect(outer, inner, filtered(paint)); }
void MlxDarkModeCanvas::onDrawPath(const SkPath& path, const SkPaint& paint) { SkNWayCanvas::onDrawPath(path, filtered(paint)); }
void MlxDarkModeCanvas::onDrawGlyphRunList(const sktext::GlyphRunList& list, const SkPaint& paint) { SkNWayCanvas::onDrawGlyphRunList(list, filtered(paint)); }
void MlxDarkModeCanvas::onDrawTextBlob(const SkTextBlob* blob, SkScalar x, SkScalar y, const SkPaint& paint) { SkNWayCanvas::onDrawTextBlob(blob, x, y, filtered(paint)); }
void MlxDarkModeCanvas::onDrawVerticesObject(const SkVertices* vertices, SkBlendMode mode, const SkPaint& paint) { SkNWayCanvas::onDrawVerticesObject(vertices, mode, filtered(paint)); }

void MlxDarkModeCanvas::onDrawPatch(const SkPoint cubics[12], const SkColor colors[4], const SkPoint texCoords[4], SkBlendMode mode, const SkPaint& paint)
{
    SkColor dark[4];
    for (int i = 0; colors && i < 4; ++i)
        dark[i] = darkened(colors[i]);
    SkNWayCanvas::onDrawPatch(cubics, colors ? dark : nullptr, texCoords, mode, filtered(paint));
}

void MlxDarkModeCanvas::onDrawEdgeAAQuad(const SkRect& rect, const SkPoint clip[4], QuadAAFlags flags, const SkColor4f& color, SkBlendMode mode)
{
    SkNWayCanvas::onDrawEdgeAAQuad(rect, clip, flags, mlxForcedDarkColor(color), mode);
}

} // namespace WebCore
