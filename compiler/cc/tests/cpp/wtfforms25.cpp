// WTF/JSC forms 25: + and - through conversions, trailing commas, empty packs before arguments, invoke_result of member functions, defaulted copies outside the class
#include <stdio.h>
#include <new>
#include <type_traits>
#include <string>

namespace p_posconv {
struct Pos {
    int line = 0; int offset = 0;
    Pos() = default;
    Pos(int l, int o) : line(l), offset(o) { }
    Pos operator+(int a) const { return Pos(line, offset + a); }
    Pos operator-(int a) const { return Pos(line, offset - a); }
    operator int() const { return offset; }
    friend bool operator==(const Pos&, const Pos&) = default;
};
struct N { Pos d { 1, 70000 }; Pos divot() const { return d; } int f(const Pos& s) { if ((s.offset - divot()) & ~0xFFFF) return 1; if ((s.line - divot().line) & ~0xFFFF) return 2; return 3; } };
int run() { N n; printf("%d %d\n", n.f(Pos(1, 70010)), n.f(Pos(1, 5))); return 0; }
}

namespace p_trailcomma {
struct V { int t; bool s; const char* str; V(int a, bool b, const char* c) : t(a), s(b), str(c) { } };
V make(int k) { return V { k, true, "x", }; }
int run() { V v = make(4); auto w = V { 5, false, "y", }; printf("%d %d %s %d\n", v.t, v.s, w.str, w.t); return 0; }
}

namespace p_packmid {
static int count = 0;
template<typename... T> void dataLog(const T&... v) { count += sizeof...(v); }
template<typename... Types> void dataLogLn(const Types&... values) { dataLog(values..., "\n"); }
struct L { int a; int b; };
int run() { dataLogLn(); dataLogLn(1, 2); alignas(8) char buf[16]; L* l = new (buf) L { 3, 4, }; printf("%d %d\n", count, l->b); return 0; }
}

namespace p_invres2 {
struct Vec { int at(unsigned i) const { return (int)i; } };
int run() { decltype(&Vec::at) p = &Vec::at; std::invoke_result<decltype(&Vec::at), const Vec, unsigned>::type x = 3; std::invoke_result_t<decltype(&Vec::at), const Vec&, unsigned> y = 4; Vec v; printf("%d %d %d\n", x, y, (v.*p)(5)); return 0; }
}

namespace p_outdefault {
struct Base { int b = 1; };
struct V : Base {
    std::string name; int arr[3]; int n;
    V() : name("init"), arr{1, 2, 3}, n(4) { }
    V(const V&);
    V(V&&);
    V& operator=(const V&);
    ~V();
};
V::V(const V&) = default;
V::V(V&&) = default;
V& V::operator=(const V&) = default;
V::~V() = default;
int run() {
    V a; a.name = "alpha"; a.arr[1] = 20; a.b = 7; a.n = 9;
    V b(a); V c; c = a; V d(static_cast<V&&>(c));
    printf("%s %d %d %d | %s %d %d %d | %s %d\n", b.name.c_str(), b.arr[1], b.b, b.n, d.name.c_str(), d.arr[1], d.b, d.n, c.name.c_str(), c.n);
    return 0;
}
}

int main() {
    p_posconv::run();
    p_trailcomma::run();
    p_packmid::run();
    p_invres2::run();
    p_outdefault::run();
    return 0;
}
