// Forced dark mode (PORT.md, "Forced dark mode"): a page that has no dark
// design of its own, painted dark on a dark desktop, as Chrome's "Auto
// Dark Mode for Web Contents" does, by the port's painting layer: the
// view is painted through MlxDarkModeCanvas, a Skia canvas in front of the
// target that changes each draw's colours (WebKit's painting code is
// unchanged).
//
//     if (mlxPageNeedsForcedDark(document)) {
//         MlxDarkModeCanvas dark(*target);
//         GraphicsContextSkia context(dark, ...);
//         view->paint(context, rect);
//     }
#pragma once

#include <skia/core/SkColor.h>
#include <skia/core/SkRefCnt.h>
#include <skia/utils/SkNWayCanvas.h>
#include <wtf/HashMap.h>

class SkColorFilter;

namespace WebCore {

class Document;

// Whether DOCUMENT designs for dark itself: its root's or its meta
// color-scheme allows dark, or a style sheet of it (or one it imports, a
// rule nested in another) has a media query on prefers-color-scheme: dark.
bool mlxPageHasDarkDesign(Document&);

// The colour as forced dark paints it: its lightness inverted in OKLab
// (white to the desktop's dark background's lightness, black to its
// text's), its hue and chroma kept (within sRGB), its alpha kept.
SkColor4f mlxForcedDarkColor(const SkColor4f&);

// A canvas drawing into TARGET with forced dark's colours: solid colours
// (shapes, borders, text, SVG's paths) by mlxForcedDarkColor; gradients
// and other shaders through a colour matrix of the same inversion (in
// sRGB's luma); raster images (images, image backgrounds, canvases,
// video frames) as they are.
class MlxDarkModeCanvas final : public SkNWayCanvas {
public:
    explicit MlxDarkModeCanvas(SkCanvas& target);
    ~MlxDarkModeCanvas() override;

private:
    SkPaint filtered(const SkPaint&);
    SkColor darkened(SkColor);

    void onDrawPaint(const SkPaint&) override;
    void onDrawBehind(const SkPaint&) override;
    void onDrawPoints(PointMode, size_t count, const SkPoint[], const SkPaint&) override;
    void onDrawRect(const SkRect&, const SkPaint&) override;
    void onDrawRegion(const SkRegion&, const SkPaint&) override;
    void onDrawOval(const SkRect&, const SkPaint&) override;
    void onDrawArc(const SkRect&, SkScalar, SkScalar, bool, const SkPaint&) override;
    void onDrawRRect(const SkRRect&, const SkPaint&) override;
    void onDrawDRRect(const SkRRect&, const SkRRect&, const SkPaint&) override;
    void onDrawPath(const SkPath&, const SkPaint&) override;
    void onDrawGlyphRunList(const sktext::GlyphRunList&, const SkPaint&) override;
    void onDrawTextBlob(const SkTextBlob*, SkScalar x, SkScalar y, const SkPaint&) override;
    void onDrawPatch(const SkPoint cubics[12], const SkColor colors[4], const SkPoint texCoords[4], SkBlendMode, const SkPaint&) override;
    void onDrawVerticesObject(const SkVertices*, SkBlendMode, const SkPaint&) override;
    void onDrawEdgeAAQuad(const SkRect&, const SkPoint[4], QuadAAFlags, const SkColor4f&, SkBlendMode) override;

    sk_sp<SkColorFilter> m_shaderFilter;
    HashMap<uint32_t, uint32_t> m_colors;
};

} // namespace WebCore
