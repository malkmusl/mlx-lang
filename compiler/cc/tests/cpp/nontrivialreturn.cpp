// A class that is not trivially copyable is returned in memory whatever
// its size (the Itanium ABI): made right in the caller's object, so one of
// 16 bytes that points into itself still does after the return (it was
// returned in registers and its pointer left behind). Trivially copyable
// ones of 16 bytes or less stay in registers; std::string by value too.
#include <cstdio>
#include <string>

struct Self {
    Self(int v) : data(&inlineValue), inlineValue(v) { }
    Self(const Self& o) : data(&inlineValue), inlineValue(*o.data) { }
    ~Self() { if (data != &inlineValue) std::printf("left behind\n"); }
    int* data;
    int inlineValue;
};
static_assert(sizeof(Self) == 16);

struct Pair { long a; long b; };

static Self make(int v) { return Self(v); }
static Self named(int v)
{
    Self s(v);
    s.inlineValue += 1;
    return s;
}
struct Maker {
    virtual ~Maker() { }
    virtual Self get(int v) const { return Self(v * 2); }
};
static Pair pair(long a) { return { a, a + 1 }; }
static std::string text(int n) { return std::to_string(n) + "!"; }

int main()
{
    Self a = make(3);
    Self b = named(4);
    Maker maker;
    const Maker& m = maker;
    Self c = m.get(5);
    auto lambda = [](int v) { return Self(v + 100); };
    Self d = lambda(6);
    Pair p = pair(7);
    std::printf("%d %d %d %d %ld %ld %s\n", *a.data, *b.data, *c.data, *d.data, p.a, p.b, text(42).c_str());
    return 0;
}
