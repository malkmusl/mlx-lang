// decltype of a member access names the member's declared type, not a
// reference (Skia's `decltype(result->fPaletteEntryOverrides[0].index)`
// for SkTFitsIn).
#include <cstdio>
#include <cstdint>
#include <type_traits>

struct Override { uint16_t index; uint32_t color; };
template <typename T> struct AutoMalloc {
    T *fPtr;
    T &operator[](int i) { return fPtr[i]; }
    T *get() const { return fPtr; }
};
struct Descriptor { AutoMalloc<Override> fOverrides; Override fFirst; };

template <typename D, typename S>
constexpr typename std::enable_if<std::is_integral<S>::value && std::is_integral<D>::value, bool>::type fitsIn(S src) {
    return (S)(D)src == src;
}

int main(int argc, char **argv) {
    Override storage[2] = {{3, 4}, {5, 6}};
    Descriptor d{{storage}, {7, 8}};
    Descriptor *result = &d;
    using IndexType = decltype(result->fOverrides[0].index);
    using FirstType = decltype(result->fFirst.index);
    using Parens = decltype((result->fFirst.index));
    static_assert(std::is_same_v<IndexType, uint16_t>);
    static_assert(std::is_integral<IndexType>::value);
    static_assert(std::is_same_v<FirstType, uint16_t>);
    static_assert(std::is_same_v<Parens, uint16_t &>);
    size_t wide = 70000;
    printf("%d %d %d\n", fitsIn<IndexType>((size_t)12), fitsIn<IndexType>(wide), (int)sizeof(IndexType));
    return argc;
}
