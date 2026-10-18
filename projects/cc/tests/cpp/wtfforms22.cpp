// WTF/JSC forms 22: constexpr objects (constructors, members, results by value, arrays, constant globals)
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

namespace p_cxobj {
struct OT {
    constexpr OT(uint8_t bits = TypeEmpty) : m_bits(bits) { }
    constexpr uint8_t bits() const { return m_bits; }
    constexpr OT withInt32() const { return OT(m_bits | TypeInt32); }
    constexpr OT withNumber() const { return OT(m_bits | TypeNumber); }
    static constexpr uint8_t TypeEmpty = 0x0;
    static constexpr uint8_t TypeInt32 = 0x1;
    static constexpr uint8_t TypeNumber = 0x02;
private:
    uint8_t m_bits { 0 };
};
struct P {
    static constexpr uint32_t rs = 3;
    static constexpr uint32_t ls = rs + 3;
    static constexpr uint16_t intInt() {
        constexpr OT i32 { OT().withInt32() };
        constexpr uint16_t bits = (i32.bits() << ls) | (i32.bits() << rs);
        return bits;
    }
    static constexpr uint16_t numInt() {
        constexpr OT n { OT().withNumber() };
        constexpr OT i32 { OT().withInt32() };
        constexpr uint16_t bits = (n.bits() << ls) | (i32.bits() << rs);
        return bits;
    }
};
int run() { constexpr int64_t c = static_cast<int64_t>(P::intInt()); constexpr int64_t d = P::numInt(); static const int64_t t[] = { c, d }; printf("%ld %ld\n", (long)t[0], (long)t[1]); return 0; }
}

namespace p_cxobj2 {
struct Pt { int x = 1; int y { 2 }; constexpr Pt() = default; constexpr Pt(int a, int b) : x(a), y(b) { } constexpr int sum() const { return x + y; } };
struct Box { Pt lo; Pt hi; constexpr Box(Pt a, Pt b) : lo(a), hi(b) { } constexpr int area() const { return (hi.x - lo.x) * (hi.y - lo.y); } constexpr const Pt& low() const { return lo; } };
constexpr Pt add(Pt a, Pt b) { return Pt(a.x + b.x, a.y + b.y); }
constexpr int arr() { int t[5] = { }; for (int i = 0; i < 5; i++) t[i] = i * i; int s = 0; for (int i = 0; i < 5; i++) s += t[i]; return s; }
constexpr int mut() { Pt p(3, 4); p.x += 10; p.y = p.x * 2; return p.sum(); }
constexpr Pt origin { 5, 6 };
constexpr int viaGlobal() { return origin.sum() + origin.x; }
struct Flags { uint32_t bits = 0; constexpr Flags with(uint32_t b) const { Flags f = *this; f.bits |= b; return f; } };
constexpr uint32_t chain() { return Flags().with(1).with(4).with(16).bits; }
static_assert(add(Pt(1, 2), Pt(3, 4)).sum() == 10);
static_assert(Box(Pt(0, 0), Pt(3, 5)).area() == 15);
static_assert(Box(Pt(7, 8), Pt(9, 9)).low().y == 8);
static_assert(arr() == 30);
static_assert(mut() == 39);
static_assert(viaGlobal() == 16);
static_assert(chain() == 21);
static_assert(Pt().sum() == 3);
int run() {
    static const int64_t t[] = { add(Pt(1, 2), Pt(3, 4)).sum(), Box(Pt(0, 0), Pt(3, 5)).area(), arr(), mut(), viaGlobal(), chain(), Pt().sum() };
    for (auto v : t) printf("%ld ", (long)v);
    printf("\n");
    return 0;
}
}

namespace p_cxvoid {
typedef int PropertyOffset;
static constexpr PropertyOffset firstOut = 64;
constexpr inline void validate(PropertyOffset);
constexpr inline intptr_t inOut(PropertyOffset);
constexpr inline void check(PropertyOffset offset) { (void)offset; ((void)0); }
constexpr inline void validate(PropertyOffset offset) { check(offset); ((void)0); }
constexpr inline intptr_t inOut(PropertyOffset offset) { validate(offset); ((void)0); return -static_cast<ptrdiff_t>(offset - firstOut) - 1; }
struct Butterfly { constexpr static int indexOfStorage() { ((void)0); return -1; } };
constexpr inline intptr_t inButterfly(PropertyOffset offset) { return inOut(offset) + Butterfly::indexOfStorage(); }
const int64_t* d() {
    constexpr int64_t c0 = static_cast<int64_t>((inButterfly(firstOut)) * sizeof(int64_t));
    static const int64_t t[] = { 1, c0 };
    return t;
}
int run() { const int64_t* t = d(); printf("%ld %ld\n", (long)t[0], (long)t[1]); return 0; }
}

namespace p_localcx {
typedef int PropertyOffset;
static constexpr PropertyOffset firstOut = 64;
constexpr int offsetIn(PropertyOffset o) { return -static_cast<int>(o - firstOut) - 2; }
const int64_t* d() {
    constexpr int64_t c0 = static_cast<int64_t>((offsetIn(firstOut)) * sizeof(int64_t));
    constexpr int64_t c1 = static_cast<int64_t>(sizeof(long) * 3);
    static const int64_t t[] = { 1, c0, c1, unsigned(5) };
    return t;
}
int run() { const int64_t* t = d(); printf("%ld %ld %ld %ld\n", (long)t[0], (long)t[1], (long)t[2], (long)t[3]); return 0; }
}

int main() {
    p_cxobj::run();
    p_cxobj2::run();
    p_cxvoid::run();
    p_localcx::run();
    return 0;
}
