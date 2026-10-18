// C++17/20 forms WTF's headers use: `class X final`, consteval and
// `if consteval`, a constructor delegating to a defaulted one,
// `X<U>::template f<T>()` and `x.template f<T>()`, `x = {a, b}`,
// friend classes named with a qualifier or a template, a parameter
// pack captured by a lambda, constrained `Concept auto` parameters, a
// function template declared before its definition, an explicit
// specialization deduced from its result type, a global with no
// constant value, function pointers as template arguments, class
// template argument deduction through guides, defaulted comparisons,
// nested namespace definitions, brace elision, placement new with
// several arguments, a template hidden by overloads declared after it.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <new>
#include <bit>
#include <span>
#include <compare>
#include <concepts>
#include <limits>

class Final final { public: int v = 4; };

consteval int twice(int x) { return x * 2; }
constexpr int where() { if consteval { return 1; } else { return 2; } }

struct Delegating {
    Delegating() = default;
    Delegating(decltype(nullptr)) : Delegating() {}
    int x = 3;
};

template <class U> struct Traits { template <class T> static T cast(U v) { return (T)(v * 2); } };
template <class T, class U> T byteCast(const U &value) { return Traits<U>::template cast<T>(value); }
struct Getter { template <class T> T get() const { return (T)7; } };
template <class X> int viaObject(const X &x) { return x.template get<int>(); }

namespace JSC { class Cached; }
template <typename> class DropLock;
struct WithFriends {
    friend class JSC::Cached;
    template <typename> friend class DropLock;
    int v = 1;
};

struct Out {
    void printImpl() {}
    template <class T, class... R> void printImpl(const T &v, const R &...rest) { show(v); printImpl(rest...); }
    void show(int v) { printf("%d ", v); }
    void show(const char *s) { printf("%s", s); }
    template <class F> void atomically(const F &f) { f(*this); }
    template <class... Types> void print(const Types &...values) { atomically([&](Out &out) { out.printImpl(values...); }); }
};
template <class... A> int sumAll(A... a) { auto f = [=]() { return (a + ... + 0); }; return f(); }

template <typename T> concept Dumpable = requires(const T &value) { { value.dump() }; };
struct Dumps { void dump() const { printf("dumped\n"); } };
void describe(int v) { printf("int %d\n", v); }
void describe(const Dumpable auto &value) { value.dump(); }
int doubled(std::integral auto x) { return x * 2; }

namespace W {
template <typename T> constexpr unsigned ctz(T value);
template <typename T> unsigned firstBit(T word) { return ctz(word); }
template <typename T> constexpr unsigned ctz(T value) { unsigned n = 0; while (value && !(value & 1)) { value >>= 1; ++n; } return n; }
}

template <typename T> T minimumFor() { return std::numeric_limits<T>::min(); }
template <> float minimumFor() { return -1.5f; }

static int counter() { static int n = 0; return ++n; }
int dynamicGlobal = counter() + 41;
constexpr float powerOfTwo(unsigned e) { float p = 1; while (e--) p *= 2; return p; }
template <typename T> constexpr float maxPlusOne = powerOfTwo(sizeof(T) * 8);

struct Lock { int n = 0; void lock() { n += 1; } void unlock() { n += 10; } };
static void lockIt(Lock *l) { l->lock(); }
static void unlockIt(Lock *l) { l->unlock(); }
template <typename T, void (lockFunction)(T *), void (*unlockFunction)(T *)>
struct ExternalLocker { explicit ExternalLocker(T *l) : m(l) { lockFunction(m); } ~ExternalLocker() { unlockFunction(m); } T *m; };
template <typename T> bool isLessThan(const T &a, const T &b) { return a < b; }
static bool greater(const int &a, const int &b) { return a > b; }
template <typename T, bool (*higher)(const T &, const T &) = &isLessThan<T>> struct Queue { static bool before(const T &a, const T &b) { return higher(a, b); } };

template <typename T> class Locker { public: explicit Locker(T &l) : m(l) { m.lock(); } ~Locker() { m.unlock(); } T &m; };
Locker(Lock &) -> Locker<Lock>;
template <typename T> struct Box { Box(T v) : value(v) {} T value; };
template <typename T> Box(T) -> Box<T>;

struct Point { int x; int y; bool operator==(const Point &) const = default; };
struct Seconds { long v; friend constexpr auto operator<=>(Seconds, Seconds) = default; };
struct Mixed { double a; int b; auto operator<=>(const Mixed &) const = default; };

namespace A::B::C { int f() { return 3; } }
namespace A::B { int g() { return C::f() + 1; } }

typedef union { uint8_t values[4]; uint32_t word; } Vec;
constexpr Vec splat(uint8_t c) { return Vec { c, c, c, c }; }

enum NotNullTag { NotNull };
inline void *operator new(size_t, NotNullTag, void *location) { return location; }
struct Placed { Placed(int n) : n(n) {} int n; };

template <typename Bits, typename In> constexpr bool isIdentical(In left, In right) { return std::bit_cast<Bits>(left) == std::bit_cast<Bits>(right); }
constexpr bool isIdentical(double left, double right) { return isIdentical<int64_t>(left, right); }

int main() {
    Final f;
    Delegating d(nullptr);
    printf("%d %d %d %d\n", f.v, twice(21), where(), d.x);
    printf("%d %d\n", byteCast<int>(2.5), viaObject(Getter()));
    Point p; p = { 3, 4 };
    WithFriends w;
    printf("%d %d %d\n", p.x, p.y, w.v);
    Out o; o.print(1, "x", 2, "\n");
    printf("%d\n", sumAll(1, 2, 3, 4));
    describe(5); describe(Dumps()); printf("%d\n", doubled(21));
    printf("%u %u\n", W::firstBit(8u), W::ctz(48u));
    printf("%d %g\n", minimumFor<int>() < 0, minimumFor<float>());
    printf("%d %g %g\n", dynamicGlobal, maxPlusOne<short>, maxPlusOne<char>);
    Lock l;
    { ExternalLocker<Lock, lockIt, &unlockIt> x(&l); }
    { Locker locker(l); }
    Box b(2.5);
    Box c { 7 };
    printf("%d %d %d %g %d\n", l.n, Queue<int>::before(1, 2), Queue<int, greater>::before(1, 2), b.value, c.value);
    Point q { 3, 4 }, r { 3, 5 };
    Seconds s1 { 1 }, s2 { 2 };
    Mixed m1 { 1.5, 2 }, m2 { 1.5, 3 };
    printf("%d %d %d %d %d %d %d\n", p == q, p != r, s1 < s2, s1 == s2, (s1 <=> s2) < 0, m1 < m2, m1 == m2);
    printf("%d %d %x\n", A::B::C::f(), A::B::g(), splat(1).word);
    void *mem = malloc(sizeof(Placed));
    Placed *pl = new (NotNull, mem) Placed(9);
    printf("%d %d %d %d\n", pl->n, (void *)pl == mem, isIdentical(0.0, -0.0), isIdentical(1.5, 1.5));
    free(mem);
    unsigned long a[4] = { 1, 2, 3, 4 };
    std::span<unsigned long> mutableSpan(a, 4);
    std::span<const unsigned long> constSpan;
    constSpan = mutableSpan.subspan(1);
    printf("%zu %lu\n", constSpan.size(), constSpan[0]);
    return 0;
}
