// `!` on a class without a conversion to bool inside a requires-
// expression makes the requirement false, not an error (WebCore's
// RectEdges<T>::isZero for T a class of the style values).
#include <stdio.h>
#include <algorithm>
#include <array>
#include <concepts>
#include <utility>

template<class T> struct RectEdges {
    std::array<T, 4> sides;
    template<typename F> bool allOf(F&& functor) const { return std::ranges::all_of(sides, std::forward<F>(functor)); }
    bool isZero() const
        requires (requires { { !std::declval<T>() } -> std::same_as<bool>; })
    {
        return allOf([](auto& edge) { return !edge; });
    }
    bool operator==(const RectEdges<T>&) const = default;
};

struct Outset {
    int value;
    bool operator==(const Outset&) const = default;
};

template<class T> concept Negatable = requires(T t) { { !t } -> std::same_as<bool>; };

int main(int argc, char **argv)
{
    RectEdges<Outset> outsets { { Outset { 1 }, Outset { 2 }, Outset { 3 }, Outset { 4 } } };
    RectEdges<int> zeros { { 0, 0, 0, 0 } };
    RectEdges<int> mixed { { 0, argc, 0, 0 } };
    printf("%d %d %d %d %d\n", (int)(outsets == outsets), (int)zeros.isZero(), (int)mixed.isZero(), (int)Negatable<int>, (int)Negatable<Outset>);
    return argc - 2;
}
