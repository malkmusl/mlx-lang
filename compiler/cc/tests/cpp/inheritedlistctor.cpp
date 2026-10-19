// Braces of a class whose constructors are its base's (`using
// Base::Base;`) choose the base's initializer_list constructor:
// `return { A, C };` of WebCore's EnumSetOrKeywordBase's classes (no
// constructor matched), and a list of two ints made no (size, value)
// pair.
#include <stdio.h>
#include <initializer_list>
#include <utility>

enum class V { A = 1, B = 2, C = 4 };
template<typename T> struct Set {
    unsigned bits = 0;
    constexpr Set() = default;
    constexpr Set(std::initializer_list<T> list) { for (auto v : list) bits |= unsigned(v); }
};
struct None { };

template<typename Derived, typename T, typename K> struct SetOrKeyword {
    using EnumSet = T;
    constexpr SetOrKeyword(EnumSet &&list) : m_value { std::move(list) } { }
    constexpr SetOrKeyword(K) : m_value { } { }
    constexpr SetOrKeyword(V value) : SetOrKeyword { EnumSet { value } } { }
    constexpr SetOrKeyword(std::initializer_list<V> list) : SetOrKeyword { EnumSet { list } } { }
    EnumSet m_value;
};
struct Contain : SetOrKeyword<Contain, Set<V>, None> {
    using SetOrKeyword::SetOrKeyword;
};

struct Ints {
    int count = 0;
    int sum = 0;
    Ints(int size, int value) : count(size), sum(size * value) { }
    Ints(std::initializer_list<int> list) { for (int v : list) { count++; sum += v; } }
};
struct MoreInts : Ints { using Ints::Ints; };

constexpr Contain initial() { return { V::A, V::C }; }
constexpr Contain one() { return { V::B }; }
constexpr Contain none() { return None { }; }

int main(int argc, char **argv)
{
    printf("%u %u %u\n", initial().m_value.bits, one().m_value.bits, none().m_value.bits);
    Contain c { V::A, V::B, V::C };
    MoreInts listed { 3, 4 };
    MoreInts sized(3, 4);
    printf("%u %d %d %d %d\n", c.m_value.bits, listed.count, listed.sum, sized.count, sized.sum);
    return argc;
}
