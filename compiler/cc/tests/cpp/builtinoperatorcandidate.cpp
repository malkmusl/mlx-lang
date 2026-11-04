// The built-in operator is a candidate too ([over.match.oper]): a class
// converting to unsigned minus an unsigned is the built-in subtraction,
// better than `operator-(int, const LU &)` reached through the class's
// conversion and LU's converting constructor (WebCore's cssjit
// StackAllocator took LayoutUnit's, its stack offsets in sixty-fourths:
// the selector JIT's code read a stack slot a byte off).
#include <stdio.h>
#include <limits>
namespace N {
class LU {
public:
    LU() { }
    constexpr LU(int v) : raw(v * 64) { }
    LU(unsigned v) : raw(v * 64) { }
    explicit LU(double v) : raw(v * 64) { }
    constexpr operator int() const { return raw / 64; }
    constexpr operator float() const { return raw / 64.f; }
    explicit constexpr operator bool() const { return raw; }
    int raw = 0;
};
inline LU operator-(const LU& a, const LU& b) { LU r; r.raw = a.raw - b.raw; return r; }
inline LU operator-(const LU& a, unsigned b) { return a - LU(b); }
inline float operator-(const LU& a, float b) { return a.raw / 64.f - b; }
inline LU operator-(const int a, const LU& b) { return LU(a) - b; }
inline bool operator<(const int a, const LU& b) { return a * 64 < b.raw; }
class Stack {
public:
    class Ref {
    public:
        Ref() : m(std::numeric_limits<unsigned>::max()) { }
        explicit Ref(unsigned o) : m(o) { }
        operator unsigned() const { return m; }
    private:
        unsigned m;
    };
    unsigned before(Ref ref) { return ref - unit(); }
    bool under(Ref ref) { return ref < 9u; }
    static unsigned unit() { return 8; }
};
}
int main()
{
    N::Stack s;
    N::LU five(5);
    printf("%u %d %d %d\n", s.before(N::Stack::Ref(16)), s.under(N::Stack::Ref(8)), (int)(five - 2u), (int)(7 - five));
    return 0;
}
