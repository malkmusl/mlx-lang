// Bit-fields in constant expressions: a member of a temporary aggregate
// (WebCore's Node.cpp, `static_assert(RareDataBitFields { Page::
// maxNumberOfFrames, 0, 0, 0, 0 }.connectedSubframeCount == ...)`), and
// written and read by a constexpr function.
#include <cstdint>
#include <cstdio>
struct Bits {
    uint16_t count : 10;
    uint16_t state : 2;
    uint16_t custom : 2;
    uint16_t a : 1;
    uint16_t b : 1;
};
struct Signed { int low : 4; int high : 4; };
struct Page { static constexpr int maxFrames = 1000; };
constexpr int packed(int c, int s)
{
    Bits bits { 0, 0, 0, 0, 0 };
    bits.count = c;
    bits.state = s;
    bits.b = 1;
    return bits.count * 100 + bits.state * 10 + bits.b;
}
constexpr int negative()
{
    Signed s { 0, 0 };
    s.low = -3;
    s.high = 7;
    return s.low * 10 + s.high;
}
static_assert(Bits { Page::maxFrames, 0, 0, 0, 0 }.count == Page::maxFrames, "fits");
static_assert(Bits { 1024 + 5, 0, 0, 0, 0 }.count == 5, "wraps");
static_assert(packed(12, 3) == 1231);
static_assert(negative() == -23);
int main() {
    constexpr int p = packed(7, 2);
    std::printf("%d %d\n", p, negative());
    return 0;
}
