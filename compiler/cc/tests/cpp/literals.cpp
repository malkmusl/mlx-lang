// User-defined literals: integer, floating and string literal operators
// (`3_s`, `1.5_s`, `"hello"_lit` with its length), one in an inline
// namespace brought in by a using-directive.
#include <stdio.h>
#include <stddef.h>
struct Seconds { double v; };
constexpr Seconds operator""_s(long double s) { return Seconds{(double)s}; }
constexpr Seconds operator""_s(unsigned long long s) { return Seconds{(double)s}; }
constexpr Seconds operator"" _ms(unsigned long long s) { return Seconds{s / 1000.0}; }
struct Lit { const char *p; size_t n; };
constexpr Lit operator""_lit(const char *p, size_t n) { return Lit{p, n}; }
namespace ns { inline namespace literals { constexpr int operator""_twice(unsigned long long v) { return (int)v * 2; } } }
using namespace ns::literals;
int main() {
    Seconds a = 3_s, b = 1.5_s, c = 250_ms;
    Lit l = "hello"_lit;
    Lit m = "ab" "cd"_lit;
    printf("%g %g %g %s %d %s %d %d %d\n", a.v, b.v, c.v, l.p, (int)l.n, m.p, (int)m.n, 21_twice, 0x10_twice);
    return 0;
}
