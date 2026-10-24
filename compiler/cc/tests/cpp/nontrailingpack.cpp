// A function parameter pack before other parameters deduces nothing: it
// takes as many arguments as it was given elements explicitly (none when
// none); and inside an element of an enclosing expansion an argument
// with its own `...` expands its own packs whole. WTF's tuple_zip calls
// `tuple_zip_impl<Head, Tail...>(head, tail..., index_sequence)`, which
// makes `zip_tuple_at_index<I>(forward<TupleLikes>(tupleLikes)...)...`
// (WebCore's style-30 stopped there).
#include <stdio.h>
#include <tuple>
#include <type_traits>
#include <utility>

template<std::size_t I, typename... TupleLikes> using zip_tuple_at_index_t = std::tuple<std::tuple_element_t<I, std::decay_t<TupleLikes>>...>;

template<std::size_t I, typename... TupleLikes> auto zip_tuple_at_index(TupleLikes&&... tupleLikes)
{
    return zip_tuple_at_index_t<I, TupleLikes...> { std::get<I>(std::forward<TupleLikes>(tupleLikes))... };
}

template<typename... TupleLikes, std::size_t... I> auto tuple_zip_impl(TupleLikes&&... tupleLikes, std::index_sequence<I...>)
{
    return std::tuple<zip_tuple_at_index_t<I, TupleLikes...>...> {
        zip_tuple_at_index<I>(std::forward<TupleLikes>(tupleLikes)...)...
    };
}

template<typename Head, typename... Tail> auto tuple_zip(Head&& head, Tail&&... tail)
{
    constexpr std::size_t size = std::tuple_size_v<std::decay_t<Head>>;
    return tuple_zip_impl<Head, Tail...>(std::forward<Head>(head), std::forward<Tail>(tail)..., std::make_index_sequence<size>());
}

template<typename... Ts, typename L> int counted(Ts... ts, L last) { return (int)sizeof...(Ts) * 100 + (int)last + (0 + ... + ts); }

template<int I, class... T> int at(T... t) { int a[] = { t... }; return a[I]; }
template<class... T> int sum(T... t) { return (0 + ... + t); }
struct Two { int a, b; };
template<class... T, int... I> int picks(std::integer_sequence<int, I...>, T... t)
{
    int array[] = { at<I>(t...)... };
    Two two { at<I>(t...)... };
    return sum(at<I>(t...)...) * 10000 + array[1] * 100 + two.a * 10 + two.b;
}

int main(int argc, char **)
{
    std::tuple<int, double> a { 1, 0.5 };
    std::tuple<char, long> b { 'x', 7 };
    auto z = tuple_zip(a, b);
    printf("%d %c %.1f %ld\n", std::get<0>(std::get<0>(z)), std::get<1>(std::get<0>(z)), std::get<0>(std::get<1>(z)), std::get<1>(std::get<1>(z)));
    static_assert(std::is_same_v<decltype(z), std::tuple<std::tuple<int, char>, std::tuple<double, long>>>);
    printf("%d %d\n", counted<int, int>(1, 2, 3), counted<>(4));
    printf("%d\n", picks(std::integer_sequence<int, 1, 2>(), 3, 4, 5));
    return argc;
}
