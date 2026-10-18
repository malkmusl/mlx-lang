// A braced list whose elements do not convert to the initializer_list
// constructor's element type takes the other constructors (Skia's
// `setDataFromBytes({storage->data(), storage->size()})` of an
// SkSpan<std::byte>).
#include <cstdio>
#include <cstddef>
#include <type_traits>
#include <initializer_list>
#include <iterator>

template <typename T> class Span {
public:
    template <typename Integer, std::enable_if_t<std::is_integral_v<Integer>, bool> = true>
    Span(T *ptr, Integer size) : fPtr(ptr), fSize((size_t)size), fFromList(false) {}
    template <typename U, typename = std::enable_if_t<std::is_same_v<const U, T>>>
    Span(const Span<U> &that) : fPtr(that.data()), fSize(that.size()), fFromList(that.fromList()) {}
    template <size_t N> Span(T (&a)[N]) : Span(a, N) {}
    // (As SkSpan's: the list's elements by their pointer, which is a
    // `T *` only when T is const.)
    Span(std::initializer_list<T> il) : Span(std::data(il), std::size(il)) { fFromList = true; }
    T *data() const { return fPtr; }
    size_t size() const { return fSize; }
    bool fromList() const { return fFromList; }
private:
    T *fPtr;
    size_t fSize;
    bool fFromList;
};

enum class Byte : unsigned char {};

struct Storage {
    Byte bytes[16];
    Byte *data() { return bytes; }
    size_t size() const { return sizeof(bytes); }
};

static void take(Span<Byte> s) { printf("%zu %d\n", s.size(), s.fromList()); }
static void takeConst(Span<const Byte> s) { printf("%zu %d\n", s.size(), s.fromList()); }

struct Point { float x, y; };
struct Matrix {
    // (Overloads: the braces are weighed against both.)
    void map(Span<Point> dst, Span<const Point> src) const { printf("map %zu %zu %d\n", dst.size(), src.size(), dst.fromList()); }
    void map(Span<Point> pts) const { map(pts, pts); }
    void toQuad(Point dst[4]) const {
        Point src[4] = {{1, 2}, {3, 4}, {5, 6}, {7, 8}};
        this->map({dst, 4}, src);
    }
};
static void takeInts(Span<const int> s) { printf("%zu %d\n", s.size(), s.fromList()); }

int main(int argc, char **argv) {
    Storage storage;
    take({storage.data(), storage.size()});
    takeConst({Byte{1}, Byte{2}, Byte{3}});
    int values[4] = {1, 2, 3, 4};
    takeInts({values, 4});
    takeInts({5, 6});
    Point quad[4];
    Matrix().toQuad(quad);
    return argc;
}
