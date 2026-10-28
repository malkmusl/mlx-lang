// std::ranges' projections through pointer elements, as std::invoke has
// them: a member function or data member of what each pointer points to
// (B3's Air sorts its StackSlot *s by `&StackSlot::offsetFromFP` and
// inserts with ranges::upper_bound: mlxlibcpp read the pointers' own
// bytes, the slots came out unsorted, and spill slots were given the
// callee-save registers' places).
#include <algorithm>
#include <functional>
#include <stdio.h>
#include <vector>
struct Slot { long off; long offsetFromFP() const { return off; } };
struct Data { long key; };
int main()
{
    Slot a { -8 }, b { -40 }, c { -16 }, d { -24 };
    std::vector<Slot*> v { &b, &a, &d, &c };
    std::ranges::sort(v, std::ranges::greater { }, &Slot::offsetFromFP);
    for (auto* s : v) printf("%ld ", s->off);
    Slot e { -20 };
    auto it = std::ranges::upper_bound(v, e.offsetFromFP(), std::ranges::greater { }, &Slot::offsetFromFP);
    printf("| insert at %ld\n", (long)(it - v.begin()));
    Data x { 3 }, y { 1 }, z { 2 };
    std::vector<Data*> w { &x, &y, &z };
    std::ranges::sort(w, std::ranges::less { }, &Data::key);
    printf("%ld %ld %ld\n", w[0]->key, w[1]->key, w[2]->key);
    return 0;
}
