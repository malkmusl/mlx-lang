// An aggregate initialized with braces around one expression of its own
// type is a copy of it ([dcl.init.list] 3.2), static, constexpr or a
// class's static member alike (JavaScriptCore's StaticScratchRegisterAllocator:
// `static constexpr ArrayType values { constructScratchRegisters() };`
// had its first member made of the array).
#include <array>
#include <stdio.h>

struct Three {
    signed char e[3];
};

constexpr Three make()
{
    Three x { };
    x.e[0] = 4;
    x.e[1] = 5;
    return x;
}

static constexpr Three global { make() };
static const std::array<int, 2> pair { std::array<int, 2> { 7, 8 } };

struct Holder {
    static constexpr Three member { make() };
};

template<int N> struct Instance {
    static constexpr Three member { make() };
};

static constexpr Three copied = Holder::member;
static constexpr Three fromInstance = Instance<1>::member;

int main()
{
    Three local { make() };
    printf("%d %d | %d %d | %d %d | %d %d | %d %d | %d %d\n", global.e[0], global.e[1], pair[0], pair[1], local.e[0], local.e[1],
        Holder::member.e[0], Holder::member.e[1], copied.e[0], copied.e[1], fromInstance.e[0], fromInstance.e[1]);
    return 0;
}
