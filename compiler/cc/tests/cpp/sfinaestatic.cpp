// Detecting a traits class's static member function through
// `decltype(U::ShouldGrow(std::declval<int>(), std::declval<int>()))` in
// a partial specialization (Skia's SkTHash.h): a traits class without it
// takes the primary template.
#include <cstdio>
#include <type_traits>
#include <utility>

template <typename U, typename = void> struct HasShouldGrow : std::false_type {};
template <typename U>
struct HasShouldGrow<U, std::void_t<decltype(U::ShouldGrow(std::declval<int>(), std::declval<int>()))>> : std::true_type {};

struct Plain {
    static int Hash(int k) { return k; }
};
struct Growing {
    static bool ShouldGrow(int count, int capacity) { return count * 4 >= capacity * 3; }
};

template <typename Traits> bool shouldGrow(int count, int capacity) {
    if constexpr (HasShouldGrow<Traits>::value) {
        return Traits::ShouldGrow(count, capacity);
    } else {
        return count * 2 >= capacity;
    }
}

int main(int argc, char **argv) {
    printf("%d %d\n", HasShouldGrow<Plain>::value, HasShouldGrow<Growing>::value);
    printf("%d %d\n", shouldGrow<Plain>(5, 8), shouldGrow<Growing>(5, 8));
    return argc;
}
