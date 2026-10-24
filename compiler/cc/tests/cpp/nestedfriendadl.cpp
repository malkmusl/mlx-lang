// A nested class's hidden friend operator== found by argument-dependent
// lookup while the class around it has an operator== of its own (a
// member: no candidate for the nested class's operands), from a template
// in another namespace that has an operator== template of its own
// (WebCore's ScrollingStateNode.h: LayerRepresentation, whose defaulted
// operator== compares a Variant of its private GraphicsLayerData, which
// has a defaulted friend operator==; mpark's variant compares the
// alternatives with its equal_to, `lhs == rhs` in its namespace).
#include <stdio.h>

namespace lib {
template <typename L, typename R> auto equal(const L& l, const R& r) -> decltype(l == r) { return l == r; }
template <typename T> struct Box { T value; };
template <typename T> bool operator==(const Box<T>& a, const Box<T>& b) { return equal(a.value, b.value); }
template <typename T> int describe(const T&) { return 1; }
}

struct Outer {
    bool operator==(const Outer&) const = default;
    static int describe(const Outer&) { return 2; }
    struct Data {
        int id { 0 };
        friend bool operator==(const Data&, const Data&) = default;
    };
    lib::Box<Data> box;
};

// A function of the nested class's namespace, beside a static member of
// that name in the class around it.
int describe(const Outer::Data& data) { return 10 + data.id; }

int main()
{
    Outer a, b;
    printf("%d\n", a == b);
    b.box.value.id = 3;
    printf("%d\n", a == b);
    printf("%d\n", lib::equal(a.box.value, a.box.value));
    printf("%d\n", describe(b.box.value));
    return 0;
}
