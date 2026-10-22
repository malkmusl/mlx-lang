// A class template's instance named through a reference member while its
// argument is only declared is made later, once the argument is a whole
// class (WebCore's BorderPainter.h `const BorderEdges &edges` before
// BorderEdge's definition made RectEdges<BorderEdge> of no size).
#include <stdio.h>
#include <array>
#include <utility>
class Edge;
template<typename T> class Edges {
public:
    Edges(T&& a, T&& b, T&& c, T&& d) : m_sides({ { std::move(a), std::move(b), std::move(c), std::move(d) } }) { }
    const T& top() const { return m_sides[0]; }
    std::array<T, 4> m_sides;
};
using AllEdges = Edges<Edge>;
struct Painting { const AllEdges& edges; };
class Edge {
public:
    Edge(float width, long color) : width(width), color(color) { }
    Edge(const Edge& other) : width(other.width), color(other.color) { }
    float width;
    long color;
};
static AllEdges makeEdges(long color, float width)
{
    return { Edge { width, color }, Edge { width + 1, color }, Edge { width + 2, color }, Edge { width + 3, color } };
}
int main(int argc, char **)
{
    AllEdges edges = makeEdges(0x301, 2.5f);
    Painting painting { edges };
    printf("%zu %g %lx %g\n", sizeof(AllEdges), painting.edges.top().width, painting.edges.top().color, painting.edges.m_sides[3].width);
    return argc == 2 ? 0 : 1;
}
