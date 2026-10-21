// std::tuple takes no room past its elements (no empty tuple<> after the
// last one): WTF's HashTable asserts a KeyValuePair of a tuple key and a
// Vector<size_t, 8> is at most 150 bytes (WebCore's text breaking cache).
// One-element tuples still work as the others do.
#include <cstdio>
#include <string>
#include <tuple>
#include <utility>
struct B { char c[24]; long x; };
struct A { long a; };
int main() {
    std::printf("%zu %zu %zu %zu %zu\n", sizeof(std::tuple<int>), sizeof(std::tuple<int, int, int>), sizeof(std::tuple<long, B, A>),
        sizeof(std::tuple<char, long>), sizeof(std::tuple<long, long, long, long>));
    std::tuple<std::string> one { "one" };
    std::tuple<std::string> copy = one;
    std::tuple<int> converted = std::tuple<short>(7);
    std::tuple<std::string> other { "other" };
    one.swap(other);
    auto joined = std::tuple_cat(std::make_tuple(1), std::tuple<std::string>("two"), std::make_tuple(3.5));
    int x = 0;
    std::tie(x) = std::make_tuple(42);
    std::printf("%s %s %s %d %s %g %d %d %d\n", std::get<0>(one).c_str(), std::get<0>(copy).c_str(), std::get<0>(other).c_str(),
        std::get<0>(converted), std::get<1>(joined).c_str(), std::get<2>(joined), x, copy == std::tuple<std::string>("one"), std::tuple<int>(1) < std::tuple<int>(2));
    return 0;
}
