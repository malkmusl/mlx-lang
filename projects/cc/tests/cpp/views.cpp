// views::reverse and views::transform (called and piped), a global
// generic lambda, lambdas with template parameters, argument-dependent
// lookup of plain calls (is_lt of an ordering, a namespace's function
// for its class or enum), and concepts whose definitions hold a
// requires expression and a lambda.
#include <stdio.h>
#include <compare>
#include <cstddef>
#include <ranges>
#include <tuple>
#include <utility>
#include <vector>

static auto dereferenceView = std::views::transform([](auto &&x) -> decltype(auto) { return *x; });
namespace n { struct P { int v; }; int twice(P p) { return p.v * 2; } enum class E { a }; int code(E) { return 7; } }
template <class T> concept Pairish = requires(T t) { t.first; t.second; };
// (Read, not evaluated: constant evaluation does not call closures.)
template <class T> concept Twoish = requires(T t) { t.first; } && []<std::size_t... N>(std::index_sequence<N...>) { return sizeof...(N) == 2; }(std::make_index_sequence<2>());

int main() {
    std::vector<int> v{1, 2, 3};
    for (int x : v | std::views::reverse) printf("%d ", x);
    for (int x : std::views::reverse(v)) printf("%d ", x);
    for (int x : std::views::transform(v, [](int y) { return y * 10; })) printf("%d ", x);
    for (int x : v | std::views::transform([](int y) { return y + 100; })) printf("%d ", x);
    int a = 7, b = 8;
    std::vector<int *> ptrs{&a, &b};
    for (int x : ptrs | dereferenceView) printf("%d ", x);
    printf("\n");
    auto sum = []<std::size_t... N>(std::index_sequence<N...>) { return (N + ... + 0); };
    auto scale = []<class T>(T a, auto b) { return a * b; };
    printf("%d %g\n", (int)sum(std::make_index_sequence<5>()), scale(2.5, 2));
    n::P p{4};
    std::partial_ordering o = 1 <=> 2;
    printf("%d %d %d %d\n", twice(p), code(n::E::a), (int)is_lt(o), (int)is_eq(o));
    printf("%d %d\n", (int)Pairish<std::pair<int, int>>, (int)Pairish<int>);
    return 0;
}
