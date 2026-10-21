// A constructor template taking `std::initializer_list<T *>` for braces
// of pointers all of one type, the `...` constructor for braces of
// several types: Brigand's all_same, which tells whether a predicate
// gave one type for every element (WebCore's union conversions ask
// brigand::any whether a union holds undefined or null).
#include <stdio.h>
#include <initializer_list>
#include <type_traits>

struct AllSame {
    const bool value = false;
    constexpr AllSame(...) { }
    template<typename T> constexpr AllSame(std::initializer_list<T *>) : value { true } { }
};

template<class Predicate, class... Ts> struct None {
    static constexpr AllSame tester { static_cast<typename Predicate::template apply<Ts> *>(nullptr)... };
    static constexpr bool value = tester.value;
};
struct IsInt { template<class T> using apply = std::bool_constant<std::is_same_v<T, int>>; };

struct Counted {
    int count = 0;
    template<typename T> Counted(std::initializer_list<T> list) : count((int)list.size()) { }
    Counted(int a, double b) : count(a + (int)b) { }
};

int main(int argc, char **argv)
{
    constexpr AllSame same { static_cast<std::false_type *>(nullptr), static_cast<std::false_type *>(nullptr) };
    constexpr AllSame differ { static_cast<std::false_type *>(nullptr), static_cast<std::true_type *>(nullptr) };
    AllSame atRunTime { (int *)nullptr, (int *)nullptr, (int *)nullptr };
    static_assert(same.value && !differ.value);
    printf("%d %d %d\n", (int)same.value, (int)differ.value, (int)atRunTime.value);
    printf("%d %d %d\n", (int)None<IsInt, char, long>::value, (int)None<IsInt, char, int>::value, (int)None<IsInt, int, int>::value);
    Counted three { argc, argc, argc };
    Counted mixed { argc, 2.5 };
    printf("%d %d\n", three.count, mixed.count);
    return argc - 2;
}
