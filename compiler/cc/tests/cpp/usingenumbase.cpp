// `using enum E;` of a base class's member enum, in a member function of
// a derived class defined outside it (WebCore's HTMLPlugInElement, with
// Node::TypeFlag).
#include <cstdio>
namespace N {
struct Node {
    enum class TypeFlag : unsigned short { A = 1, B = 2, C = 4 };
    static constexpr unsigned flags(TypeFlag a, TypeFlag b) { return unsigned(a) | unsigned(b); }
};
struct Element : Node { };
struct Plugin : Element {
    static constexpr unsigned pluginFlags();
    unsigned one() const;
};
constexpr unsigned Plugin::pluginFlags()
{
    using enum TypeFlag;
    return flags(B, C);
}
unsigned Plugin::one() const
{
    using enum TypeFlag;
    return unsigned(A);
}
}
int main() {
    static_assert(N::Plugin::pluginFlags() == 6);
    std::printf("%u %u\n", N::Plugin::pluginFlags(), N::Plugin().one());
    return 0;
}
