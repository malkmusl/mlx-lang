// `V v { n }` of a class with a constructor taking a size and one taking
// an initializer_list: the list's only when an element is made from n
// (WTF's `Vector<LineAdjustment> adjustments { lineCount }` is lineCount
// elements: an aggregate is not made from a size_t).
#include <stdio.h>
#include <stddef.h>
#include <initializer_list>

struct Unit { int v { 0 }; Unit() = default; Unit(int x) : v(x) { } };
struct Adjustment { Unit offset { 0 }; bool first { false }; };

template<typename T> struct Vec {
    size_t n;
    bool fromList;
    explicit Vec(size_t size) : n(size), fromList(false) { }
    Vec(std::initializer_list<T> list) : n(list.size()), fromList(true) { }
};

int main()
{
    size_t count = 5;
    Vec<Adjustment> a { count };
    Vec<Unit> b { 7 };
    Vec<int> c { 3 };
    Vec<double> d { 1.5, 2.5 };
    Vec<Adjustment> e { Adjustment { }, Adjustment { } };
    printf("%zu %d\n", a.n, a.fromList);
    printf("%zu %d\n", b.n, b.fromList);
    printf("%zu %d\n", c.n, c.fromList);
    printf("%zu %d\n", d.n, d.fromList);
    printf("%zu %d\n", e.n, e.fromList);
    return 0;
}
