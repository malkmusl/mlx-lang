// WebKit forms 19: JavaScriptCore's (empty braces to an initializer_list constructor, constexpr calls on globals, mem_fn, functions as auto template arguments, pasted literal suffixes, base initializers of the same template)
#include <stdio.h>
#include <initializer_list>
#include <array>
#include <functional>
#include <stddef.h>

namespace p_optset {
enum class Flag : unsigned char { A = 1, B = 2, C = 4 };
struct Set {
    constexpr Set() : bits(0) {}
    constexpr Set(std::initializer_list<Flag> list) : bits(0) { for (Flag f : list) bits |= (unsigned)f; }
    unsigned bits;
};
template<typename E> struct CSet {
    constexpr CSet(Set o) : storage(o.bits) {}
    constexpr CSet(std::initializer_list<E> list) : CSet<E>(Set(list)) {}
    const unsigned storage;
};
using Info = CSet<Flag>;
const Info none = Info { };
const Info one = Info { Flag::A };
const Info two = Info { Flag::B, Flag::C };
int run() { printf("%u %u %u\n", none.storage, one.storage, two.storage); return 0; }
}

namespace p_arrsize {
enum class Unit { Day, Hour, Minute };
template<typename T, size_t N> constexpr std::array<T, N> toArray(const T (&a)[N]) {
    std::array<T, N> r {};
    for (size_t i = 0; i < N; ++i) r[i] = a[i];
    return r;
}
inline constexpr auto units = toArray<Unit>({ Unit::Day, Unit::Hour, Unit::Minute, });
static_assert(units.size() == 3);
struct Box { int v; constexpr int width() const { return 8; } int read() const { return v; } };
Box box { 5 };
static_assert(box.width() == 8);
int run() { printf("%zu %d %d\n", units.size(), (int)units[1], box.read()); return 0; }
}

namespace p_memfn {
struct Dir { int id; Dir *next = nullptr; void setNext(Dir *d) { next = d; } int twice(int k) const { return id * k; } };
template<typename F> void link(F f, Dir *a, Dir *b) { f(a, b); }
int run() {
    Dir a { 1 }, b { 2 };
    link(std::mem_fn(&Dir::setNext), &a, &b);
    auto tw = std::mem_fn(&Dir::twice);
    printf("%d %d %d\n", a.next->id, tw(b, 5), tw(&a, 3));
    return 0;
}
}

namespace p_cplit {
extern "C" {
typedef struct { size_t size; size_t alignment; const char *name; } btype;
}
#define BT_INIT(s, a, n) ((btype){ .size = (s), .alignment = (a), .name = (n) })
struct Block { static constexpr size_t blockSize = 16384; };
static const btype heapType { BT_INIT(Block::blockSize, Block::blockSize, "Structure Heap") };
static const btype other = BT_INIT(8, 8, "x");
extern "C" void my_close(int *p) { printf("close %d\n", *p); }
template<typename F> struct Traits;
template<typename R, typename A> struct Traits<R (*)(A)> { using Arg = A; };
template<auto fn> struct Deleter { void operator()(typename Traits<decltype(fn)>::Arg p) { if (p) fn(p); } };
int run() {
    using D = Deleter<my_close>;
    int v = 7;
    D d; d(&v);
    printf("%zu %zu %s %s\n", heapType.size, heapType.alignment, heapType.name, other.name);
    return 0;
}
}

namespace p_pasteudl {
struct Lit { const char *p; size_t n; };
constexpr Lit operator""_s(const char *p, size_t n) { return Lit { p, n }; }
#define NAME(x) #x ## _s
#define EACH(m) m(alpha) m(beta)
#define SHOW(x) printf("%s %zu\n", NAME(x).p, NAME(x).n);
int run() { EACH(SHOW) return 0; }
}

namespace p_basetmpl {
template<typename T> struct Adapter;
template<> struct Adapter<const char *> { Adapter(const char *s) : s(s) {} const char *s; };
struct Lit { const char *p; };
template<> struct Adapter<Lit> : Adapter<const char *> { Adapter(Lit l) : Adapter<const char *> { l.p } {} };
int run() { Adapter<Lit> a(Lit { "hi" }); printf("%s\n", a.s); return 0; }
}

int main() {
    p_optset::run();
    p_arrsize::run();
    p_memfn::run();
    p_cplit::run();
    p_pasteudl::run();
    p_basetmpl::run();
    return 0;
}
