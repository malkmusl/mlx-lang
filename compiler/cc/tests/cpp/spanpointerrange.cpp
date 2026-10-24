// A std::span is made from a range with no data() whose iterators are
// pointers (contiguous) and that has size(): WTF's FixedVector passed as
// a std::span<const AtomString> (WebCore's DOMTokenList::add).
#include <span>
#include <stdio.h>
#include <vector>

template<class T> struct Fixed {
    T *items;
    size_t count;
    T *begin() { return items; }
    T *end() { return items + count; }
    const T *begin() const { return items; }
    const T *end() const { return items + count; }
    size_t size() const { return count; }
};

static int sum(std::span<const int> s)
{
    int t = 0;
    for (int x : s)
        t += x;
    return t;
}

int main(int argc, char **)
{
    int raw[] = { 1, 2, 3, 4 };
    const Fixed<int> f { raw, 3 };
    Fixed<int> g { raw + 1, 3 };
    std::vector<int> v { 5, 6 };
    std::span<int> mutableSpan = g;
    mutableSpan[0] = 20;
    printf("%d %d %d %d\n", sum(f), sum(g), sum(v), raw[1]);
    return argc;
}
