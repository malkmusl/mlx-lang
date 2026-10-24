// `auto t = T { args };` of a class with constructors: the temporary the
// braces make is the object itself (C++17: no move, no temporary
// destroyed), as of `T(args)` (WebCore's `auto textBoxTrimmer =
// TextBoxTrimmer { *this };`, whose destructor reads its CheckedPtr).
#include <cstdio>
struct P { int* p; P(int* q) : p(q) {} P(P&& o) : p(o.p) { o.p = nullptr; std::printf("  P move\n"); } ~P() {} };
struct T {
    T(int& x) : m(&x) { std::printf("  make\n"); }
    T(int& x, int) : m(&x) { std::printf("  make2\n"); }
    ~T() { std::printf("  destroy %s\n", m.p ? "set" : "NULL"); }
    P m;
};
int main()
{
    int v = 1;
    std::printf("auto = T { v }\n"); { auto t = T { v }; }
    std::printf("auto = T(v)\n"); { auto t = T(v); }
    std::printf("T t = T { v, 0 }\n"); { T t = T { v, 0 }; }
    std::printf("auto = T { v, 0 }\n"); { auto t = T { v, 0 }; }
    return 0;
}
