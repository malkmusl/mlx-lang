// Brace elision into an aggregate member of class type: the elements
// that do not make the member itself make its members (Style's
// GridTrackSize::MinMax { CSS::Keyword::Auto { }, CSS::Keyword::MaxContent
// { } }, a FunctionNotation whose one member `parameters` holds two
// GridTrackBreadths).
#include <cstdio>
struct Auto { };
struct MaxContent { };
struct Breadth {
    Breadth(Auto) : v(1) { }
    Breadth(MaxContent) : v(2) { }
    Breadth(int x) : v(x) { }
    int v;
};
struct Params { Breadth min; Breadth max; };
template<int C, typename T> struct Notation {
    T parameters;
    const T* operator->() const { return &parameters; }
};
using MinMax = Notation<5, Params>;
struct Point { int x; int y; };
struct Line { Point a; Point b; int width; };
struct Tagged { int tag; Point p; int rest = 9; };
int main() {
    MinMax a = MinMax { Auto { }, MaxContent { } };
    printf("%d %d\n", a->min.v, a->max.v);
    MinMax b { Breadth(7), 8 };
    printf("%d %d\n", b->min.v, b->max.v);
    Params p { 3, 4 };
    MinMax c { p };
    printf("%d %d\n", c->min.v, c->max.v);
    Line l { 1, 2, { 3, 4 }, 5 };
    printf("%d %d %d %d %d\n", l.a.x, l.a.y, l.b.x, l.b.y, l.width);
    Line m { Point { 6, 7 }, 8, 9 };
    printf("%d %d %d %d %d\n", m.a.x, m.a.y, m.b.x, m.b.y, m.width);
    Tagged t { 1, 2 };
    printf("%d %d %d %d\n", t.tag, t.p.x, t.p.y, t.rest);
}
