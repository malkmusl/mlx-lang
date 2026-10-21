// `std::size(table)` of a constexpr std::array holding pointers (made
// when the program starts, so its bytes are not read at compile time)
// is a constant: `c.size()` in std::size reads nothing of `c`, only its
// address passes through `&*c`. WebCore's DOMException static_asserts so
// on its descriptions; reading the elements stays a run-time matter.
#include <array>
#include <iterator>
#include <stdio.h>

struct Literal {
    const char *p;
    constexpr Literal(const char *s) : p(s) { }
};
struct Description {
    Literal name;
    Literal message;
    int code;
};

static constexpr std::array descriptions {
    Description { "a", "first", 1 },
    Description { "b", "second", 3 },
    Description { "c", "third", 0 },
};
static_assert(std::size(descriptions) == 3);
static_assert(!std::empty(descriptions));

template<class C> constexpr int lastCode(const C &c) { return c[std::size(c) - 2].code; }

int main(int argc, char **)
{
    static const int code = lastCode(descriptions);
    char sized[std::size(descriptions)];
    printf("%s %d %zu %d %d\n", descriptions[1].message.p, descriptions[2].code, std::size(descriptions), code, (int)sizeof(sized));
    return argc;
}
