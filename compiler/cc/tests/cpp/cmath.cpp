// <cmath>'s overloads: float, long double and integer arguments, the
// classification functions (isnan, signbit, fpclassify) as calls,
// abs, lerp, frexp; math.h and cmath together.
#include <cmath>
#include <math.h>
#include <stdio.h>
static float roundHalfEven(float value) {
    float rounded = std::round(value);
    if (std::fabs(value - rounded) == 0.5f) {
        if (std::fmod(rounded, 2.0f) != 0.0f)
            return rounded - std::copysign(1.0f, value);
    }
    return rounded;
}
int main() {
    printf("%g %g %g\n", roundHalfEven(2.5f), roundHalfEven(3.5f), roundHalfEven(-2.5f));
    double d = 2.0; float f = 3.0f; long double ld = 4.0L;
    printf("%g %g %Lg %g\n", std::sqrt(d), std::sqrt(f), std::sqrt(ld), std::sqrt(16));
    printf("%g %g %g\n", std::pow(d, 3), std::pow(f, 2.0f), std::pow(2, 10));
    printf("%d %d %d %d\n", std::isnan(NAN), std::isinf(INFINITY), std::isfinite(1.0), std::signbit(-1.0f));
    printf("%d %d %d\n", isnan(0.0 / 0.0), std::isnan(1), (int)std::lround(2.6));
    printf("%g %g %g\n", std::abs(-2.5), std::abs(-1.5f), std::lerp(0.0, 10.0, 0.25));
    int e; double m = std::frexp(8.0, &e);
    printf("%g %d %g %g\n", m, e, std::ldexp(1.0, 4), std::atan2(1.0f, 1.0f));
    printf("%d %g %g\n", std::fpclassify(0.0) == FP_ZERO, std::fmax(1, 2.5), std::trunc(-2.7));
    return 0;
}
