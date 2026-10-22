// An object of the class itself is assigned by its copy or move
// assignment (declared `= default`, or not at all), not by another
// operator= reached through a conversion of the object: WebCore's
// LayoutUnit has `operator=(float other) { return *this =
// LayoutUnit(other); }` and `operator float()`, and called itself.
#include <cstdio>
struct LayoutUnit {
    LayoutUnit() = default;
    explicit LayoutUnit(double v) : value(int(v * 64)) { }
    LayoutUnit& operator=(const LayoutUnit&) = default;
    LayoutUnit& operator=(float other) { return *this = LayoutUnit(other); }
    constexpr operator int() const { return value / 64; }
    constexpr operator float() const { return value / 64.0f; }
    constexpr operator double() const { return value / 64.0; }
    int value { 0 };
};
struct Implicit {
    Implicit() = default;
    explicit Implicit(int v) : value(v) { }
    Implicit& operator=(int other) { return *this = Implicit(other * 2); }
    operator int() const { return value; }
    int value { 0 };
};
struct Derived : Implicit {
    using Implicit::Implicit;
};
int main()
{
    Implicit a;
    a = 21;
    Implicit b;
    b = Derived(5);
    std::printf("%d %d\n", a.value, b.value);
    LayoutUnit u;
    u = 2.5f;
    LayoutUnit v;
    v = u;
    std::printf("%d %d\n", u.value, v.value);
    return 0;
}
