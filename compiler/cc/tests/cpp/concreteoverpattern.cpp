// Partial ordering of function templates: a parameter of a type naming no
// template parameter is more specialized than `const Op &` (WebCore's
// Style::Calculation::forAllChildren(const Child &, const F &) over
// forAllChildren(const Op &, const F &), whose body would apply a tuple
// protocol Child has not), and `vector<int>` than `vector<T>`.
#include <cstdio>
#include <tuple>
#include <vector>
struct Child { int value; };
struct Node { int a; int b; };
template<typename F, typename Op> int forAll(const Op& root, const F& f)
{
    return std::apply([&](const auto&... x) { return (0 + ... + f(x)); }, std::tuple<int, int>(root.a, root.b));
}
template<typename F> int forAll(const Child& root, const F& f) { return f(root.value) * 100; }
template<typename T> int which(const std::vector<T>&) { return 1; }
template<typename T> int which(const std::vector<int>&) { return 2; }
template<typename T> int pick(const T&, int) { return 1; }
template<typename T> int pick(const T&, const T&) { return 2; }
int main() {
    auto twice = [](int x) { return x * 2; };
    std::printf("%d %d\n", forAll(Child { 4 }, twice), forAll(Node { 1, 2 }, twice));
    std::printf("%d %d\n", which<int>(std::vector<int> { }), pick(1.5, 2));
    return 0;
}
