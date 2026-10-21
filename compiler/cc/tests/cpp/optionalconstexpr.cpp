// std::optional made and read in constant expressions (a static
// constexpr member's operator-> in a static_assert, WebCore's
// JSNodeCustom.h), and its values made and unmade once each.
#include <cstdio>
#include <cstdint>
#include <limits>
#include <optional>

static int made, unmade;
struct Counted {
    int v;
    Counted(int x) : v(x) { made++; }
    Counted(const Counted& o) : v(o.v) { made++; }
    ~Counted() { unmade++; }
};

enum JSType : uint8_t { First = 0, NodeType = 240 };
struct Range { JSType first; JSType last; };

template<typename T> struct Traits;
struct Node;
template<> struct Traits<Node> {
    static constexpr std::optional<Range> typeRange { { static_cast<JSType>(NodeType), static_cast<JSType>(NodeType + 15) } };
    static_assert(std::numeric_limits<uint8_t>::max() == typeRange->last);
    static_assert(typeRange->first == NodeType);
    static_assert((*typeRange).first == NodeType);
};

int main(int argc, char** argv)
{
    std::printf("%d %d\n", (int)Traits<Node>::typeRange->first, (int)Traits<Node>::typeRange->last);
    {
        std::optional<Counted> a { Counted { argc } };
        std::optional<Counted> b = a;
        std::optional<Counted> c;
        c.emplace(7);
        b.reset();
        std::printf("%d %d %d %d\n", a->v, (int)b.has_value(), c->v, made - unmade);
    }
    std::printf("%d %d\n", made, unmade);
    return 0;
}
