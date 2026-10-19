// The elements of a braced argument read only for their types (the
// conversion ranks) make no temporaries the call destroys:
// `v.push_back({ 3, Str("c") })` destroyed a Str that was never made
// ("free(): invalid pointer").
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <initializer_list>
#include <tuple>
#include <utility>
#include <vector>

static int made, unmade;

struct Str {
    char *p = nullptr;
    Str() = default;
    Str(const char *s) : p(strdup(s)) { made++; }
    Str(const Str &o) : p(o.p ? strdup(o.p) : nullptr) { if (p) made++; }
    Str(Str &&o) : p(o.p) { o.p = nullptr; }
    ~Str() { if (p) unmade++; free(p); }
    const char *c() const { return p ? p : "-"; }
};

struct Two {
    long a;
    Str s;
    Two(const long &a, Str &&s) : a(a), s(static_cast<Str &&>(s)) { }
};

struct Swapped {
    Str s;
    long a;
    template<class S, class A> Swapped(S &&s, A &&a) : s(std::forward<S>(s)), a(a) { }
};

static long take(Two &&t) { return t.a + (long)strlen(t.s.c()); }
static long takeSwapped(const Swapped &t) { return t.a + (long)strlen(t.s.c()); }
static size_t count(std::initializer_list<Str> list) { return list.size(); }

int main(int argc, char **argv)
{
    std::vector<std::tuple<long, Str>> v;
    v.push_back({ 3, Str("c") });
    v.push_back({ 1, Str("a") });
    v.push_back({ 2, Str("bb") });
    for (auto &e : v) printf("%ld %s\n", std::get<0>(e), std::get<1>(e).c());
    printf("%ld %ld\n", take({ 4, Str("four") }), takeSwapped({ Str("x"), argc }));
    v.clear();
    printf("%d\n", made - unmade);
    printf("%zu\n", count({ Str("one"), Str("two"), Str("three") }));
    return argc;
}
