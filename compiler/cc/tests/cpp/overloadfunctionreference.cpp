// An overloaded inline function bound to a reference to function (the
// conformance suite passes std::abs and the like as `DoubleFunc1 &`): the
// overload of the reference's type is chosen and its body is emitted,
// though nothing calls it by name.
#include <stdio.h>

typedef double DoubleFunc1(double);

inline double over(double x)
{
    return x + 1;
}

inline int over(int x)
{
    return x + 2;
}

static double apply(DoubleFunc1 &f, double x)
{
    return f(x);
}

int main()
{
    DoubleFunc1 &r      = over;
    double (*p)(double) = over;
    printf("%g %g %g\n", apply(over, 2), r(5), p(7));
    return 0;
}
