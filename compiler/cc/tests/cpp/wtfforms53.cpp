// c ? classValue : other (and the other way round): the other side made into the class by its constructor (JSValue from a GetterSetter *, std::string from a literal); one result destroyed by the expression
#include <stdio.h>
#include <stdint.h>
#include <string>

namespace p_condcls {
struct Cell { int x; };
struct GetterSetter : Cell {};
struct Value {
    uint64_t bits;
    Value() : bits(0) {}
    Value(Cell *c) : bits((uint64_t)(uintptr_t)c) {}
    Value(const Cell *c) : bits((uint64_t)(uintptr_t)c | 1) {}
    explicit Value(bool b) : bits(b ? 7 : 6) {}
    Value(int i) : bits(0xffff000000000000ull | (uint32_t)i) {}
    Value(double d) : bits(0x1000) { (void)d; }
};
static GetterSetter gs;
struct Slot { bool cacheable; Value v; Value getValue() const { return v; } GetterSetter *getterSetter() const { return &gs; } };
int run() {
    Slot s{false, Value(5)};
    Value r = s.cacheable ? s.getValue() : s.getterSetter();
    Value r2 = !s.cacheable ? s.getterSetter() : s.getValue();
    printf("%d %d\n", r.bits == (uint64_t)(uintptr_t)&gs, r2.bits == (uint64_t)(uintptr_t)&gs);
    return 0;
}
}

namespace p_cond2 {
struct V { long b; V() : b(0) {} V(int i) : b(100 + i) {} V(const char *) : b(7) {} };
struct Base { int x = 1; virtual ~Base() {} }; struct Der : Base { int y = 2; };
static int dtors;
struct R { int v; R(int x) : v(x) {} R(const R &o) : v(o.v) {} ~R() { dtors++; } };
int run() {
    bool t = true, f = false;
    V a = t ? V() : 5;
    V b = f ? 5 : V();
    V c = t ? "x" : V(3);
    Der d; Base bb;
    Base &r = t ? static_cast<Base &>(d) : bb;
    std::string s = t ? std::string("yes") : "no";
    std::string s2 = f ? "yes" : std::string("no");
    R rr = t ? R(1) : R(2);
    R r2 = f ? 9 : R(3);
    printf("%ld %ld %ld %d %s %s %d %d\n", a.b, b.b, c.b, r.x, s.c_str(), s2.c_str(), rr.v, r2.v);
    return 0;
}
}

int main() {
    p_condcls::run();
    p_cond2::run();
    return 0;
}
