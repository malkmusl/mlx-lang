// A null pointer constant `0` bound to `T *const &` (a temporary), so
// `fill(const T &)` is viable beside `fill(const T &, size_t)`: WTF's
// Vector<RenderTableCell *>::fill(0) in AutoTableLayout.
#include <cstdio>
#include <cstddef>
template<typename T> struct Vec {
    T items[3];
    void fill(const T&, size_t);
    void fill(const T& val) { fill(val, 3); }
};
template<typename T> void Vec<T>::fill(const T& val, size_t n) { for (size_t i = 0; i < n; ++i) items[i] = val; }
struct Cell { };
void take(Cell* const& p) { std::printf("%d\n", p == nullptr); }
int main() {
    Vec<Cell*> v;
    Cell c;
    v.fill(&c);
    std::printf("%d\n", v.items[2] == &c);
    v.fill(0);
    std::printf("%d\n", v.items[2] == nullptr);
    take(0);
    return 0;
}
