// A braced list whose elements do not convert to the initializer_list
// constructor's element type takes the other constructors (Skia's
// `setDataFromBytes({storage->data(), storage->size()})` of an
// SkSpan<std::byte>).
#include <cstdio>
#include <cstddef>
#include <type_traits>
#include <initializer_list>

template <typename T> class Span {
public:
    template <typename Integer, std::enable_if_t<std::is_integral_v<Integer>, bool> = true>
    Span(T *ptr, Integer size) : fPtr(ptr), fSize((size_t)size), fFromList(false) {}
    Span(std::initializer_list<T> il) : fPtr(il.begin()), fSize(il.size()), fFromList(true) {}
    size_t size() const { return fSize; }
    bool fromList() const { return fFromList; }
private:
    const T *fPtr;
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
static void takeInts(Span<int> s) { printf("%zu %d\n", s.size(), s.fromList()); }

int main(int argc, char **argv) {
    Storage storage;
    take({storage.data(), storage.size()});
    take({Byte{1}, Byte{2}, Byte{3}});
    int values[4] = {1, 2, 3, 4};
    takeInts({values, 4});
    takeInts({5, 6});
    return argc;
}
