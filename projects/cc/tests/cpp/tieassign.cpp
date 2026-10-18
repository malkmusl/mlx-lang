// `std::tie(a, b) = pair` and `= tuple` of other element types (Skia's
// `std::tie(fPixmap, fInvMatrix) = access->level()`).
#include <cstdio>
#include <tuple>
#include <utility>

struct Pixmap { int w = 0; };
struct Rect { int l, t, r, b; };

static std::pair<Pixmap, float> level() { return {Pixmap{8}, 0.5f}; }
static std::tuple<const void *, Rect> prepare() { static int data; return {&data, Rect{1, 2, 3, 4}}; }

int main(int argc, char **argv) {
    Pixmap pixmap;
    float scale = 0;
    std::tie(pixmap, scale) = level();
    const void *dataPtr = nullptr;
    Rect rect{};
    std::tie(dataPtr, rect) = prepare();
    long wide = 0;
    double d = 0;
    std::tie(wide, d) = std::make_tuple(7, 2.5f);
    printf("%d %.1f %d %d %ld %.1f\n", pixmap.w, scale, dataPtr != nullptr, rect.r, wide, d);
    return argc;
}
