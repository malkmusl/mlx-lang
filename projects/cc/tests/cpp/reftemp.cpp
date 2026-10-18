// A temporary bound to a local reference lives as long as the reference
// ([class.temporary]): `const S &r = make(2)`, `auto &&r = make(5)`, and
// the result of `c ? object : temporary` (JSC's linkedSourceCode:
// `const SourceCode &parent = c ? passed : defaultSource()`).
#include <cstdio>
struct S { int v; S(int x) : v(x) { } S(const S& o) : v(o.v + 10) { } ~S() { printf("~S %d\n", v); } };
S make(int x) { return S(x); }
int main(int argc, char**) {
    S a(1);
    { const S& r = make(2); printf("r=%d\n", r.v); }
    { const S& r = argc ? a : make(3); printf("r=%d\n", r.v); }
    { const S& r = argc > 5 ? a : make(4); printf("r=%d\n", r.v); }
    { auto&& r = make(5); printf("r=%d\n", r.v); }
    for (int i = 0; i < 2; i++) { const S& r = make(6 + i); if (i == 1) break; printf("loop r=%d\n", r.v); }
    printf("end\n");
    return 0;
}
