// c ? classValue : pointer (and the other way round): the pointer converted to the class by its constructor (JSValue from a GetterSetter *)
#include <stdio.h>
#include <stdint.h>

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

int main() {
    p_condcls::run();
    return 0;
}
