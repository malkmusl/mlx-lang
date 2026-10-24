// An alias template of a pack (`template<typename... Ts> using Variant =
// std::variant<Ts...>`) in a function template's parameter, given a
// parameter and another type (`Variant<T, CSS::Keyword::None>`,
// WebCore's CSSCalcExecutor.h): the class template's pattern with those
// arguments as given, deduced against an instance.
#include <stdio.h>
#include <variant>

template<typename... Ts> struct Box { };
template<typename... Ts> using BoxAlias = Box<Ts...>;
template<typename... Ts> using Variant = std::variant<Ts...>;
struct None { };

template<class T> int first(BoxAlias<T, None>) { return 1; }
template<class T> int second(BoxAlias<None, T>) { return 2; }
template<class T> int alone(BoxAlias<T>) { return 3; }
template<class... Us> int rest(BoxAlias<int, Us...>) { return 10 + sizeof...(Us); }
template<class T, class... Us> int both(BoxAlias<T, Us...>, T value) { return (int)value + sizeof...(Us); }

struct Clamp {
    template<class T> T operator()(T min, T value, T max) { return value < min ? min : value > max ? max : value; }
    template<class T> T operator()(Variant<T, None> min, T value, Variant<T, None> max)
    {
        return (*this)(std::get<T>(min), value, std::get<T>(max));
    }
};

int main(int argc, char **)
{
    printf("%d %d %d\n", first(Box<double, None>()), second(Box<None, char>()), alone(Box<long>()));
    printf("%d %d %d\n", rest(Box<int>()), rest(Box<int, char, long>()), both(Box<short, int, int>(), (short)5));
    Variant<double, None> low = 1.5;
    Variant<double, None> high = 2.5;
    printf("%g %g\n", Clamp()(low, 3.0, high), Clamp()(1.0, 0.5, 2.0));
    return argc;
}
