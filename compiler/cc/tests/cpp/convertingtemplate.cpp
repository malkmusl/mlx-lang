// A converting constructor template whose default arguments weigh a
// conversion of their own: mpark::variant's `best_match` converts the
// argument to each alternative (WebCore's `create("none"_s)` for a
// Variant<..., String, ...>). And a copy constructor takes a derived
// class's object (std::is_constructible<EffectTiming, const
// CustomAnimationOptions &>, then `return options;` for a
// Variant<double, EffectTiming>).
#include <stdio.h>
#include <type_traits>
#include <tuple>
#include <utility>

template<unsigned long I> using Index = std::integral_constant<unsigned long, I>;
template<class Arg, unsigned long I, class T> struct Leaf {
    using Function = Index<I> (*)(T);
    operator Function() const { return nullptr; }
};
template<class Arg, class... Ts> struct Overloads {
    template<class> struct Set;
    template<unsigned long... Is> struct Set<std::index_sequence<Is...>> : Leaf<Arg, Is, Ts>... { };
    using Type = Set<std::index_sequence_for<Ts...>>;
};
template<class Arg, class... Ts> using BestMatch = decltype(std::declval<typename Overloads<Arg, Ts...>::Type>()(std::declval<Arg>()));

struct Literal { const char *text; };
struct Str { Str(Literal literal) : text(literal.text) { } const char *text; };
struct Timing { double delay; };
struct Options : Timing { int id; };

template<class... Ts> struct Variant {
    template<class Arg, class D = std::decay_t<Arg>, std::enable_if_t<!std::is_same<D, Variant>::value, int> = 0,
        unsigned long I = BestMatch<Arg, Ts...>::value,
        std::enable_if_t<std::is_constructible<std::tuple_element_t<I, std::tuple<Ts...>>, Arg>::value, int> = 0>
    Variant(Arg &&) : index(I) { }
    unsigned long index;
};

int indexOf(Variant<Str> v) { return (int)v.index; }
int indexOf(Variant<int, Str> v, int) { return (int)v.index; }
Variant<double, Timing> timing(const Options &options) { return options; }

int main(int argc, char **argv)
{
    printf("%d %d\n", (int)std::is_convertible<Literal, Variant<Str>>::value, (int)std::is_convertible<Literal, Variant<int, Str>>::value);
    printf("%d %d\n", (int)std::is_constructible<Timing, const Options &>::value, (int)std::is_constructible<Timing, Options &&>::value);
    printf("%d %d %d\n", indexOf(Literal { "none" }), indexOf(Literal { "none" }, argc), (int)timing(Options { { 1.5 }, 2 }).index);
    return argc - 2;
}
