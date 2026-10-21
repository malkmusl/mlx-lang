// The standard concepts after <functional> or <iterator> alone, as other
// libraries have them (Skia's GlyphVector.h names std::derived_from
// including <functional> and <optional>).
#include <functional>
#include <iterator>
#include <cstdio>

struct Base {};
struct Derived : Base {};

template <typename T>
concept Strike = std::derived_from<T, Base> && std::copy_constructible<T>;

template <Strike T>
int pick(const T &) { return 1; }

int pick(...) { return 0; }

int main(int argc, char **argv) {
    Derived d;
    printf("%d %d %d\n", pick(d), pick(3), std::same_as<int, int>);
    return argc;
}
