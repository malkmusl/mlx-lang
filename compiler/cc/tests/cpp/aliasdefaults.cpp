// An alias template named in a pattern (a member template's signature)
// with a parameter left to its default that names the parameters before
// it, as WTF::StdMap's `FastAllocator<std::pair<const Key, Value>>`:
// those are replaced by the arguments given too ("expected a type as the
// template argument").
#include <stdio.h>
#include <utility>

template<typename A, typename B, typename C, typename D> struct Map {
    A key;
    B value;
    static constexpr int compareSize = sizeof(C);
    static constexpr int allocatorSize = sizeof(D);
};
template<typename T> struct Allocator { T sample; };
template<typename T> struct Less { char pad[2]; };
template<typename Key, typename Value, typename Compare = Less<Key>, typename Alloc = Allocator<std::pair<const Key, Value>>>
using StdMap = Map<Key, Value, Compare, Alloc>;

enum class Variant { A, B };
struct Algorithm {
    using SpanLength = unsigned;
    struct Size { long a; long b; };
    template<Variant variant> int increase(StdMap<SpanLength, Size> &map) { return variant == Variant::A ? map.value.a : map.value.b; }
    template<Variant variant> int sizes(const StdMap<char, Size, Less<long>> &map) { return map.compareSize * 1000 + map.allocatorSize + (int)variant; }
};

int main(int argc, char **argv)
{
    Algorithm algorithm;
    StdMap<unsigned, Algorithm::Size> map { 1, { 20, 30 } };
    StdMap<char, Algorithm::Size, Less<long>> other { 'x', { 1, 2 } };
    printf("%d %d\n", algorithm.increase<Variant::A>(map), algorithm.increase<Variant::B>(map));
    printf("%d %d\n", algorithm.sizes<Variant::B>(other), (int)sizeof(map));
    return argc;
}
