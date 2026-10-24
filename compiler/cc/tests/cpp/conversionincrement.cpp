// ++ and -- of a class with no operator++ but a conversion to a
// reference (`operator unsigned &()`): the referenced object incremented,
// as WebCore's GridSpan iterator is in a range-for (`for (auto row :
// area.rows)`; the loop ran past the span and Grid::insert indexed past
// its rows).
#include <stdio.h>

struct Span {
    struct Iterator {
        Iterator(unsigned v) : value(v) { }
        operator unsigned &() { return value; }
        unsigned operator*() const { return value; }
        unsigned value;
    };
    Iterator begin() const { return start; }
    Iterator end() const { return stop; }
    int start, stop;
};

int main()
{
    Span span { 1, 4 };
    int turns = 0;
    for (auto line : span) {
        printf("%u ", line);
        if (++turns > 10)
            break;
    }
    Span::Iterator it(5);
    ++it;
    it++;
    --it;
    unsigned before = it--;
    printf("\n%u %u %d\n", it.value, before, (int)(it != Span::Iterator(7)));
    return 0;
}
