// An optional made from an optional of another type holds that one's
// value converted (`std::optional<double> d = optionalUnsigned;`), not
// its `explicit operator bool` as a number: a number is made directly
// from a class only by a conversion to that number's type when the
// conversion is explicit (std::is_constructible).
#include <optional>
#include <type_traits>
#include <stdio.h>

struct Flag {
    explicit operator bool() const { return true; }
};

struct Count {
    explicit operator int() const { return 3; }
    operator long() const { return 4; }
};

int main(int argc, char **)
{
    std::optional<unsigned> u = 5u;
    std::optional<double> d = u;
    std::optional<double> e(u);
    std::optional<long> f = std::optional<int>(7);
    std::optional<unsigned> none;
    std::optional<double> g = none;
    printf("%g %g %ld %d\n", *d, *e, *f, (int)g.has_value());
    printf("%d %d %d %d\n", (int)std::is_constructible<double, std::optional<unsigned> &>::value,
        (int)std::is_constructible<bool, std::optional<unsigned> &>::value,
        (int)std::is_constructible<bool, Flag>::value, (int)std::is_constructible<int, Flag>::value);
    printf("%d %d %d\n", (int)std::is_constructible<int, Count>::value,
        (int)std::is_constructible<long, Count>::value, (int)std::is_constructible<double, Count>::value);
    return argc;
}
