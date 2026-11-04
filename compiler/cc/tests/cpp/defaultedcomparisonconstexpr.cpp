// A defaulted comparison is constexpr (WTF::Range's friend operator==
// in B3's static_assert over its constant effects table), a member one
// too.
#include <stdio.h>
#include <stdint.h>
template<typename T> class Range {
public:
    constexpr Range() = default;
    constexpr Range(T b, T e) : m_begin(b), m_end(e) { }
    static constexpr Range top() { return Range(0, UINT64_MAX); }
    friend bool operator==(const Range&, const Range&) = default;
private:
    T m_begin { 0 };
    T m_end { 0 };
};
struct P { int a; int b; bool operator==(const P&) const = default; };
struct Effects { bool exits { false }; Range<uint64_t> reads; };
constexpr Effects make(int i)
{
    Effects e;
    if (i & 1) {
        e.reads = Range<uint64_t>::top();
        e.exits = true;
    }
    return e;
}
constexpr bool check()
{
    for (int i = 0; i < 10; ++i) {
        Effects e = make(i);
        if (e.exits && !(e.reads == Range<uint64_t>::top()))
            return false;
    }
    return true;
}
static_assert(check());
static_assert(P { 1, 2 } == P { 1, 2 });
static_assert(!(P { 1, 2 } == P { 1, 3 }));
int main() { printf("%d %d\n", check(), P { 3, 4 } == P { 3, 5 }); return 0; }
