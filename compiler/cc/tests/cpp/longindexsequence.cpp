// make_index_sequence<511> (WTF's enumNames<E>() of a uint16_t enum): the
// halves joined, instances nested log N deep; and its braced list of 511
// elements.
#include <stdio.h>
#include <utility>
#include <array>
template<size_t... Is> constexpr std::array<int, sizeof...(Is)> squares(std::index_sequence<Is...>) { return { int(Is * Is)... }; }
template<class T, T... Is> constexpr T sum(std::integer_sequence<T, Is...>) { return (T(0) + ... + Is); }
template<class... Ts> constexpr size_t count(Ts...) { return std::index_sequence_for<Ts...>::size(); }
int main()
{
    constexpr auto table = squares(std::make_index_sequence<511> { });
    static_assert(table.size() == 511 && table[510] == 510 * 510);
    printf("%d %d %d %ld %d %d\n", table[0], table[7], table[510], (long)sum(std::make_integer_sequence<long, 200> { }), (int)sum(std::make_integer_sequence<unsigned char, 0> { }), (int)count(1, 'a', 2.0));
    return 0;
}
