// A const object of a derived class binds no non-const reference to its
// base: ancestorsOfType<T>(const Element &) takes WebCore's
// ancestorsOfType(const Node &) overload, not ancestorsOfType(Node &).
#include <cstdio>
struct Node { int id; };
struct Element : Node { };
template<typename T> struct Range { T* first; };
template<typename T> Range<T> ancestorsOfType(Node& d);
template<typename T> Range<const T> ancestorsOfType(const Node& d);
template<typename T> Range<T> ancestorsOfType(Node& d) { return Range<T> { nullptr }; }
template<typename T> Range<const T> ancestorsOfType(const Node& d) { return Range<const T> { static_cast<const T*>(&d) }; }
template<typename T> Range<const T> lineage(const Element& first)
{
    return ancestorsOfType<T>(first);
}
int main() {
    Element e;
    e.id = 3;
    printf("%d\n", lineage<Element>(e).first->id);
}
