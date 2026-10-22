// A class inheriting its base's constructors (`using Base::Base;`) is
// default constructible only when its base (and each member) is: the
// implicit default constructor is deleted otherwise. (RectEdges<T>()
// requires is_default_constructible_v<T>, for Style's ScrollPaddingEdge
// a PrimitiveNumericOrKeyword with no default constructor.)
#include <cstdio>
#include <type_traits>
#include <array>
template<typename T> struct RectEdges {
    RectEdges() requires (std::is_default_constructible_v<T>) : m_sides { } { }
    RectEdges(const T& v) : m_sides { v, v, v, v } { }
    std::array<T, 4> m_sides;
};
struct NumOrKeyword {
    NumOrKeyword(int v) : v(v) { }
    int v;
};
struct Edge : NumOrKeyword { using NumOrKeyword::NumOrKeyword; };
struct Defaulted { int x = 3; };
struct Holder { Edge e; };
struct HolderInit { Edge e { 4 }; };
struct RefMember { int& r; };
int main() {
    printf("%d %d\n", std::is_default_constructible_v<Edge>, std::is_default_constructible_v<Defaulted>);
    printf("%d %d\n", std::is_default_constructible_v<RectEdges<Edge>>, std::is_default_constructible_v<RectEdges<int>>);
    printf("%d %d %d\n", std::is_default_constructible_v<Holder>, std::is_default_constructible_v<HolderInit>, std::is_default_constructible_v<RefMember>);
    RectEdges<Edge> r { Edge(7) };
    printf("%d\n", r.m_sides[3].v);
}
