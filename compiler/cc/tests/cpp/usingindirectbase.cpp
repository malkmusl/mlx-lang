// A using-declaration naming a base of a base (WebCore's Element:
// `using Node::computedStyle;`, its base ContainerNode) brings that
// base's overloads beside the class's own ("too few arguments"), each
// called on its own base (here at an offset in the object).
#include <stdio.h>

struct Other { long data[3] = { 7, 8, 9 }; };
struct Node {
    int base = 4;
    virtual ~Node() { }
    int style() { return style(5) + base; }
    virtual int style(int x) { return x; }
    long style(long x, long y) { return x + y + base; }
};
struct Container : Other, Node { int pad = 0; };
struct Element : Container {
    using Node::style;
    int style(int x) override { return x * 10; }
};

int main(int argc, char **argv)
{
    Element e;
    Element *p = &e;
    printf("%d %d %ld\n", p->style(), p->style(2), p->style(1L, 2L));
    return argc;
}
