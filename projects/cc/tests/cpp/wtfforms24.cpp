// WTF/JSC forms 24: deduction guides from constructor templates, references made with parentheses, overload candidates not taken
#include <stdio.h>
#include <utility>

namespace p_ctadsfs {
template<typename T> class SetForScope {
public:
    SetForScope(T& v) : m_v(v), m_old(v) { }
    template<typename U> SetForScope(T& v, U&& n) : SetForScope(v) { m_v = std::forward<U>(n); }
    template<typename U, typename V> SetForScope(T& v, U&& n, V&& r) : m_v(v), m_old(std::forward<V>(r)) { m_v = std::forward<U>(n); }
    ~SetForScope() { m_v = std::move(m_old); }
private:
    T& m_v; T m_old;
};
struct G {
    bool m_allow { true };
    bool m_bits : 1 { true };
    int run() { SetForScope p(m_allow, false); return m_allow ? 1 : 0; }
};
int run() { G g; int r = g.run(); printf("%d %d\n", r, g.m_allow); return 0; }
}

namespace p_ovtmpl {
struct V { V(int) { } };
struct B {
    struct Impl { B* p; int sign() const { return 7; } };
    static int cmp(B* x, double y);
    static int cmp(double x, B* y);
    template<typename I> static int cmp(I x, double y);
    template<typename I> static int cmp(double x, I y) { return -cmp(y, x); }
    static int cmp(int x, double y);
    static int cmp(double x, int y);
    static int cmp(V x, double y);
    bool eq(double v) { return cmp(this, v) == 1; }
};
int B::cmp(B* x, double y) { return cmp(Impl { x }, y) == 7 ? 1 : 0; }
int B::cmp(double x, B* y) { return cmp(x, Impl { y }); }
template<typename I> int B::cmp(I x, double y) { return x.sign(); }
int B::cmp(int x, double y) { return 2; }
int B::cmp(double x, int y) { return 3; }
int B::cmp(V x, double y) { return 4; }
int run() { B b; printf("%d %d %d\n", b.eq(1.5), B::cmp(2, 1.0), B::cmp(1.0, &b)); return 0; }
}

int main() {
    p_ctadsfs::run();
    p_ovtmpl::run();
    return 0;
}
