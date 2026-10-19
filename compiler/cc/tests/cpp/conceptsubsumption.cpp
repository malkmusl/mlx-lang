// Of two function templates alike but for their constraints, the one
// whose constraint subsumes the other's: `DimensionPercentageNumeric
// auto` over `Numeric auto` where `concept DimensionPercentageNumeric =
// Numeric<T> && VariantLike<T> && ...` (WebCore's
// StylePrimitiveNumericTypes+Calculation.h), whichever comes first, the
// concepts named in another namespace or through others.
#include <stdio.h>

namespace css {
template<typename T> concept Numeric = requires (T t) { t.value; };
template<typename T> concept Composite = Numeric<T> && (sizeof(T) > 4);
}

namespace style {
template<typename T> concept Numeric = requires (T t) { t.value; };
template<typename T> concept VariantLike = requires (T t) { t.alternative; };
template<typename T> concept Mixed = Numeric<T> && VariantLike<T> && css::Numeric<typename T::Raw>;
template<typename T> concept MixedMore = Mixed<T> && requires (T t) { t.more; };

int copy(Numeric auto const &) { return 1; }
int copy(Mixed auto const &) { return 2; }

template<MixedMore T> int pick(const T &) { return 3; }
template<Mixed T> int pick(const T &) { return 2; }
template<Numeric T> int pick(const T &) { return 1; }

int composite(css::Numeric auto const &) { return 1; }
int composite(css::Composite auto const &) { return 2; }
}

struct Raw { int value; };
struct Plain { int value; using Raw = ::Raw; };
struct Variant { int value; int alternative; using Raw = ::Raw; };
struct Richer { int value; int alternative; int more; using Raw = ::Raw; };
struct Small { char value; };
struct Big { int value; int other; };

int main(int argc, char **)
{
    Plain p { 1 };
    Variant v { 1, 2 };
    Richer r { 1, 2, 3 };
    printf("%d %d %d\n", style::copy(p), style::copy(v), style::copy(r));
    printf("%d %d %d\n", style::pick(p), style::pick(v), style::pick(r));
    printf("%d %d\n", style::composite(Small { 1 }), style::composite(Big { 1, 2 }));
    return argc;
}
