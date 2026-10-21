// Tuples holding a class with its own copy and move, pushed into a
// vector from braced lists through its reallocations, then sorted by a
// projection (a WTF probe of this shape once ended in "free(): invalid
// pointer").
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <algorithm>
#include <tuple>
#include <vector>

struct Str {
    char *p = nullptr;
    Str() = default;
    Str(const char *s) : p(strdup(s)) { }
    Str(const Str &o) : p(o.p ? strdup(o.p) : nullptr) { }
    Str(Str &&o) : p(o.p) { o.p = nullptr; }
    Str &operator=(const Str &o) { if (this != &o) { free(p); p = o.p ? strdup(o.p) : nullptr; } return *this; }
    Str &operator=(Str &&o) { Str t(static_cast<Str &&>(o)); char *q = p; p = t.p; t.p = q; return *this; }
    ~Str() { free(p); }
    const char *c() const { return p ? p : ""; }
};
static bool lessStr(const Str &a, const Str &b) { return strcmp(a.c(), b.c()) < 0; }

int main(int argc, char **argv)
{
    std::vector<std::tuple<long, Str>> v;
    v.push_back({ 3, Str("c") });
    v.push_back({ 1, Str("a") });
    v.push_back({ 2, Str("b") });
    for (auto &e : v) printf("%ld %s\n", std::get<0>(e), std::get<1>(e).c());
    std::ranges::sort(v, lessStr, [](const auto &e) { return std::get<1>(e); });
    for (auto &e : v) printf("%ld %s\n", std::get<0>(e), std::get<1>(e).c());
    return argc - 1;
}
