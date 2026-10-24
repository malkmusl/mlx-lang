// std::min<double> names the template of (a, b), not the one with a comparison
// (nothing to deduce it from): CSSNumericValue's operationOnValuesOfSameUnit.
#include <algorithm>
#include <stdio.h>
template <typename F> double apply(F op, double a, double b) { return op(a, b); }
int main() {
    printf("%g\n", apply<const double&(*)(const double&, const double&)>(std::min<double>, 3, 2));
    const double &(*p)(const double &, const double &) = std::max<double>;
    printf("%g\n", p(3, 2));
}
