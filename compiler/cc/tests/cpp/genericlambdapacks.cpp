// A generic lambda's body naming a function parameter pack of the
// function around (`std::forward<Rest>(rest)...` in WebCore's
// CSSValueTypes.h: "undeclared identifier 'rest'"): each element is
// captured, by reference and by copy, the pack empty too.
#include <stdio.h>
#include <utility>

template<typename F, typename V> auto visit(F &&f, const V &v) { return f(v); }
void out(int a) { printf("%d\n", a); }
void out(int a, int b, long c) { printf("%d %d %ld\n", a, b, c); }
int sum() { return 0; }
template<typename... T> int sum(int first, T... more) { return first + sum(more...); }

template<typename T, typename... Rest> void each(const T &value, Rest &&...rest)
{
    visit([&](const auto &alternative) { out(alternative, std::forward<Rest>(rest)...); }, value);
}

template<typename... Rest> int copied(int start, Rest... rest)
{
    auto f = [=](auto scale) { return scale * sum(start, rest...); };
    ((rest = 0), ...);
    return f(2) + visit([&](auto x) { return x + sum((rest + 1)...); }, 100);
}

struct Holder {
    int base = 1000;
    template<typename... Rest> int add(Rest... rest)
    {
        return visit([&](auto x) { return x + base + sum(rest...); }, 5);
    }
};

int main(int argc, char **argv)
{
    each(1);
    each(2, 3, 4L);
    printf("%d %d %d\n", copied(1), copied(1, 2, 3), Holder().add(10, 20));
    return argc;
}
