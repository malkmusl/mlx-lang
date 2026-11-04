// Calls lowered in place by the LIR back end: accessors, constructors,
// operators, a template, a qualified virtual call, references, an
// always_inline loop; a noinline function is called. The output is the
// same without inlining (-fno-inline).
#include <stdio.h>
struct LU { int v; LU() : v(0) {} explicit LU(int x) : v(x) {} static LU fromRaw(int r) { LU u; u.v = r; return u; } int raw() const { return v; } LU operator+(const LU& o) const { return LU(v + o.v); } bool operator<(LU o) const { return v < o.v; } };
struct Pt { LU x, y; Pt(LU a, LU b) : x(a), y(b) {} LU getX() const { return x; } LU getY() const { return y; } int sum() const { return getX().raw() + getY().raw(); } };
template <typename T> static inline T maxOf(T a, T b) { return a < b ? b : a; }
struct Base { virtual int f() const { return 1; } int g() const { return f() + 1; } };
struct Der : Base { int f() const override { return 5; } };
static inline int& ref(int& r) { r += 1; return r; }
__attribute__((always_inline)) static inline int big(int n) { int t = 0; for (int i = 0; i < n; i++) { if (i % 3 == 0) t += i; else if (i % 3 == 1) t -= 1; else t *= 2; } return t; }
__attribute__((noinline)) static int keep(int a) { return a * 7; }
struct Holder { int values[4]; int at(int i) const { return values[i]; } };
int main() {
    Pt p(LU(3), LU(4));
    LU s = p.getX() + p.getY();
    Der d; Base& b = d;
    int r = 1; ref(r); ref(ref(r));
    Holder h = { { 10, 20, 30, 40 } };
    printf("%d %d %d %d %d %d %d %d %d %d\n", p.sum(), s.raw(), LU::fromRaw(9).raw(), maxOf(3, 8), maxOf(LU(2), LU(1)).raw(), b.g(), d.Base::f(), r, big(10), keep(3));
    printf("%d %d\n", h.at(2), (p.getX() < p.getY()) ? 1 : 0);
    return 0;
}
