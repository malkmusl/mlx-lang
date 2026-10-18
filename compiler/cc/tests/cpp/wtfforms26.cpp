// WTF/JSC forms 26: instances made inside lambdas, member template operator*, class template deduction in a range-based for
#include <stdio.h>
#include <concepts>
#include <vector>
#include <type_traits>

namespace p_abbrlam3 {
struct Bits { unsigned v = 0b1011; template<typename F> void forEachSetBit(const F& f) const { for (unsigned i = 0; i < 8; i++) if (v & (1u << i)) f(i); } };
struct Dir {
    std::vector<int*> m_blocks;
    Bits bits() const { return Bits(); }
    void forEachBlock(const std::invocable<int*> auto& functor);
};
struct BlockSet { int count = 5; };
struct Space {
    BlockSet m_blocks;
    Dir dir;
    template<typename F> void forEachBlock(const F&);
    template<typename F> void forEachDirectory(const F& f) { f(dir); }
    int sum() { int s = m_blocks.count; forEachBlock([&](int* p) { s += *p; }); return s; }
};
template<typename F> inline void Space::forEachBlock(const F& functor)
{
    forEachDirectory([&] (Dir& directory) { directory.forEachBlock(functor); return 0; });
}
inline void Dir::forEachBlock(const std::invocable<int*> auto& functor)
{
    bits().forEachSetBit([&] (unsigned index) { functor(m_blocks[index]); });
}
int run() {
    int a = 1, b = 2, c = 3, d = 4; Space sp; sp.dir.m_blocks = { &a, &b, &c, &d };
    printf("%d\n", sp.sum()); return 0;
}
}

namespace p_derefmt {
template<typename T, unsigned A> class PackedPtr {
public:
    PackedPtr(T* p) : m(p) { }
    T* get() const { return m; }
    T* operator->() const { return get(); }
    template<typename U = T> requires (!std::is_void_v<U>) U& operator*() const { return *get(); }
private:
    T* m;
};
template<typename T> class CellPtr : public PackedPtr<T, 8> { public: using Base = PackedPtr<T, 8>; CellPtr(T* p) : Base(p) { } };
struct CB { int v = 9; };
static int show(const CB& c) { return c.v; }
int run() { CB cb; CellPtr<CB> p(&cb); PackedPtr<CB, 8> q(&cb); printf("%d %d %d\n", show(*p), (*q).v, p->v); return 0; }
}

namespace p_ctadfor {
template<typename T> class Checked { public: Checked(T* p) : m(p) { } T* operator->() const { return m; } T& operator*() const { return *m; } private: T* m; };
struct Target { int id; void connect() { id *= 10; } };
int run() {
    Target a { 1 }, b { 2 };
    std::vector<Target*> list { &a, &b };
    int sum = 0;
    for (Checked target : list) { target->connect(); sum += (*target).id; }
    printf("%d\n", sum); return 0;
}
}

int main() {
    p_abbrlam3::run();
    p_derefmt::run();
    p_ctadfor::run();
    return 0;
}
