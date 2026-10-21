// Brace-initialized aggregates with bit-fields (T x{...}); pointers to virtual member functions called through the vtable; a defaulted == beside a defaulted <=>; captures, mutable lambdas, structured bindings
#include <stdio.h>
#include <utility>
#include <tuple>

namespace p_broad1 {
static int copies, dtors;
struct C { int v; C(int x) : v(x) {} C(const C &o) : v(o.v) { copies++; } C(C &&o) : v(o.v) { o.v = -1; } ~C() { dtors++; } };
struct Pt { int x, y; auto operator<=>(const Pt &) const = default; bool operator==(const Pt &) const = default; };
struct VBase { virtual ~VBase() {} virtual int f() const { return 1; } int g() const { return f() * 10; } };
struct VDer : VBase { int f() const override { return 2; } };
union U { int i; float f; U() : i(5) {} };
enum class Kind : unsigned char { A = 1, B = 200 };
struct Flags { Kind k : 8; bool on : 1; unsigned rest : 7; };
int run() {
    C c(3);
    auto byCopy = [c] { return c.v; };
    auto byMove = [m = std::move(c)] { return m.v; };
    int counter = 0;
    auto mut = [counter]() mutable { return ++counter; };
    mut(); int second = mut();
    Pt a{1, 2}, b{1, 3};
    bool lt = a < b, eq = a == b, ge = b >= a;
    auto [x, y] = a;
    std::tuple<int, long, char> t{1, 2, 'c'};
    auto [t0, t1, t2] = t;
    int (VBase::*pmf)() const = &VBase::f;
    VDer d; const VBase &vb = d;
    U u;
    Flags fl{Kind::B, true, 99};
    printf("%d %d %d %d | %d %d %d | %d %d | %d %ld %c | %d %d | %d | %d %d %d\n", byCopy(), byMove(), c.v, second, lt, eq, ge, x, y, t0, t1, t2, (vb.*pmf)(), vb.g(), u.i, (int)fl.k, fl.on, fl.rest);
    return 0;
}
}

namespace p_bfinit {
enum class Kind : unsigned char { A = 1, B = 200 };
struct Flags { Kind k : 8; bool on : 1; unsigned rest : 7; };
struct F2 { unsigned a : 8; bool on : 1; unsigned rest : 7; };
struct F3 { Kind k : 8; unsigned rest : 7; };
int run() {
    Flags fl{Kind::B, true, 99};
    F2 f2{200, true, 99};
    F3 f3{Kind::B, 99};
    Flags g = {Kind::A, false, 5};
    Flags h; h.k = Kind::B; h.on = true; h.rest = 99;
    printf("%d %d %d | %d %d %d | %d %d | %d %d %d | %d %d %d\n", (int)fl.k, fl.on, fl.rest, f2.a, f2.on, f2.rest, (int)f3.k, f3.rest, (int)g.k, g.on, g.rest, (int)h.k, h.on, h.rest);
    return 0;
}
}

namespace p_pmfv {
struct VBase { virtual ~VBase() {} virtual int f() const { return 1; } virtual int h(int x) { return x; } int g() const { return 7; } };
struct VDer : VBase { int f() const override { return 2; } int h(int x) override { return x * 10; } };
int call(const VBase &b, int (VBase::*p)() const) { return (b.*p)(); }
int run() {
    VDer d;
    VBase *pb = &d;
    int (VBase::*pf)() const = &VBase::f;
    int (VBase::*pg)() const = &VBase::g;
    int (VBase::*ph)(int) = &VBase::h;
    printf("%d %d %d %d %d\n", (pb->*pf)(), (pb->*pg)(), (pb->*ph)(3), call(d, &VBase::f), pf == &VBase::f);
    return 0;
}
}

int main() {
    p_broad1::run();
    p_bfinit::run();
    p_pmfv::run();
    return 0;
}
