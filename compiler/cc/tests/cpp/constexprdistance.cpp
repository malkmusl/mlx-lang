// The distance of two pointers in a constexpr function template, whose
// instance is defined where it is called: the elements' class, an
// instance of a template named only by an alias so far, is made for its
// size. (A constexpr std::sort's `last - first` over WTF::Vector's
// std::tuple<JSValue, String> counted bytes, and JavaScriptCore's
// Array.prototype.sort lost its elements.)
#include <stdio.h>

template <typename T> struct Box { T a; T b; T c; };
using LongBox = Box<long>;

template <typename I> constexpr long distance(I first, I last) { return last - first; }

int main(int argc, char **argv)
{
    static long storage[9];
    LongBox *first = (LongBox *)storage;
    LongBox *last = (LongBox *)(storage + 9);
    printf("%ld\n", distance(first, last));
    printf("%d\n", (int)sizeof(LongBox));
    return argc - 1;
}
