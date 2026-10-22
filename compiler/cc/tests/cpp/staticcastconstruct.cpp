// static_cast<T>(e) of a value of no class, T a class: T's constructor
// from it, an explicit one too (WTF's clampTo<LayoutUnit>(double),
// `static_cast<TargetType>(value)`), not the value's bits; LayoutUnit's
// fromFloatRound was 2^29 for 68.12.
#include <cstdio>
#include <concepts>
#include <limits>
struct LU {
    int m;
    constexpr LU() : m(0) {}
    constexpr LU(int v) : m(v * 64) {}
    explicit LU(float v) : m(int(v * 64)) {}
    explicit LU(double v) : m(int(v * 64)) {}
    static constexpr LU raw(int v) { LU u; u.m = v; return u; }
    constexpr float toFloat() const { return m / 64.0f; }
    constexpr double toDouble() const { return m / 64.0; }
    constexpr operator float() const { return toFloat(); }
    constexpr operator double() const { return toDouble(); }
    static LU clamp(double value) { return clampTo<LU>(value, raw(std::numeric_limits<int>::min()), raw(std::numeric_limits<int>::max())); }
    template<typename T, typename S> requires (!std::same_as<T, S> && std::floating_point<S>)
    static constexpr T clampTo(S value, T min, T max)
    {
        if (value >= static_cast<S>(max)) { std::printf("  >= max %f\n", static_cast<S>(max)); return max; }
        if (!(value > static_cast<S>(min))) { std::printf("  <= min\n"); return min; }
        return static_cast<T>(value);
    }
};
int main(int argc, char**)
{
    volatile float w = 68.12f + argc - 1;
    LU a = LU::clamp(w + 1.0f / 128);
    std::printf("raw %d\n", a.m);
    LU b = static_cast<LU>(68.12);
    std::printf("cast raw %d\n", b.m);
    double d = static_cast<double>(LU::raw(std::numeric_limits<int>::max()));
    std::printf("max as double %f\n", d);
    return 0;
}
