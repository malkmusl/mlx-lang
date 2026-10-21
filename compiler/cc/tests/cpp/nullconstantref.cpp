// A null pointer constant binds a const reference to a pointer (through
// the null pointer it makes): Vector<RenderTableCell *>::fill(0) is
// fill(const T &), not fill(const T &, size_t) (WebCore's AutoTableLayout).
#include <cstdio>
#include <cstddef>
struct Cell { int v; };
template<typename T> struct Vec {
    T items[3];
    size_t m_size = 3;
    size_t size() const { return m_size; }
    void fill(const T& val, size_t newSize) { for (size_t i = 0; i < newSize; ++i) items[i] = val; }
    void fill(const T& val) { fill(val, size()); }
};
int main() {
    Cell c { 1 };
    Vec<Cell*> v;
    v.fill(&c);
    v.fill(0);
    printf("%d\n", v.items[2] == nullptr);
}
