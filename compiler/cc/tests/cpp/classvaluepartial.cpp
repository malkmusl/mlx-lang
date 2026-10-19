// A partial specialization whose arguments include a value parameter of
// a class type (WebCore's `template<Range R, SingleValueUnitEnum U,
// typename V> struct PrimitiveNumericRaw<R, U, V>`) matches a class's
// value given for it: it was never chosen (StyleZIndex's "no matching
// constructor").
#include <stdio.h>
#include <concepts>
#include <type_traits>

template<typename T> struct Traits;
enum class One { A };
enum class Two { A, B };
template<> struct Traits<One> { static constexpr int count = 1; };
template<> struct Traits<Two> { static constexpr int count = 2; };
template<typename T> concept UnitEnum = std::is_enum_v<T> && requires { requires std::integral<decltype(Traits<T>::count)>; };
template<typename T> concept Single = UnitEnum<T> && requires { requires (Traits<T>::count == 1); };

struct Range { double min; double max; };
template<Range R, UnitEnum U, typename V> struct Raw {
    static constexpr int kind = 1;
    constexpr Raw(U, double v) : value(v) { }
    double value;
};
template<Range R, Single U, typename V> struct Raw<R, U, V> {
    static constexpr int kind = 2;
    template<typename T> requires std::integral<T> || std::floating_point<T> constexpr Raw(T v) : value(static_cast<double>(v)) { }
    double value;
};
constexpr Range All { -1, 1 };
constexpr Range Half { 0, 0.5 };
template<Range A, Range B> struct Pair { static constexpr int same = 0; };
template<Range A> struct Pair<A, A> { static constexpr int same = 1; };

int main(int argc, char **argv)
{
    Raw<All, One, int> a(4.5);
    Raw<All, Two, int> b(Two::B, 2.5);
    printf("%d %d %g %g\n", Raw<All, One, int>::kind, Raw<All, Two, int>::kind, a.value, b.value);
    printf("%d %d\n", Pair<All, All>::same, Pair<All, Half>::same);
    return argc;
}
