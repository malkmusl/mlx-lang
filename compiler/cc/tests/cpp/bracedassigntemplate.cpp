// `s = { a, b }` with a constructor template taking the two values
// exactly: the template constructs, not LS(const IS &) through IS's
// (int, int) (LayoutRect::shiftEdgesTo's `m_location = { left, top }`
// truncated LayoutUnits to an IntPoint).
#include <cstdio>
struct IS { int w, h; IS(int a, int b) : w(a), h(b) {} };
struct LU {
    int m;
    constexpr LU() : m(0) {}
    constexpr LU(int v) : m(v * 64) {}
    static LU raw(int v) { LU u; u.m = v; return u; }
    constexpr operator int() const { return m / 64; }
    constexpr operator float() const { return m / 64.0f; }
};
struct LS {
    LS() = default;
    LS(const IS& s) : w(s.w), h(s.h) {}
    template<typename T, typename U> constexpr LS(T width, U height) : w(width), h(height) {}
    LU w, h;
};
int main(int argc, char **)
{
    LU a = LU::raw(4360), b = LU::raw(1088);
    LS s1 { a, b };
    LS s2; s2 = { a, b };
    LS s3; s3 = LS { a, b };
    std::printf("%d %d %d %d\n", s1.w.m, s2.w.m, s3.w.m, s2.h.m);
    return argc == 2 ? 0 : 1;
}
