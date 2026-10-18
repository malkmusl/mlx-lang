// A local `static constexpr` object of class type read in a static_assert
// (Skia's SkRecordCanvas: `static constexpr SkIRect kSafeBounds = {...}`).
#include <cstdio>
#include <cstdint>

struct IRect {
    int32_t fLeft, fTop, fRight, fBottom;
    constexpr int32_t width() const { return fRight - fLeft; }
};

constexpr IRect kGlobal = {1, 2, 5, 9};
static_assert(kGlobal.fBottom - kGlobal.fTop == 7);

static int32_t clamp(int32_t v) {
    static constexpr int32_t kSafeEdge = 1000 / 2 - 1;
    static constexpr IRect kSafeBounds = {-kSafeEdge, -kSafeEdge, kSafeEdge, kSafeEdge};
    static_assert((kSafeBounds.fRight - kSafeBounds.fLeft) >= 0 && (kSafeBounds.fBottom - kSafeBounds.fTop) >= 0);
    static_assert(kSafeBounds.width() == 998);
    return v > kSafeBounds.fRight ? kSafeBounds.fRight : v;
}

int main(int argc, char **argv) {
    printf("%d %d\n", clamp(5), clamp(5000));
    return argc;
}
