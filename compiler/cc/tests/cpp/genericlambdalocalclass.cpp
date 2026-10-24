// A generic lambda names a class or enum declared in the function around
// it (`-> std::optional<TypedChildOrNone>` in WebCore's consumeClamp):
// its call operator's instances are read once the function is gone, so
// the closure keeps the local classes its body names, as it keeps local
// typedefs. One hiding a global class of the name is the local one.
#include <optional>
#include <stdio.h>

struct Pair { long a; };

static std::optional<int> consume(int depth)
{
    struct TypedChildOrNone {
        int child;
        long type;
    };
    enum class Kind { None, Some };
    struct Pair { int a; int b; };
    auto parse = [](auto &tokens, auto d) -> std::optional<TypedChildOrNone> {
        if (tokens < 0)
            return std::nullopt;
        return TypedChildOrNone { tokens + d, 2 };
    };
    auto kindOf = [](auto value) { return value > 4 ? Kind::Some : Kind::None; };
    auto pair = [](auto x) { return Pair { x, x * 3 }; };
    int t = depth;
    auto r = parse(t, 3);
    if (!r)
        return std::nullopt;
    return r->child + (int)r->type + (kindOf(r->child) == Kind::Some ? 100 : 0) + pair(2).b + (int)sizeof(pair(1));
}

int main(int argc, char **)
{
    Pair global { 5 };
    printf("%d %d %ld\n", *consume(4), (int)consume(-9).has_value(), global.a);
    return argc;
}
