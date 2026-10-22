// The temporaries of a branch (`c ? f()->data() : other`, `a && T(b)`,
// `a || T(b)`) live to the end of the full expression: a reference into
// one is still good while the expression copies from it (WebCore's
// Document::setURL, `c ? SecurityOrigin::create(url)->data() :
// topOrigin().data()`), and each is destroyed once, only when its branch
// ran, in a loop as well; one within a branch that was skipped (`(a &&
// T(x).on()) || (b && T(y).on())`, WebCore's hasOverlayScrollbars) is
// not destroyed either, whatever its flag's stack slot held.
#include <cstdio>
struct Data { int v; Data(int x) : v(x) {} Data(const Data& o) : v(o.v) { std::printf("  copy data %d\n", v); } };
struct Origin {
    Data d;
    Origin(int x) : d(x) { std::printf("  make origin %d\n", x); }
    ~Origin() { std::printf("  destroy origin %d\n", d.v); d.v = -1; }
    const Data& data() const { return d; }
};
struct Holder { Origin* p; Holder(int x) : p(new Origin(x)) {} ~Holder() { delete p; } Origin* operator->() const { return p; } };
static Holder create(int x) { return Holder(x); }
struct Flag { int v; Flag(int x) : v(x) { std::printf("  flag %d\n", v); } ~Flag() { std::printf("  ~flag %d\n", v); } bool on() const { return v > 1; } };
static __attribute__((noinline)) void dirty()
{
    volatile int junk[64];
    for (int i = 0; i < 64; i++) junk[i] = 0x10101;
}
static __attribute__((noinline)) bool either(bool a, bool b)
{
    return (a && Flag(40).on()) || (b && Flag(50).on());
}
int main(int argc, char**)
{
    dirty();
    std::printf("either %d\n", either(false, false));
    dirty();
    std::printf("either %d\n", either(true, true));
    dirty();
    std::printf("either %d\n", either(false, true));
    Origin top(7);
    bool c = argc > 0;
    auto a = c ? create(5)->data() : top.data();
    std::printf("a %d\n", a.v);
    auto b = !c ? top.data() : create(6)->data();
    std::printf("b %d\n", b.v);
    for (int i = 0; i < 4; i++) {
        bool x = (i % 2 == 0) && Flag(i).on();
        bool y = (i % 2 == 0) || Flag(i + 10).on();
        int z = (i < 2 ? Flag(i + 20).v : Flag(i + 30).v);
        std::printf("i %d: %d %d %d\n", i, x, y, z);
    }
    return 0;
}
