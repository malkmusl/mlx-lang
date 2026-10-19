// `using Base::Base;` with Base a typedef the base class declares (not
// the derived class): the constructors are inherited, conversions through
// them included (WebCore's BackgroundSizeLength, made from a keyword in
// SpaceSeparatedSize's constructor).
#include <stdio.h>
#include <type_traits>
#include <utility>

template<class... Keywords> struct KeywordList {
    template<class K> static constexpr bool contains = (std::is_same_v<K, Keywords> || ...);
};
template<class K, class List> concept ValidKeywordForList = List::template contains<K>;
struct Auto { };
struct Cover { };

template<class T> struct LengthPercentage { T value; };
template<class N, class... Keywords> struct NumericOrKeyword;
template<class T, class... Keywords> struct NumericOrKeyword<LengthPercentage<T>, Keywords...> {
    using Base = NumericOrKeyword<LengthPercentage<T>, Keywords...>;
    using List = KeywordList<Keywords...>;
    template<ValidKeywordForList<List> Keyword> NumericOrKeyword(Keyword) : index(7) { }
    NumericOrKeyword(int x) : index(x) { }
    int index;
};

struct SizeLength : NumericOrKeyword<LengthPercentage<float>, Auto> {
    using Base::Base;
};

template<class T> struct SpaceSeparatedSize {
    constexpr SpaceSeparatedSize(T first, T second) : first(std::move(first)), second(std::move(second)) { }
    T first, second;
};

struct BackgroundSize {
    using LengthSize = SpaceSeparatedSize<SizeLength>;
    BackgroundSize(Auto keyword) : value { LengthSize { keyword, keyword } } { }
    BackgroundSize(SizeLength&& length) : value { LengthSize { length, Auto { } } } { }
    LengthSize value;
};

int main(int argc, char **argv)
{
    BackgroundSize automatic { Auto { } };
    BackgroundSize fixed { SizeLength { 3 } };
    SizeLength converted = Auto { };
    printf("%d %d %d %d %d\n", automatic.value.first.index, automatic.value.second.index, fixed.value.first.index, fixed.value.second.index, converted.index);
    return argc - 2;
}
