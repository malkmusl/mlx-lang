// Bit-field members made by a constructor's initializers (m_isFinalized(false)), by default member initializers (Tri m : 2 { ... }) and in copies: each into its bits
#include <stdio.h>
#include <string>

namespace p_bfctor {
enum class Tri : unsigned char { False, True, Indeterminate };
struct Table {
    bool m_has : 1;
    bool m_fin : 1;
    bool m_linked : 1;
    bool m_32 : 1;
    Tri m_opt : 2 { Tri::Indeterminate };
    unsigned m_num { 0 };
    Table() : m_has(false), m_fin(false), m_linked(false), m_32(false) {}
    Table(bool b, unsigned n) : m_has(true), m_fin(false), m_linked(false), m_32(b), m_num(n) {}
};
struct Def { unsigned a : 3 = 5; unsigned b : 4 = 9; int c = 7; };
int run() {
    Table t, u(true, 4);
    Def d;
    printf("%d %d %d %d %d | %d %d %d %d %d %u | %u %u %d\n", t.m_has, t.m_fin, t.m_linked, t.m_32, (int)t.m_opt, u.m_has, u.m_fin, u.m_linked, u.m_32, (int)u.m_opt, u.m_num, d.a, d.b, d.c);
    return 0;
}
}

namespace p_bfcopy {
struct F { bool a : 1; bool b : 1; unsigned c : 5; std::string s; F() : a(true), b(false), c(21), s("x") {} };
struct G { F f; unsigned d : 3 { 6 }; bool e : 1 { true }; };
int run() {
    G g; G h = g; G i; i = h; F f2(g.f); f2.b = true; F f3; f3 = f2;
    printf("%d %d %u %s %u %d | %d %d %u | %d %d %u\n", i.f.a, i.f.b, i.f.c, i.f.s.c_str(), i.d, i.e, f2.a, f2.b, f2.c, f3.a, f3.b, f3.c);
    return 0;
}
}

int main() {
    p_bfctor::run();
    p_bfcopy::run();
    return 0;
}
