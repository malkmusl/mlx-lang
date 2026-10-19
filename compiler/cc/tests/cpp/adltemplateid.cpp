// `get<I>(x)` where no template `get` is visible: the function template
// the arguments' namespaces hold (C++20's argument-dependent lookup of a
// template-id; WTF's tuple_zip calls `get<I>` of WebCore::Style's
// tuple-like Position, "undeclared identifier 'get'").
#include <stdio.h>

namespace Style {
struct Position { int x; int y; };
template<unsigned I> int get(const Position &p) { return I == 0 ? p.x : p.y; }
enum class Axis { X, Y };
template<Axis A> const char *name(Axis) { return A == Axis::X ? "x" : "y"; }
}

namespace Tools {
template<unsigned I, class T> int pick(const T &t) { return get<I>(t) * 10; }
}

int main(int argc, char **)
{
    Style::Position p { 3, 4 };
    printf("%d %d\n", get<0>(p), get<1>(p));
    printf("%d %s\n", Tools::pick<1>(p), name<Style::Axis::Y>(Style::Axis::X));
    return argc;
}
