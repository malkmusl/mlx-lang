// A call operator with an abbreviated parameter after the template
// header's own (`template<typename... Rest> StyleType operator()(
// BuilderState&, std::derived_from<CSSValue> auto const& value, Rest&&...)`,
// WebCore's CSSValueConversionInvoker), in a class template.
#include <stdio.h>
#include <concepts>
struct V { int k = 1; }; struct P : V { };
template<class S> struct Inv {
  template<typename... Rest> int operator()(int a, std::derived_from<V> auto const& value, Rest&&... rest) const { return a + value.k + (int)sizeof...(Rest); }
  int g(auto const& x) const { return x.k; }
};
int main(int argc, char **argv) { Inv<int> i; P p; printf("%d %d\n", i(argc, p, 2, 3), i.g(p)); return argc - 2; }
