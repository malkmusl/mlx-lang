// Const arrays: a const array is an array of const elements (`const A &`
// of `A = S[2]` deduces `const T (&)[N]`), and a local `static
// constexpr` array sized by its initializer keeps that size.
#include <cstdio>
#include <iterator>

struct S { int a; };
template <typename A> size_t count(const A &arr) { return std::size(arr); }
template <typename T, size_t N> int first(const T (&arr)[N]) { return (int)arr[0] + (int)N; }

int main(int argc, char **argv) {
    constexpr float a[] = {1, 2, 3};
    static const float b[] = {1, 2, 3, 4};
    static constexpr float c[] = {1, 2, 3, 4, 5};
    printf("%zu %zu %zu\n", sizeof(a), sizeof(b), sizeof(c));
    printf("%zu %zu %zu\n", std::size(a), std::size(b), std::size(c));
    const S x[3] = {{1}, {2}, {3}};
    S y[2] = {{1}, {2}};
    printf("%zu %zu %zu %zu\n", std::size(x), std::size(y), count(x), count(y));
    printf("%d %d\n", first(c), first(b));
    return argc;
}
