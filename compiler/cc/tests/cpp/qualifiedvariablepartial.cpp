// A variable template's partial specializations declared by its
// qualified name at file scope, as WebCore's CSSPrimitiveNumeric.h has
// them (`template<auto R, typename V> inline constexpr auto
// WebCore::TreatAsVariantLike<WebCore::CSS::Number<R, V>> = true;`):
// the template the qualifier names is specialized (not a new one made at
// file scope), and a concept reads the specializations too.
#include <stdio.h>

namespace W {
template<typename> inline constexpr auto TreatAs = false;
template<typename T> concept Like = TreatAs<T>;
namespace C {
struct Range { double min; double max; };
inline constexpr Range All { -1, 1 };
inline constexpr Range Nonnegative { 0, 1 };
template<Range R = All, typename V = double> struct Number { V value; };
template<typename V> struct Plain { V value; };
template<int N> struct Sized { };
}
}
template<auto R, typename V> inline constexpr auto W::TreatAs<W::C::Number<R, V>> = true;
template<typename V> inline constexpr auto W::TreatAs<W::C::Plain<V>> = true;
template<auto N> inline constexpr auto W::TreatAs<W::C::Sized<N>> = N > 2;

template<typename T> int kind(const T &) requires W::Like<T> { return 1; }
template<typename T> int kind(const T &) requires (!W::Like<T>) { return 2; }

int main(int argc, char **argv)
{
    printf("%d %d %d\n", (int)W::Like<W::C::Number<W::C::Nonnegative, int>>, (int)W::Like<W::C::Plain<long>>, (int)W::Like<W::C::Number<>>);
    printf("%d %d %d\n", (int)W::Like<W::C::Sized<3>>, (int)W::Like<W::C::Sized<1>>, (int)W::Like<int>);
    printf("%d %d %d\n", (int)W::TreatAs<W::C::Number<W::C::All, float>>, kind(W::C::Plain<char> { 'x' }), kind(5));
    return argc;
}
