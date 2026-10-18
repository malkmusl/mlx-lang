// A constant expression calling a constexpr function that takes a class
// passed by its address (a copy constructor of its own) by value.
#include <stdio.h>
struct Lit {
    constexpr Lit(const char* p) : p(p) {}
    constexpr Lit(const Lit& o) : p(o.p) {}
    constexpr char at(int i) const { return p[i]; }
    const char* p;
};
constexpr Lit operator""_s(const char* s, unsigned long) { return Lit(s); }
template <class S> constexpr unsigned code(const S& s) { return (unsigned)s.at(0) << 16 | (unsigned)s.at(1); }
constexpr unsigned byValue(Lit l) { return l.at(0); }
int f(const Lit& c) { switch (code(c)) { case code("az"_s): return 1; case code("tr"_s): return 2; default: return 0; } }
static_assert(byValue(Lit("q")) == 'q', "v");
int main() { printf("%d %d %d\n", f(Lit("tr")), f(Lit("az")), f(Lit("xx"))); return 0; }
