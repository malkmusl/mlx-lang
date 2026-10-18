// <algorithm>, <iterator> and <array> in constant expressions, as C++20
// has them: WebCore's CSS::KeywordList asks std::ranges::find over a
// std::array of its keywords in a concept, at compile time.
#include <stdio.h>
#include <algorithm>
#include <array>
#include <iterator>

enum class ID { Auto, Cover, Contain };
struct Auto { static constexpr ID value = ID::Auto; };
struct Cover { static constexpr ID value = ID::Cover; };
struct Contain { static constexpr ID value = ID::Contain; };

template<class... Keywords> struct KeywordList {
    static constexpr auto identifiers = std::array { Keywords::value... };
    static constexpr bool isValidKeyword(auto keyword) { return std::ranges::find(identifiers, keyword.value) != identifiers.end(); }
    static constexpr long offsetForKeyword(auto keyword) { return std::distance(identifiers.begin(), std::ranges::find(identifiers, keyword.value)); }
};
template<class K, class List> concept ValidKeywordForList = List::isValidKeyword(K());

template<class List> struct Value {
    template<ValidKeywordForList<List> K> constexpr Value(K keyword) : index(List::offsetForKeyword(keyword)) { }
    long index;
};

using Keywords = KeywordList<Auto, Contain>;
static_assert(Keywords::isValidKeyword(Contain()));
static_assert(!Keywords::isValidKeyword(Cover()));
static_assert(ValidKeywordForList<Auto, Keywords>);
static_assert(!ValidKeywordForList<Cover, Keywords>);

constexpr std::array<int, 5> numbers { 1, 3, 3, 7, 9 };
static_assert(*std::begin(numbers) == 1);
static_assert(std::count(numbers.begin(), numbers.end(), 3) == 2);
static_assert(std::is_sorted(numbers.begin(), numbers.end()));
static_assert(*std::find_if(numbers.begin(), numbers.end(), [](int n) { return n > 5; }) == 7);
static_assert(std::equal(numbers.begin(), numbers.begin() + 2, std::array { 1, 3 }.begin()));
static_assert(*std::lower_bound(numbers.begin(), numbers.end(), 4) == 7);
static_assert(std::ranges::find(numbers, 9) - numbers.begin() == 4);

int main(int argc, char **argv)
{
    Value<Keywords> value { Contain() };
    printf("%ld %d %d\n", value.index, (int)Keywords::isValidKeyword(Auto()), (int)std::count(numbers.begin(), numbers.end(), argc + 1));
    return argc - 2;
}
