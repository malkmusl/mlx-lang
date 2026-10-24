// A call naming its template arguments explicitly, among templates of
// the name of which the first has a default for a parameter left out:
// each template takes its own defaults (Skia's SubRunAllocator
// makePODArray<T, Alignment = alignof(T)>(int) beside
// makePODArray<T, Src, Map>(const Src &, Map)).
#include <cstdio>
#include <cstddef>
#include <new>
#include <type_traits>
#include <span>
#include <vector>

class Drawable;
union IDOrDrawable {
    unsigned short fGlyphID;
    Drawable *fDrawable;
};

class Alloc {
public:
    template <typename T>
    inline static constexpr bool HasNoDestructor = std::is_trivially_destructible<T>::value;

    template <typename T, size_t Alignment = alignof(T)> T *makePODArray(int n)
        requires (sizeof(T) % Alignment == 0) {
        static_assert(HasNoDestructor<T>, "This is not POD.");
        return reinterpret_cast<T *>(new char[sizeof(T) * n]);
    }

    template <typename T, typename Src, typename Map>
    std::span<T> makePODArray(const Src &src, Map map) {
        static_assert(HasNoDestructor<T>, "This is not POD.");
        int size = (int)src.size();
        T *result = this->template makePODArray<T>(size);
        for (int i = 0; i < size; i++) new (&result[i]) T(map(src[i]));
        return {result, src.size()};
    }
};

template <typename T, typename U = int> U convert(T value) { return (U)value; }
template <typename T, typename A, typename B> T convert(A a, B b) { return (T)(a + b); }

template <typename T> size_t count(const T &values) { return values.size(); }

static std::span<const unsigned short> ids(std::span<const unsigned short> s) { return s; }

int main(int argc, char **argv) {
    Alloc a;
    Alloc *alloc = &a;
    unsigned short glyphs[3] = {4, 5, 6};
    auto map = [](const unsigned short id) { return IDOrDrawable{id}; };
    auto made = alloc->makePODArray<IDOrDrawable>(ids(glyphs), map);
    printf("%zu %d\n", made.size(), made[2].fGlyphID);
    printf("%d %.1f %zu\n", convert<double>(3.75), convert<double>(1, 0.5), count<std::vector<int>>({1, 2, 3}));
    return argc;
}
