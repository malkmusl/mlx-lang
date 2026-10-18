// std::is_constructible weighs constructor templates too, their
// signatures' failures (SFINAE, constraints) making them no candidates
// (mpark::variant's converting constructor, which WTF::CompactVariant
// asks std::constructible_from about).
#include <stdio.h>
#include <concepts>
#include <type_traits>
#include <utility>

struct Keyword { };
struct Other { };
struct Text { const char *chars; Text(const char *c) : chars(c) { } };

template<class A, class B> struct Either {
    template<class Arg, class D = std::decay_t<Arg>,
             std::enable_if_t<!std::is_same_v<D, Either>, int> = 0,
             std::enable_if_t<std::is_constructible_v<A, Arg> || std::is_constructible_v<B, Arg>, int> = 0>
    constexpr Either(Arg &&) : index(std::is_constructible_v<A, Arg> ? 0 : 1) { }
    int index;
};

struct Explicit {
    template<class T> explicit Explicit(T *) { }
};

struct Constrained {
    template<class T> requires std::integral<T> Constrained(T) { }
};

using KeywordOrText = Either<Keyword, Text>;
static_assert(std::is_constructible_v<KeywordOrText, Keyword>);
static_assert(std::is_constructible_v<KeywordOrText, Keyword &>);
static_assert(std::is_constructible_v<KeywordOrText, const char *>);
static_assert(!std::is_constructible_v<KeywordOrText, Other>);
static_assert(std::constructible_from<KeywordOrText, const Keyword &>);
static_assert(std::is_constructible_v<Explicit, int *>);
static_assert(!std::is_convertible_v<int *, Explicit>);
static_assert(std::is_constructible_v<Constrained, long>);
static_assert(!std::is_constructible_v<Constrained, double>);

int main(int argc, char **argv)
{
    KeywordOrText keyword { Keyword { } };
    KeywordOrText text { "none" };
    printf("%d %d %d\n", keyword.index, text.index, (int)std::is_constructible_v<KeywordOrText, Other>);
    return argc - 2;
}
