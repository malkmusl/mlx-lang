// `C x(LT<float>::f())`, `C x(std::numeric_limits<float>::infinity())`:
// a member function of a class template's instance called, an object
// made, no function declared (B3's greedy register allocator).
#include <stdio.h>
#include <limits>
struct C { explicit constexpr C(float v) : m(v) { } float m; };
struct L { static constexpr float three() { return 3; } };
template<typename T> struct LT { static constexpr T four() { return 4; } };
float g1() { C x(L::three()); return x.m; }
float g2() { C x(LT<float>::four()); return x.m; }
float g3(float m) { constexpr C invalid(std::numeric_limits<float>::infinity()); const C limit = m > 0 ? C(5 / m) : invalid; return limit.m; }
int main() { printf("%g %g %g %g\n", g1(), g2(), g3(2), g3(0)); return 0; }
