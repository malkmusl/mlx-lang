// A constructor's list naming several members of one anonymous struct
// (Skia's SkGlyphDigest, bit-fields of 20 bits: `: fPackedID{id},
// fIndex{index}`) makes each of them, the others by their default
// initializers; an anonymous union's member named, else its default one.
#include <cstdio>
#include <cstdint>
struct Bits {
    Bits(int x) : a{(uint64_t)x}, c(3) {}
    struct { uint64_t a : 20; uint64_t b : 20 = 9; uint64_t c : 3; };
};
struct Plain {
    Plain(int x) : first(x), second(x + 1) {}
    struct { uint32_t first; uint32_t second; uint32_t third = 42; };
    int after = 7;
};
struct Either {
    Either() {}
    Either(float f) : f(f) {}
    union { int i = 11; float f; };
};
int main()
{
    Bits b(5);
    std::printf("bits %d %d %d\n", (int)b.a, (int)b.b, (int)b.c);
    Plain p(20);
    std::printf("plain %u %u %u %d\n", p.first, p.second, p.third, p.after);
    Either e;
    Either g(2.5f);
    std::printf("union %d %g\n", e.i, g.f);
    return 0;
}
