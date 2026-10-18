// `span = {pointer, count}` for a span of a scoped enumeration: the
// initializer list constructor does not apply since a pointer does not
// convert to the enumeration (Skia's SkPathData: SkSpan<SkPathVerb>).
#include <initializer_list>
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <iterator>
#include <vector>

enum class Verb : uint8_t { Move, Line };

template <typename T>
class Span {
public:
    constexpr Span() : fPtr{nullptr}, fSize{0} {}
    template <typename Integer, std::enable_if_t<std::is_integral_v<Integer>, bool> = true>
    constexpr Span(T *ptr, Integer size) : fPtr{ptr}, fSize{(size_t)size} {}
    template <typename U, typename = std::enable_if_t<std::is_same_v<const U, T>>>
    constexpr Span(const Span<U> &that) : fPtr(that.fPtr), fSize(that.fSize) {}
    constexpr Span(const Span &o) = default;
    template <size_t N> constexpr Span(T (&a)[N]) : Span(a, N) {}
    template <typename Container>
    constexpr Span(Container &&c) : Span(std::data(c), std::size(c)) {}
    Span(std::initializer_list<T> il) : Span(std::data(il), std::size(il)) {}
    constexpr Span &operator=(const Span &that) = default;
    size_t size() const { return fSize; }
    T *fPtr;
    size_t fSize;
};

struct Data {
    Span<float> fConics;
    Span<Verb> fVerbs;
    void init(std::byte *data, size_t ncns, size_t nvbs) {
        fConics = {reinterpret_cast<float *>(data), ncns};
        fVerbs = {reinterpret_cast<Verb *>(data), nvbs};
    }
};

int main(int argc, char **argv) {
    alignas(8) std::byte storage[16] = {};
    Data d;
    d.init(storage, 2, 3);
    Span<Verb> local = {reinterpret_cast<Verb *>(storage), 4};
    printf("%zu %zu %zu %d\n", d.fConics.size(), d.fVerbs.size(), local.size(), (int)d.fVerbs.fPtr[0]);
    // (A list of elements still takes the initializer_list.)
    std::vector<Verb> verbs;
    verbs = {Verb::Line, Verb::Move, Verb::Line};
    std::vector<int> numbers;
    numbers = {4, 5};
    printf("%zu %d %zu %d\n", verbs.size(), (int)verbs[2], numbers.size(), numbers[1]);
    return argc;
}
