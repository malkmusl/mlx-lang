// A class template instance for a class not complete yet whose member
// template takes `T (&)[N]` (std::span's): the array type in the
// pattern does not need T whole (WebCore's MQ::Condition holding a
// Vector of a Variant of itself).
#include <cstdio>
#include <cstddef>

template<typename T> struct View {
    T* data { nullptr };
    std::size_t size { 0 };
    View() = default;
    template<std::size_t N> View(T (&array)[N]) : data(array), size(N) { }
};

template<typename T> struct Holder {
    alignas(8) unsigned char bytes[sizeof(T) * 2];
};

template<typename T> struct List {
    T* items { nullptr };
    std::size_t count { 0 };
    View<T> view() const { return { }; }
};

struct Condition;
struct Feature { int id; };
template<typename A, typename B> struct Either {
    Holder<A> a;
    Holder<B> b;
    int which;
};
struct Condition {
    List<Either<Condition, Feature>> queries;
    int op;
};

int main(int argc, char** argv)
{
    Feature features[3] { { 1 }, { 2 }, { argc } };
    View<Feature> v(features);
    Condition c { { }, 4 };
    std::printf("%zu %d %d %zu\n", v.size, v.data[2].id, c.op, c.queries.view().size);
    return 0;
}
