// Overload resolution and conversions: member templates beside plain
// members, qualified calls of templates, operators of nested classes
// found by their namespace, copies that are not conversions, the more
// specialized template, references to functions and through conversion
// operators, char8_t and the character literals' types, ::name past a
// local, while with a declaration and range-for iterator lifetimes.
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <type_traits>
#include <utility>

struct L { int v; };
struct C {
    void wait(L &l) { l.v++; }
    template <class P> void wait(L &l, P pred) { while (!pred()) wait(l); }
    template <class P> void w2(L &l, P pred) { while (!pred()) w2(l); }
    void w2(L &l) { l.v += 10; }
};

namespace q {
enum class F { a = 1, b = 2 };
template <class T, class = typename std::enable_if<std::is_integral<T>::value>::type>
int t(char *, T, int base = 10) { return 1000 + base; }
inline int t(char *, double) { return 1; }
inline int t(char *, double, F) { return 2; }
struct T { struct id { int h; }; id get() const { return id{h}; } int h; };
inline bool operator!=(T::id a, T::id b) { return a.h != b.h; }
}

struct Cat { virtual ~Cat() {} };
inline const Cat &cat() { static Cat c; return c; }
enum class E { a = 1, b = 2 };
struct Code {
    Code(int value, const Cat &c) noexcept : _value(value), _c(&c) {}
    Code(E e) noexcept : _value((int)e + 100), _c(&cat()) {}
    int value() const noexcept { return _value; }
    explicit operator bool() const noexcept { return _value != 0; }
private:
    int _value;
    const Cat *_c;
};
inline Code make(E e) noexcept { return Code((int)e, cat()); }

template <class R> struct D {
    template <class R2> explicit D(const R2 &c) : count((R)c) {}
    template <class R2> D(const D<R2> &o) : count((R)o.count * 1000) {}
    D(const D &o) : count(o.count) {}
    R count;
};

void work(int n) { printf("work %d\n", n); }
template <class F> void call(F &&f) { f(1); void (*p)(int) = std::forward<F>(f); p(2); }

namespace n { struct P { int v; }; }
struct Wrap {
    n::P p;
    operator const n::P &() const { return p; }
};
int get(const n::P &x) { return x.v; }

void g(unsigned char) { puts("uchar"); }
void g(char8_t) { puts("char8"); }
void h(const char *) { puts("char"); }
void h(const char8_t *) { puts("char8"); }
void h(const char16_t *) { puts("char16"); }
void h(const char32_t *) { puts("char32"); }
void h(const wchar_t *) { puts("wchar"); }

struct It {
    int *count; int i;
    It(int *c, int v) : count(c), i(v) { ++*count; }
    It(const It &o) : count(o.count), i(o.i) { ++*count; }
    ~It() { --*count; }
    int operator*() const { return i; }
    It &operator++() { ++i; return *this; }
    bool operator!=(const It &o) const { return i != o.i; }
};
struct R { int *c; It begin() { return It(c, 0); } It end() { return It(c, 3); } };
namespace rn { struct Q { int *c; }; inline It begin(Q x) { return It(x.c, 5); } inline It end(Q x) { return It(x.c, 7); } }

int *nextOf(int *p, int *end) { return p + 1 < end ? p + 1 : 0; }

int main() {
    C c; L l{0}; int k = 0;
    c.wait(l, [&k] { return ++k > 3; });
    c.w2(l, [&k] { return ++k > 6; });
    printf("waits %d %d\n", l.v, k);

    char b[4];
    printf("qualified %d %d %d\n", q::t(b, 255u, 16), q::t(b, 5), q::t(b, 2.0));
    q::T x{1}, y{2};
    printf("nested %d %d\n", (int)(x.get() != y.get()), (int)(x.get() != x.get()));

    E e = E::b;
    Code copied = Code((int)e, cat());
    printf("copies %d %d %d\n", copied.value(), make(e).value(), Code(e).value());

    D<long long> ms((long long)1500);
    D<long long> s = ms;
    D<double> other = ms;
    printf("special %lld %lld %g\n", ms.count, s.count, other.count);

    void (&r)(int) = work;
    r(0);
    call(work);

    Wrap w; w.p.v = 7;
    printf("conversion %d\n", get(w));

    char8_t c8 = 65; unsigned char uc = 66;
    g(c8); g(uc); g(u8'a');
    h("a"); h(u8"a"); h(u"a"); h(U"a"); h(L"a");
    printf("char8 %d %d %d\n", (int)std::is_same<char8_t, unsigned char>::value, (int)sizeof(u8"ab"), (int)sizeof(u"ab"));

    struct stat info;
    int stat = 5;
    printf("scope %d %d\n", (int)(::stat("/", &info) == 0), stat);

    int values[4] = { 1, 2, 3, 4 };
    int total = 0;
    int *p = values;
    while (int *at = nextOf(p, values + 4)) { total += *at; p = at; }
    printf("while %d\n", total);

    int live = 0;
    R range{&live};
    for (int v : range) printf("range %d live %d\n", v, live);
    rn::Q other2{&live};
    for (int v : other2) printf("adl %d live %d\n", v, live);
    printf("after %d\n", live);
    return 0;
}
