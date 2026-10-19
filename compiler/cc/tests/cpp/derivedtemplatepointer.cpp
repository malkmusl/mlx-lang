// A function template taking `Base<Args...> *` is chosen over a function
// taking `void *` for a pointer to a class derived from an instance of
// Base: the nearer base is the better conversion before a function wins
// over a template (WTF's IsBaseOfTemplate; WebCore's MarginEdge was no
// PrimitiveNumericOrKeyword, "the object is not callable").
#include <stdio.h>
#include <type_traits>
#include <utility>

template <template <typename...> class Base, typename Derived>
struct IsBaseOfTemplateImpl {
    template <typename... Args> static std::true_type test(Base<Args...>*);
    static std::false_type test(void*);
    static constexpr const bool value = decltype(test(std::declval<typename std::remove_cv<Derived>::type*>()))::value;
};
struct Range { double min; double max; };
constexpr Range AllUnzoomed { -1, 2 };
enum class Keyword { Auto, None };
template<Keyword K> struct KW { };
template<auto R, typename V = float> struct LengthPercentage { V v; };
template<typename N, typename... Ks> struct PNK;
template<auto R, typename... Ks> struct PNK<LengthPercentage<R, float>, Ks...> { int x; };
struct MarginEdge : PNK<LengthPercentage<AllUnzoomed>, KW<Keyword::Auto>> { };
template<typename... Ts> struct Plain { };
struct Other : Plain<int, KW<Keyword::None>> { };
struct Other2 : Plain<LengthPercentage<AllUnzoomed>> { };

int main(int argc, char **argv)
{
    printf("%d %d %d %d\n", (int)IsBaseOfTemplateImpl<PNK, MarginEdge>::value, (int)IsBaseOfTemplateImpl<Plain, Other>::value, (int)IsBaseOfTemplateImpl<Plain, Other2>::value, (int)IsBaseOfTemplateImpl<Plain, MarginEdge>::value);
    return argc;
}
