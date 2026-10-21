// A name that does not exist in the middle of a qualified name
// (`typename P::Index::NumericType::Base::Index`, no Base) or in a class
// template's argument (`Index<typename N::Base>`) fails the constraint
// reading it, without an instance made of a stand-in (WebCore's
// CSS::PrimitiveData and PrimitiveDataIndex).
#include <stdio.h>
#include <concepts>

template<class N, class... Keywords> struct DataIndex {
    using NumericType = N;
    // (N without a member Value: a hard error for an instance.)
    using Value = typename N::Value;
    template<typename T>
        requires (sizeof...(Keywords) != 0) && (requires { requires std::same_as<T, DataIndex<typename N::Base>>; })
    DataIndex(const T &) : kind(1) { }
    template<class R> DataIndex(R) : kind(2) { }
    int kind;
};

template<typename C, typename P> concept Subsumes
    = (!std::same_as<C, P>)
   && (std::same_as<typename C::Index, typename P::Index::NumericType::Base::Index>);

template<class N, class... Keywords> struct Data {
    using Index = DataIndex<N, Keywords...>;
    template<Subsumes<Data> T> Data(const T &) : kind(1), index(0) { }
    template<class R> Data(R raw) : kind(2), index(raw) { }
    int kind;
    Index index;
};

struct Raw { };
template<class R> struct Primitive {
    using Value = int;
    Data<Primitive<R>> data { R { } };
};

int main(int argc, char **argv)
{
    Primitive<Raw> primitive;
    printf("%d %d\n", primitive.data.kind, primitive.data.index.kind);
    return argc - 2;
}
