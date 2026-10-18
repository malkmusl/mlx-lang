// Overloads by reference binding: a modifiable lvalue takes `f(T &)` over
// `f(const T &)` (Skia's rect_points(SkRect &) beside rect_points(const
// SkRect &)), a scalar rvalue `f(const T &)` only; an unconstrained
// converting constructor weighed for a conversion is not instantiated.
#include <cstdio>
#include <cstddef>
#include <iterator>
#include <type_traits>

template <typename T> class Span {
public:
    constexpr Span() : fPtr(nullptr), fSize(0) {}
    template <typename Integer, std::enable_if_t<std::is_integral_v<Integer>, bool> = true>
    constexpr Span(T *ptr, Integer size) : fPtr(ptr), fSize((size_t)size) {}
    template <typename U, typename = std::enable_if_t<std::is_same_v<const U, T>>>
    constexpr Span(const Span<U> &that) : fPtr(std::data(that)), fSize(std::size(that)) {}
    constexpr Span(const Span &o) = default;
    template <size_t N> constexpr Span(T (&a)[N]) : Span(a, N) {}
    template <typename Container> constexpr Span(Container &&c) : Span(std::data(c), std::size(c)) {}
    constexpr T *data() const { return fPtr; }
    constexpr size_t size() const { return fSize; }
private:
    T *fPtr;
    size_t fSize;
};

struct Point { float x, y; };
struct Rect { float l, t, r, b; };

static Span<const Point> rect_points(const Rect &r) { return {reinterpret_cast<const Point *>(&r), 2}; }
static Span<Point> rect_points(Rect &r) { return {reinterpret_cast<Point *>(&r), 2}; }

struct Matrix {
    float s = 2;
    void mapPoints(Span<Point> dst, Span<const Point> src) const {
        for (size_t i = 0; i < src.size(); i++) dst.data()[i] = Point{src.data()[i].x * s, src.data()[i].y * s};
    }
    void mapPoints(Span<Point> pts) const { this->mapPoints(pts, pts); }
};

static int pick(const Rect &) { return 1; }
static int pick(Rect &) { return 2; }
static int pickInt(const int &) { return 1; }
static int pickInt(int &) { return 2; }

int main(int argc, char **argv) {
    Matrix m;
    Rect dev{}, paint{1, 2, 3, 4};
    const Rect &paintRect = paint;
    m.mapPoints(rect_points(dev), rect_points(paintRect));
    printf("%.0f %.0f %.0f %.0f\n", dev.l, dev.t, dev.r, dev.b);
    int value = 1;
    printf("%d %d %d %d\n", pick(dev), pick(paintRect), pickInt(value), pickInt(3));
    return argc;
}
