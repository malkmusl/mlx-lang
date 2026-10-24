// `auto t = T(5)` and `T{ {1, 2}, 0 }` made at the object itself: the
// temporaries the discarded construction of the temporary made (an S
// converted for a `const S &`, a braced L) are not destroyed, never
// having been made; the construction at the object makes its own.
#include <cstdio>
struct S { int v; S(int x) : v(x) { std::printf("  S(%d)\n", x); } S(const S& o) : v(o.v) { std::printf("  S copy %d\n", v); } ~S() { std::printf("  ~S %d\n", v); } };
struct L { int n; L(int a, int b) : n(a + b) { std::printf("  L(%d)\n", n); } ~L() { std::printf("  ~L %d\n", n); } };
struct T {
    T(const S& s) : v(s.v) { std::printf("  T from S %d\n", v); }
    T(const L& l, int) : v(l.n) { std::printf("  T from L %d\n", v); }
    ~T() { std::printf("  ~T %d\n", v); }
    int v;
};
int main()
{
    std::printf("paren conversion\n"); { auto t = T(5); }
    std::printf("brace conversion\n"); { auto t = T{6}; }
    std::printf("brace braced arg\n"); { auto t = T{{1, 2}, 0}; }
    std::printf("paren braced arg\n"); { auto t = T({3, 4}, 0); }
    return 0;
}
