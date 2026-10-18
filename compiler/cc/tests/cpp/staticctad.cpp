// A static constexpr data member whose class template's arguments are
// deduced from its braced initializer (`static constexpr OptionSet
// defaultTypes = { Type::A, Type::C };` in WebCore's HitTestRequest).
#include <cstdio>
#include <initializer_list>

template<typename E> struct OptionSet {
    unsigned bits { 0 };
    constexpr OptionSet() = default;
    constexpr OptionSet(E e) : bits(1u << static_cast<unsigned>(e)) { }
    constexpr OptionSet(std::initializer_list<E> list)
    {
        for (E e : list)
            bits |= 1u << static_cast<unsigned>(e);
    }
    constexpr bool contains(E e) const { return bits & (1u << static_cast<unsigned>(e)); }
};

struct Request {
    enum class Type { A, B, C };
    static constexpr OptionSet defaultTypes = { Type::A, Type::C };
    static constexpr OptionSet single = Type::B;
    OptionSet<Type> types;
    Request(OptionSet<Type> t = defaultTypes) : types(t) { }
};

int main(int argc, char** argv)
{
    Request r;
    Request s { Request::single };
    std::printf("%u %u %d %d\n", r.types.bits, s.types.bits, (int)Request::defaultTypes.contains(Request::Type::C), (int)r.types.contains(Request::Type::B));
    return 0;
}
