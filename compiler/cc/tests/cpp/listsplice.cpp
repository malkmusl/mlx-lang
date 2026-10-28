// splice of an element to where it is (an LRU list moving its front to
// the front), before its successor, and of a range to its own end: no
// change, no cycle.
#include <stdio.h>
#include <list>
int main() {
    std::list<int> l = { 1, 2, 3, 4 };
    auto show = [&] (const char *what) { printf("%s:", what); for (int x : l) printf(" %d", x); printf(" (size %zu)\n", l.size()); };
    auto it = l.begin(); std::advance(it, 2);
    l.splice(l.begin(), l, it); show("3 to front");
    l.splice(l.begin(), l, l.begin()); show("front to front");
    it = l.begin(); std::advance(it, 1);
    l.splice(it, l, it); show("self");
    it = l.begin(); std::advance(it, 1);
    l.splice(std::next(it), l, it); show("before next");
    l.erase(l.begin()); show("erase front");
    it = l.begin(); std::advance(it, 1);
    l.splice(l.end(), l, it, l.end()); show("range to end");
    std::list<int> o = { 7, 8 };
    l.splice(l.begin(), o, o.begin()); show("from other");
    return 0;
}
