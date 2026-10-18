// Double braces for a span-like class taking an initializer_list
// (Skia's `swizzle(1, {{0, 0, 0, 0}})` of an SkSpan<const int8_t>): the
// inner braces are the list.
#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <initializer_list>

template <typename T> class Span {
public:
    Span(T *p, size_t n) : fPtr(p), fSize(n) {}
    Span(std::initializer_list<T> il) : fPtr(il.begin()), fSize(il.size()) {}
    size_t size() const { return fSize; }
    T *data() const { return fPtr; }
private:
    T *fPtr;
    size_t fSize;
};

struct Type { int id; };

static int swizzle(int consumed, Span<const int8_t> components) {
    int sum = consumed;
    for (size_t i = 0; i < components.size(); i++) sum = sum * 10 + components.data()[i];
    return sum;
}
static size_t generic(const char *name, Span<const Type *const> types) { return types.size(); }

int main(int argc, char **argv) {
    Type a{1}, b{2}, c{3};
    printf("%d %d %zu\n", swizzle(1, {{0, 1, 2, 3}}), swizzle(3, {{2, 0, 1}}), generic("$gen", {{&a, &b, &c}}));
    return argc;
}
