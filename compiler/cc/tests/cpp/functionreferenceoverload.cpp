// An overloaded function's name for a reference to a function: the
// overload of the reference's signature, as for a pointer (it was the
// first declared: WTF's interleave(list, serialize, ", "_s), whose
// parameter is a `void (&)(StringBuilder &, const MediaQuery &)`, took
// MQ::serialize of a MediaQueryList, and MediaQueryList::media() was
// empty, or a crash).
#include <cstdio>
#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>

struct Builder {
    int n = 0;
    char text[64] {};
    void add(const char *s, int v) { n += std::snprintf(text + n, sizeof text - n, "%s%d ", s, v); }
};
struct Item { int v; };
struct Feature { int a, b; };
using List = std::vector<Item>;

template <typename C> using EachFunction = void (&)(Builder &, const std::remove_reference_t<decltype(*std::begin(std::declval<C>()))> &);
template <typename C> void each(const C &container, EachFunction<C> f, Builder &b)
{
    for (auto &item : container)
        f(b, item);
}

void serialize(Builder &, const List &);
void serialize(Builder &, const Item &);
void serialize(Builder &b, const Feature &f) { b.add("feature", f.a); }
void serialize(Builder &b, const List &list) { each(list, serialize, b); }
void serialize(Builder &b, const Item &i) { b.add("item", i.v); }

void call(void (&f)(Builder &, const Item &), int v)
{
    Builder b;
    f(b, Item { v });
    std::printf("%s\n", b.text);
}
template <typename C> void callDependent(const C &, void (&f)(Builder &, const typename C::value_type &), int v)
{
    Builder b;
    f(b, Item { v });
    std::printf("%s\n", b.text);
}
struct Holder {
    void (&f)(Builder &, const Item &);
};

int main()
{
    List list { { 1 }, { 2 } };
    Builder b;
    serialize(b, list);
    std::printf("%s\n", b.text);
    call(serialize, 3);
    callDependent(list, serialize, 4);
    void (&r)(Builder &, const Item &) = serialize;
    Builder c;
    r(c, Item { 5 });
    Holder h { serialize };
    h.f(c, Item { 6 });
    auto &q = static_cast<void (&)(Builder &, const Item &)>(serialize);
    q(c, Item { 7 });
    std::printf("%s\n", c.text);
    return 0;
}
