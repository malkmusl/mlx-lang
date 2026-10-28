// A class whose members take no bytes (std::array<T, 0>) is returned and
// passed as an empty class is, in an integer register, its one byte
// stored (it was returned in xmm0 and stored as 8 bytes, over the
// caller's saved rbp: JSC's CCallHelpers::clampArrayToSize<0, FPRReg>).
#include <stdio.h>
#include <array>
enum class FReg : signed char { f0, f1, invalid = -1 };
template<unsigned N, typename T> __attribute__((noinline)) std::array<T, N> clamp(std::array<T, 4> in) { std::array<T, N> out; for (unsigned i = 0; i < N; ++i) out[i] = in[i]; return out; }
struct Both { float f; std::array<int, 0> none; };
__attribute__((noinline)) Both both(float f) { return Both { f, { } }; }
__attribute__((noinline)) int shuffle(std::array<FReg, 0> a, std::array<FReg, 0> b, int x) { return x + (int)a.size() + (int)b.size(); }
__attribute__((noinline)) int run(int x)
{
    std::array<FReg, 4> in { FReg::f1, FReg::f0, FReg::invalid, FReg::invalid };
    auto none = clamp<0>(in);
    auto none2 = clamp<0>(in);
    auto one = clamp<1>(in);
    return shuffle(none, none2, x) * 10 + (int)one[0] + (int)both(2.5f).f * 100;
}
int main() { printf("%d\n", run(4)); return 0; }
