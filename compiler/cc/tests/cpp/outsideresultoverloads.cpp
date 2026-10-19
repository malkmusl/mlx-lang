// Member templates alike but for their results (`EnableIfMap<T,
// JSValue> asJS(...)` beside `EnableIfSet<T, JSValue> asJS(...)` of
// WebCore's JSDOMIterator.h), defined outside the class: each definition
// is its own declaration's ("no instance of the function template
// 'asJS' takes these arguments").
#include <stdio.h>
#include <optional>
#include <type_traits>
enum class Kind { Set, Map };
template<typename T, typename U = void> using EnableIfMap = typename std::enable_if_t<T::type == Kind::Map, U>;
template<typename T, typename U = void> using EnableIfSet = typename std::enable_if_t<T::type == Kind::Set, U>;

struct MapTraits { static constexpr Kind type = Kind::Map; };
struct SetTraits { static constexpr Kind type = Kind::Set; };

template<typename W, typename IteratorTraits> class Base {
    using Traits = IteratorTraits;
public:
    int next();
protected:
    template<typename V, typename T = Traits> EnableIfMap<T, int> asJS(int &, V &);
    template<typename V, typename T = Traits> EnableIfSet<T, int> asJS(int &, V &);
    std::optional<int> m_iterator { 5 };
};

template<typename W, typename IteratorTraits>
template<typename V, typename T> inline EnableIfMap<T, int> Base<W, IteratorTraits>::asJS(int &g, V &value) { return g + *value * 100; }
template<typename W, typename IteratorTraits>
template<typename V, typename T> inline EnableIfSet<T, int> Base<W, IteratorTraits>::asJS(int &g, V &value) { return g + *value; }

template<typename W, typename Traits> int Base<W, Traits>::next()
{
    int g = 1;
    if (m_iterator) {
        auto v = m_iterator;
        if (v)
            return asJS(g, v);
    }
    return 0;
}

int main(int argc, char **argv)
{
    Base<int, MapTraits> m;
    Base<int, SetTraits> s;
    printf("%d %d\n", m.next(), s.next());
    return argc;
}
