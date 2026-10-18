// Library pieces Skia uses: std::cbegin/cend, std::in_place_type for a
// variant, std::launder from <new>, std::align, std::ilogbf.
#include <cmath>
#include <cstdio>
#include <iterator>
#include <memory>
#include <new>
#include <variant>
#include <vector>

struct Malloc { void *data; };
struct Owned { int size; };

int main(int argc, char **argv) {
    std::vector<int> runs = {3, 1, 4};
    int sum = 0;
    for (auto it = std::cbegin(runs); it != std::cend(runs); ++it) sum += *it;
    const int fixed[3] = {5, 6, 7};
    int fixedSum = 0;
    for (auto it = std::cbegin(fixed); it != std::cend(fixed); ++it) fixedSum += *it;

    std::variant<Owned, Malloc> storage(std::in_place_type<Malloc>, Malloc{nullptr});
    std::variant<Owned, Malloc> other(std::in_place_index<0>, Owned{9});

    alignas(16) unsigned char buffer[64];
    int *placed = new (buffer) int(42);
    int *same = std::launder(reinterpret_cast<int *>(buffer));

    void *ptr = buffer + 1;
    size_t space = 40;
    void *aligned = std::align(16, 8, ptr, space);
    const size_t offset = (unsigned char *)aligned - buffer;
    void *tight = buffer + 1;
    size_t little = 4;
    void *none = std::align(16, 8, tight, little);

    printf("%d %d %zu %zu %d %d\n", sum, fixedSum, storage.index(), other.index(), *same, placed == same);
    printf("%zu %zu %d %d %d\n", offset, space, none == nullptr, std::ilogbf(8.5f), std::ilogb(0.25));
    return argc;
}
