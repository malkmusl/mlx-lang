// A variable template of an outer namespace (WebCore::Style's
// `evaluate<Result>`, an invoker object) beside functions of the name in
// an inner one (Style::Calculation's evaluate overloads, one a function
// template), called from a generic lambda: the variable template is no
// candidate of the call (mlx-cc crashed on its missing function pattern
// in StyleCalculationTree+Evaluation.cpp), nor in `evaluate<Result>(x)`
// of a class template's member (Style::Evaluation's).
#include <cstdio>
#include <variant>
namespace Style {
template<typename Result> struct Invoker {
    template<typename T> Result operator()(const T& value) const { return Result(value.value * 1000); }
};
template<typename Result> inline constexpr Invoker<Result> evaluate {};
template<typename V, typename Result> struct Evaluation {
    Result operator()(const V& value) { return std::visit([&](const auto& alternative) { return evaluate<Result>(alternative); }, value); }
};
namespace Calculation {
template<typename Op> struct Node { Op op; double value; };
struct Sum { };
struct Product { };
struct Leaf { double value; };
using Child = std::variant<Leaf, Node<Sum>, Node<Product>>;
static auto evaluate(const Leaf&) -> double;
static auto evaluate(const Node<Sum>&) -> double;
template<typename Op> static auto evaluate(const Node<Op>&) -> double;
double evaluate(const Child& v)
{
    return std::visit([&](const auto& x) { return evaluate(x); }, v);
}
double evaluate(const Leaf& leaf) { return leaf.value; }
double evaluate(const Node<Sum>& node) { return node.value + 100; }
template<typename Op> double evaluate(const Node<Op>& node) { return node.value * 2; }
}
}
int main() {
    using namespace Style::Calculation;
    Child a = Leaf { 1.5 }, b = Node<Sum> { { }, 2 }, c = Node<Product> { { }, 3 };
    std::printf("%g %g %g %g %g\n", evaluate(a), evaluate(b), evaluate(c), Style::evaluate<double>(Leaf { 0.5 }), Style::Evaluation<Child, double> { }(c));
    return 0;
}
