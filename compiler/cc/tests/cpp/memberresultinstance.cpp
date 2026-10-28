// A class template's instance named as a member function's result is
// not made with the class: made while that class is being made, it laid
// out the class's members as nothing (Skia's skvx: Vec<2,float>'s
// `Vec<4,T> xyxy() const` made Vec<4,float> first, its two Vec<2,float>
// members both at offset 0, offsetof(float4, hi) 0, and Ganesh's
// anti-aliased paths drew nothing).
#include <cstdio>
#include <cstddef>

template <typename T> struct Pair;
template <typename T> struct Half {
    Pair<T> twice() const { return Pair<T>{*this, *this}; }
    Pair<T> declared() const;
    static Pair<T> make(T x) { return Pair<T>{Half{x, x}, Half{x + 1, x + 1}}; }
    T x, y;
};
template <typename T> struct Pair { Half<T> lo, hi; };
template <typename T> Pair<T> Half<T>::declared() const { return Pair<T>{*this, Half{y, x}}; }

template <int N, typename T> struct alignas(N * sizeof(T)) Vec;
template <int N, typename T> struct alignas(N * sizeof(T)) Vec { Vec<N / 2, T> lo, hi; };
template <typename T> struct alignas(4 * sizeof(T)) Vec<4, T> {
    Vec() = default;
    Vec(Vec<2, T> a, Vec<2, T> b) : lo(a), hi(b) {}
    T w() const { return hi.hi.val; }
    Vec<2, T> lo, hi;
};
template <typename T> struct alignas(2 * sizeof(T)) Vec<2, T> {
    Vec() = default;
    Vec(T a, T b) : lo{a}, hi{b} {}
    Vec<4, T> xyxy() const { return Vec<4, T>(*this, *this); }
    Vec<1, T> lo, hi;
};
template <typename T> struct Vec<1, T> { T val; };

using Half2 = Half<double>;
using PairD = Pair<double>;
using float2 = Vec<2, float>;
using float4 = Vec<4, float>;
using float8 = Vec<8, float>;

int main() {
    // The smaller class first: its member functions name the larger.
    std::printf("%zu %zu\n", sizeof(Half2), offsetof(PairD, hi));
    std::printf("%zu %zu %zu\n", sizeof(PairD), alignof(PairD), offsetof(PairD, lo));
    std::printf("%zu %zu\n", sizeof(float2), offsetof(float2, hi));
    std::printf("%zu %zu %zu\n", sizeof(float4), offsetof(float4, hi), sizeof(float8));
    std::printf("%zu\n", offsetof(float8, hi));
    Half2 h{1.5, 2.5};
    PairD p = h.twice();
    std::printf("%g %g %g %g\n", p.lo.x, p.lo.y, p.hi.x, p.hi.y);
    PairD q = h.declared();
    std::printf("%g %g %g %g\n", q.lo.x, q.lo.y, q.hi.x, q.hi.y);
    PairD r = Half2::make(4);
    std::printf("%g %g\n", r.lo.x, r.hi.y);
    float4 v = float2(3, 4).xyxy();
    const float *f = reinterpret_cast<const float *>(&v);
    std::printf("%g %g %g %g %g\n", f[0], f[1], f[2], f[3], v.w());
    return 0;
}
