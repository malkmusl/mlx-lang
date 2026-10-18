// A local array's size in a static_assert (`static_assert(std::size(iota)
// >= 16)`, as Skia's SkRasterPipeline_opts.h): the array's address
// passed by reference, never read.
#include <cstdio>
#include <cstdint>
#include <iterator>
template <typename T, size_t N> constexpr size_t countOf(const T (&)[N]) { return N; }
int main(int argc, char **argv) {
    uint32_t iota[] = {0, 1, 2, 3, 4, 5, 6, 7};
    static_assert(std::size(iota) >= 8);
    static_assert(countOf(iota) == 8);
    static const char *names[] = {"in", "out", "inout"};
    static_assert(std::size(names) == 3);
    iota[2] = (uint32_t)argc;
    printf("%u\n", iota[2]);
    return argc;
}
