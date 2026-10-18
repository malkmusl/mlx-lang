// Class template argument deduction from a braced list: the guide of an
// initializer_list constructor takes the whole list first (WTF's
// `EnumSet { E::A, E::C }` is an EnumSet<E>), the other guides the
// elements when it does not fit, and one element of the template's own
// type is copied (`Set { other }`).
#include <cstdio>
#include <initializer_list>
#include <optional>

enum class Kind : unsigned char { A, B, C, D };

template<typename E> class Set {
public:
    constexpr Set() = default;
    constexpr Set(E e) : bits(1u << static_cast<unsigned>(e)) { }
    constexpr Set(std::initializer_list<E> list) { for (auto e : list) bits |= 1u << static_cast<unsigned>(e); }
    constexpr Set(std::optional<E> optional) : bits(optional ? 1u << static_cast<unsigned>(*optional) : 0) { }
    unsigned bits = 0;
};

template<typename T> struct Pair {
    Pair(T a, T b) : sum(a + b) { }
    T sum;
};

constexpr auto all = Set { Kind::A, Kind::C, Kind::D };
constexpr auto one = Set { Kind::B };

int main()
{
    auto two = Set { Kind::A, Kind::B };
    Set three(Kind::C);
    Set copy { two };
    Pair pair { 3, 4 };
    std::printf("%u %u %u %u %u %d\n", all.bits, one.bits, two.bits, three.bits, copy.bits, pair.sum);
    return static_cast<int>(all.bits);
}
