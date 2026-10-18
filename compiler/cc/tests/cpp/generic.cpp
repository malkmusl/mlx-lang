// `auto` parameters: abbreviated function templates (plain, in a
// class, beside template parameters, constrained, packs), generic
// lambdas capturing by reference and by copy, std::vector made from
// braces and from a range, a defaulted template parameter holding a
// decltype with a comma, and T::member in a template's signature.
#include <stdio.h>
#include <algorithm>
#include <concepts>
#include <optional>
#include <string>
#include <utility>
#include <vector>

template <typename O> typename O::value_type valueOrCompute(O optional, const std::invocable<> auto &callback) { return optional ? *optional : callback(); }
int twice(auto x) { return x * 2; }
auto sum(auto... xs) { return (xs + ... + 0); }
struct S {
    int base;
    int add(const auto &v) const { return base + v; }
    template <typename T> T mix(T t, auto u) { return t + (T)u; }
};
struct Gen { int operator()() const { return 42; } };
template <typename F> int apply(F f) { return f(3) + f(4); }
template <class It, class = decltype(*std::declval<It &>(), ++std::declval<It &>())> int iterable(It) { return 1; }
struct B { typedef int value_type; };
template <typename T> typename T::value_type zero(T) { return 0; }

int main() {
    std::optional<int> none, some(7);
    S s{10};
    printf("%d %d %d %d %d\n", valueOrCompute(none, Gen{}), valueOrCompute(some, Gen{}), twice(21), twice(1.25), sum(1, 2, 3));
    printf("%d %d %g\n", s.add(5), s.add('a'), s.mix(1.5, 2));
    auto doubled = [](auto x) { return x * 2; };
    printf("%d %g\n", doubled(21), doubled(1.25));
    int base = 100;
    auto add = [&](const auto &x) { return base + x; };
    auto copy = [=](auto x) mutable { return base - x; };
    base = 200;
    printf("%d %d\n", add(5), copy(5));
    std::vector<int> v{5, 2, 9, 1};
    std::sort(v.begin(), v.end(), [](auto &a, auto &b) { return a > b; });
    for (int x : v) printf("%d ", x);
    printf("\n%d\n", apply([base](auto n) { return base * n; }));
    auto both = [](auto a, auto b) { return a + b; };
    printf("%g %d\n", both(1, 2.5), both(1, 2));
    std::vector<int> a = {1, 2}, e{}, r(v.begin(), v.end()), n(3, 7);
    std::vector<std::string> words{"x", "yy"};
    printf("%d %d %d %d %d %s %d\n", (int)a.size(), a[1], (int)e.size(), (int)r.size(), n[2], words[1].c_str(), iterable(v.begin()));
    B b;
    printf("%d\n", zero(b));
    return 0;
}
