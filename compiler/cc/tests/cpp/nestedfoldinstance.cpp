// A fold expression whose elements make an instance of the function
// being read (WTF's TupleHashTraits::isEmptyValueImpl for a tuple of
// tuples): the instance reads the fold's own operator as it is, not as
// the `)` the outer reading made of it.
#include <stdio.h>
#include <tuple>
#include <utility>
namespace W {
template<typename T> struct HT { typedef T TraitType; static constexpr bool hasIsEmptyValueFunction = false; static T emptyValue() { return T(); } };
template<typename Traits, bool f> struct Checker;
template<typename Traits> struct Checker<Traits, true> { template<typename T> static bool isEmptyValue(const T& v) { return Traits::isEmptyValue(v); } };
template<typename Traits> struct Checker<Traits, false> { template<typename T> static bool isEmptyValue(const T& v) { return v == Traits::emptyValue(); } };
template<typename Traits, typename T> inline bool isEmpty(const T& value) { return Checker<Traits, Traits::hasIsEmptyValueFunction>::isEmptyValue(value); }
template<typename FirstTrait, typename... Traits>
struct TupleHT {
    typedef std::tuple<typename FirstTrait::TraitType, typename Traits::TraitType...> TraitType;
    static constexpr bool hasIsEmptyValueFunction = true;
    static bool isEmptyValue(const TraitType& value) { return isEmptyValueImpl(value, std::make_index_sequence<1 + sizeof...(Traits)> { }); }
private:
    using TraitsTuple = std::tuple<FirstTrait, Traits...>;
    template<std::size_t... Is>
    static bool isEmptyValueImpl(const TraitType& value, std::index_sequence<Is...>)
    {
        return (isEmpty<std::tuple_element_t<Is, TraitsTuple>>(std::get<Is>(value)) && ...);
    }
};
}
namespace J { enum class AccessType : signed char { A, B }; }
using Inner = W::TupleHT<W::HT<J::AccessType>, W::HT<bool>, W::HT<bool>, W::HT<bool>, W::HT<bool>>;
using Outer = W::TupleHT<Inner, W::HT<const int*>>;
int main()
{
    Outer::TraitType v { Inner::TraitType { J::AccessType::A, false, false, false, false }, nullptr };
    Outer::TraitType w { Inner::TraitType { J::AccessType::B, false, false, false, false }, nullptr };
    printf("%d %d\n", Outer::isEmptyValue(v), Outer::isEmptyValue(w));
    return 0;
}
