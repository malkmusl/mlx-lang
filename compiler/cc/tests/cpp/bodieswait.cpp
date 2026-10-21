// Inline and static functions whose bodies wait for a use (cpp.lazyBodies):
// called before and after their definitions, named by address only,
// through a function pointer, from a constant expression, from another
// waiting body, with default arguments the definition gives, as members
// defined inside and outside their classes, in namespaces with
// using-directives, virtual members, recursion, __func__ and static
// locals, and never used at all.
#include <cstdio>

namespace outer {
namespace inner {
inline int twice(int x) { return 2 * x; }
inline int unused(int x) { return x * 77; }
}
using namespace inner;
inline int fourTimes(int x) { return twice(twice(x)); }
}

static int calledBeforeDefinition(int);
inline int forward(int x) { return calledBeforeDefinition(x) + 1; }
static int calledBeforeDefinition(int x) { return x * 10; }

static int onlyByAddress(int x) { return x - 3; }
inline int viaPointer(int (*f)(int), int x) { return f(x); }

constexpr int square(int x) { return x * x; }
inline constexpr int cube(int x) { return square(x) * x; }
static_assert(cube(3) == 27, "cube");
int table[square(4)];

int withDefault(int x, int y);
inline int withDefault(int x, int y = 5) { return x + y; }

struct Counter {
    int value = 0;
    int bump() { return ++value; }
    int bumpTwice();
    int unusedMember() { return value * 99; }
    static int made() { return 41; }
};
inline int Counter::bumpTwice() { bump(); return bump(); }

struct Base {
    virtual ~Base() {}
    virtual int kind() const { return 1; }
};
struct Derived : Base {
    int kind() const override { return helper(); }
    int helper() const { return 2; }
};

// A static member of a class template's instance only a body nothing
// uses would have made whole: made whole for its definition.
template <class T> struct Wrap { T v; };
struct Options {
    enum class Kind : unsigned short { A = 1, B = 2 };
    using Set = Wrap<Kind>;
    static constexpr Set defaults = { Kind::B };
    int kind() const { Set s = defaults; return (int)s.v; }
};

inline int recursive(int n) { return n <= 1 ? 1 : n * recursive(n - 1); }
inline int mutualB(int);
inline int mutualA(int n) { return n == 0 ? 0 : 1 + mutualB(n - 1); }
inline int mutualB(int n) { return n == 0 ? 0 : 1 + mutualA(n - 1); }

inline const char *name() { return __func__; }
inline int counter() { static int calls = 0; return ++calls; }

int main() {
    printf("%d %d\n", outer::fourTimes(3), forward(4));
    int (*f)(int) = onlyByAddress;
    printf("%d %d\n", viaPointer(f, 10), viaPointer(&onlyByAddress, 4));
    printf("%d %d %d\n", cube(2), (int)(sizeof table / sizeof table[0]), withDefault(1));
    Counter c;
    printf("%d %d\n", c.bumpTwice(), Counter::made());
    Derived d;
    Base &b = d;
    printf("%d\n", b.kind());
    printf("%d %d\n", recursive(5), mutualA(7));
    counter();
    printf("%s %d\n", name(), counter());
    return 0;
}
