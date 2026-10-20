// Class template argument deduction from a braced pair, `Span{ptr,
// count}` (Skia's SkSpan): the initializer_list constructor's guide
// deduces T from every element, so a pointer and a count deduce no
// initializer_list<const float *>, and the guide taking a pointer and a
// count makes a Span<const float>.
#include <cstdio>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <type_traits>

template<typename T> class Span {
public:
    constexpr Span() : p(nullptr), n(0) {}
    template<typename Integer, std::enable_if_t<std::is_integral_v<Integer>, bool> = true>
    constexpr Span(T* ptr, Integer size) : p(ptr), n((size_t)size) {}
    template<typename U, typename = std::enable_if_t<std::is_same_v<const U, T>>>
    constexpr Span(const Span<U>& that) : p(that.data()), n(that.size()) {}
    constexpr Span(const Span& o) = default;
    template<size_t N> constexpr Span(T (&a)[N]) : Span(a, N) {}
    template<typename Container> constexpr Span(Container&& c) : Span(std::data(c), std::size(c)) {}
    Span(std::initializer_list<T> il) : Span(std::data(il), std::size(il)) {}
    constexpr T* data() const { return p; }
    constexpr size_t size() const { return n; }
private:
    T* p;
    size_t n;
};

static Span<const float> make(const float* floats, size_t bytes)
{
    return Span{floats, bytes / sizeof(float)};
}

int main()
{
    float values[4] = { 1, 2, 3, 4 };
    auto s = make(values, sizeof(values));
    Span t { static_cast<const float*>(values), 2 };
    int ints[3] = { 7, 8, 9 };
    Span v = Span<const int>({ ints[0], ints[2] });
    const int* ip = ints;
    Span w { ip, 3U };
    printf("%zu %g %zu %g %zu %d %zu\n", s.size(), s.data()[3], t.size(), t.data()[1], v.size(), v.data()[1], w.size());
    return 1;
}
