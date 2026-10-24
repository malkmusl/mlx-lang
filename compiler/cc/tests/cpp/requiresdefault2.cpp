// A class whose only constructor template is constrained is not default
// constructible, so a class template's `Edges() requires
// (is_default_constructible_v<T>)` is no constructor of Edges<T>
// (RectEdges<Style::MarginEdge>).
#include <stdio.h>
#include <array>
#include <type_traits>
struct Keyword {
    template<typename U> requires std::same_as<std::remove_cvref_t<U>, int>
    Keyword(U&& v) : value(v) { }
    int value;
};
template<typename T> class Edges {
public:
    Edges() requires (std::is_default_constructible_v<T>)
        : m_sides { }
    {
    }
    Edges(T a, T b) : m_sides { a, b } { }
    T first() const { return m_sides[0]; }
private:
    std::array<T, 2> m_sides;
};
static_assert(!std::is_default_constructible_v<Keyword>);
static_assert(!std::is_default_constructible_v<Edges<Keyword>>);
struct Box {
    Edges<Keyword> edges { Keyword { 4 }, Keyword { 5 } };
};
int main(int argc, char** argv)
{
    Box box;
    Edges<int> ints;
    printf("%d %d\n", box.edges.first().value, ints.first());
    return argc - 1;
}
