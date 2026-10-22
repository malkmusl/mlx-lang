// A function template whose parameter of a type naming no template
// parameter cannot take its argument is not viable before its
// constraint is checked (CWG2369): WTF's TrailingArray constructor
// `(Failable, unsigned, G &&) requires invocable<G, size_t>` for
// `(size, range, mapper)`, whose generic lambda would otherwise be read
// for a size_t (a hard error in its body).
#include <concepts>
#include <cstddef>
#include <cstdio>
#include <optional>
#include <utility>
#include <vector>
struct Pct { double value; };
struct Item { int percentage; std::optional<Pct> p; };
template<typename T> struct Fixed {
    struct Failable { };
    std::vector<T> items;
    template<typename... Args> explicit Fixed(unsigned size, Args&&...) { items.resize(size); }
    template<std::invocable<size_t> G> Fixed(Failable, unsigned size, G&& g) { for (unsigned i = 0; i < size; ++i) items.push_back(g(i)); }
    template<std::invocable<size_t> G> Fixed(float, unsigned size, G&& g) { for (unsigned i = 0; i < size; ++i) items.push_back(g(i)); }
    template<typename Range, typename Mapper> Fixed(unsigned, Range&& range, Mapper&& mapper) { for (auto& r : range) items.push_back(mapper(r)); }
    template<typename Range, typename Mapper> static Fixed map(unsigned size, Range&& range, Mapper&& mapper) { return Fixed(size, std::forward<Range>(range), std::forward<Mapper>(mapper)); }
};
int main() {
    std::vector<Item> source { { 1, std::nullopt }, { 2, Pct { 7.5 } }, { 3, std::nullopt } };
    double extra = 0.5;
    auto s = Fixed<double>::map(3, source, [&](auto& item) { if (item.p) return item.p->value; return item.percentage * 10 + extra; });
    Fixed<double> t(typename Fixed<double>::Failable { }, 2, [](size_t i) { return i * 2.0; });
    std::printf("%zu %g %g %g %g\n", s.items.size(), s.items[0], s.items[1], s.items[2], t.items[1]);
    return 0;
}
