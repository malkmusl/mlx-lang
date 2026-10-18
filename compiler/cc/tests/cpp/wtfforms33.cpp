// WebKit JSC forms: braced assignment to optional, auto... packs in constraints, vexing parses with argument expressions, initializer lists with pack expansions, UDL default arguments read by the caller, conditional temporaries destroyed once
#include <stdio.h>
#include <optional>
#include <tuple>
#include <initializer_list>
#include <new>

namespace p_op2 {
struct Cache { int m_days { 0 }; int m_year { 0 }; int m_month { 0 }; int m_day { 0 }; };
struct Info { int a; long b; };
struct M {
    std::optional<Cache> m_cache;
    std::optional<Info> m_info;
    int set(int d, int y) { m_cache = { d, y, 3, 4 }; m_info = { { 7, 8L } }; return m_cache->m_days + m_cache->m_year + m_cache->m_day + m_info->a; }
};
int run() { M m; printf("%d\n", m.set(10, 20)); return 0; }
}

namespace p_ap1 {
static int sum() { return 0; }
template <class T, class... R> static int sum(T a, R... r) { return (int)a + sum(r...); }
struct F {
    static int create(int len) { return len; }
    static int create(int len, auto... captures)
        requires (sizeof...(captures) > 0)
    {
        return len * 100 + sum(captures...) + (int)sizeof...(captures);
    }
};
int run() { printf("%d %d\n", F::create(3), F::create(2, 4, 5L, 'a')); return 0; }
}

namespace p_vp1 {
struct Str { const char* s; Str(const char* x) : s(x) { } };
struct URL { int base; int len; URL(const URL& b, const Str& rel) : base(b.base + 1), len(rel.s[0]) { } URL(int b) : base(b), len(0) { } };
struct Origin { URL u; explicit Origin(URL x) : u(x) { } };
int f(int(int, char));
int run() {
    URL dir(5);
    Origin o(URL(dir, "./x"));
    Origin p(URL(dir, Str("a")));
    printf("%d %d %d\n", o.u.base, o.u.len, p.u.len);
    return 0;
}
}

namespace p_pk1 {
struct W { long v; W(int base, long x) : v(base + x) { } };
template <class T> struct FV {
    T* d = nullptr; size_t n = 0;
    FV() = default;
    FV(std::initializer_list<T> l) : d((T*)::operator new(sizeof(T) * l.size())), n(l.size()) { size_t i = 0; for (auto& x : l) new (&d[i++]) T(x); }
    explicit FV(size_t k) : n(k) { }
};
struct F {
    FV<W> m_captures;
    F(int base, auto... captures) : m_captures { W(base, captures)... } { }
};
int run() { F f(10, 1, 2L, 3); long s = 0; for (size_t i = 0; i < f.m_captures.n; ++i) s += f.m_captures.d[i].v; printf("%zu %ld\n", f.m_captures.n, s); return 0; }
}

namespace p_ld2 {
struct Lit { const char* s; };
constexpr Lit operator""_s(const char* s, unsigned long) { return Lit { s }; }
template <typename T> struct LD { const T& m; Lit c; LD(const T& x, Lit cc) : m(x), c(cc) { } };
template <typename T> struct PLD { const T& m; Lit c; PLD(const T& x, Lit cc) : m(x), c(cc) { } };
template<typename T>
LD<T> listDump(const T& list, Lit comma = ", "_s)
{
    return LD<T>(list, comma);
}
int run() { int x = 4; auto d = listDump(x); printf("%d %s\n", d.m, d.c.s); return 0; }
}

namespace p_ud4 {
namespace N {
struct Lit { const char* s; };
Lit operator""_s(const char* s, unsigned long) { return Lit { s }; }
int show(int x, Lit comma = ", "_s) { return printf("%d %s\n", x, comma.s); }
template <class T> int showT(T x, Lit comma = ", "_s) { return printf("%d %s\n", x, comma.s); }
int run() { show(1); return 0; }
}
int run() { return N::run(); }
}

namespace p_cd5 {
static int ctors = 0, dtors = 0;
struct R { int* p = nullptr; R() { ++ctors; } R(int* x) : p(x) { ++ctors; } R(R&& o) : p(o.p) { o.p = nullptr; ++ctors; } R(const R& o) : p(o.p) { ++ctors; } ~R() { ++dtors; } };
static int take(R&& r) { return r.p ? *r.p : -1; }
static int takeV(R r) { return r.p ? *r.p : -1; }
int run() {
    int v = 5; bool c = true;
    int a = take(c ? R(&v) : R());
    int b = takeV(!c ? R(&v) : R());
    { R x = c ? R(&v) : R(); a += *x.p; }
    printf("%d %d %d\n", a, b, ctors == dtors);
    return 0;
}
}

int main() {
    p_op2::run();
    p_ap1::run();
    p_vp1::run();
    p_pk1::run();
    p_ld2::run();
    p_ud4::run();
    p_cd5::run();
    return 0;
}
