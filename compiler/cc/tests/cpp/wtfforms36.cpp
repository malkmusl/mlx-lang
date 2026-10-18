// WebKit JSC forms: value-initialization zeroes, member requires clauses in instances, initializer_list from braces, member template specializations of instances, defaults from out-of-class definitions, waiting constructor templates, converting tuples, defaulted class templates, using-declared overloads, empty braces, const this in overloads
#include <stdio.h>
#include <utility>
#include <initializer_list>
#include <tuple>
#include <cmath>
#include <variant>

namespace p_vi3 {
class LE { public: LE() = default; LE(long o) : m(o) { } long base() const { return m; } private: long m; };
static long dirty() { volatile long junk[16]; for (int i = 0; i < 16; ++i) junk[i] = 0x5555 + i; return junk[3]; }
static long f() { LE e = LE(); LE g{}; return e.base() + g.base(); }
int run() { dirty(); printf("%ld\n", f()); return 0; }
}

namespace p_rq3 {
enum class Rev { Disabled, Enabled };
template <typename C, Rev rev> class LP {
public:
    int base() const { return 1; }
    int entry(int k) requires (rev == Rev::Disabled) { return helper(k); }
    int helper(int k) requires (rev == Rev::Disabled);
    int other(int k) requires (rev == Rev::Enabled) { return k * 1000; }
};
template <typename C, Rev rev>
int LP<C, rev>::helper(int k) requires (rev == Rev::Disabled) { return k * 2; }
template class LP<char, Rev::Enabled>;
template class LP<char, Rev::Disabled>;
int run() { LP<char, Rev::Disabled> p; LP<char, Rev::Enabled> q; printf("%d %d\n", p.entry(3), q.other(2)); return 0; }
}

namespace p_rq4 {
template <class T, unsigned long Inline> class SV {
    static constexpr bool hasInline = Inline > 0;
public:
    SV() = default;
    SV(SV&&) requires (hasInline) = delete;
    SV(SV&& other) requires (!hasInline) : m_size(std::exchange(other.m_size, 0)), m_seg{other.m_seg} { }
    int m_size = 3; int m_seg = 4;
};
int run() { SV<int, 0> a; SV<int, 0> b(std::move(a)); SV<int, 2> c; printf("%d %d %d\n", a.m_size, b.m_size, c.m_seg); return 0; }
}

namespace p_il2 {
struct CR { char32_t b, e; };
static int count(std::initializer_list<char32_t> a, std::initializer_list<CR> r) { int n = (int)a.size() * 100; for (auto& x : r) n += (int)(x.e - x.b); return n; }
int run() { printf("%d\n", count(std::initializer_list<char32_t>({}), std::initializer_list<CR>({{0x00, 0xff}, {0x100, 0x110}}))); return 0; }
}

namespace p_es4 {
struct VA { int a = 1; }; struct VB { int b = 2; };
template <class K> struct Bucket { };
struct KeyOnly { }; struct KeyValue { };
template <class B> struct Impl {
    template <class V> static int visit(int* cell, V& v);
};
template <class B> template <class V> int Impl<B>::visit(int* cell, V& v) { return 100; }
template <>
template <>
int Impl<Bucket<KeyOnly>>::visit(int*, VA&) { return 1; }
template <>
template <>
int Impl<Bucket<KeyOnly>>::visit(int*, VB&) { return 2; }
int run() { VA a; VB b; int c = 0; printf("%d %d %d\n", Impl<Bucket<KeyOnly>>::visit(&c, a), Impl<Bucket<KeyOnly>>::visit(&c, b), Impl<Bucket<KeyValue>>::visit(&c, a)); return 0; }
}

namespace p_dd1 {
struct D { int dump(int out, int depth); int run(); };
int D::dump(int out, int depth = 5) { return out * 10 + depth; }
int D::run() { return dump(3); }
int run() { D d; printf("%d %d\n", d.run(), d.dump(1)); return 0; }
}

namespace p_ct6 {
struct VM { int id = 7; };
struct Cell { int v; Cell(VM& vm, int* s) : v(vm.id + *s) { } };
struct Decoder { VM m; VM& vm() { return m; } };
template <class T> struct Cached { int regs() const { return T::k; } };
#define ALWAYS_INLINE inline __attribute__((always_inline))
class UCB : public Cell {
    typedef Cell Base;
public:
    int m_regs;
protected:
    UCB(VM& vm, int* s) : Base(vm, s), m_regs(0) { }
    template <typename CB> UCB(Decoder&, int*, const Cached<CB>&);
};
class UFCB : public UCB {
public:
    static constexpr int k = 3;
    UFCB(Decoder& d, int* s, const Cached<UFCB>& c) : UCB(d, s, c) { }
};
template <typename CB>
UCB::UCB(Decoder& decoder, int* s, const Cached<CB>& cached)
    : Base(decoder.vm(), s)
    , m_regs(cached.regs())
{
}
int run() { Decoder d; int s = 1; Cached<UFCB> c; UFCB f(d, &s, c); printf("%d %d\n", f.v, f.m_regs); return 0; }
}

namespace p_tp1 {
struct PN { int v; PN(int x) : v(x) { } };
struct CI { int c; };
static std::tuple<int*, int*> two(int* a, int* b) { return { a, b }; }
int run() {
    int x = 1, y = 2; int* p; int* q;
    std::tie(p, q) = two(&y, &x);
    std::tuple<PN, const CI*> t = std::tuple<PN, std::nullptr_t>(PN(5), nullptr);
    std::tuple<long, double> u = std::make_tuple(3, 2.5f);
    printf("%d %d %d %d %ld %.1f\n", *p, *q, std::get<0>(t).v, std::get<1>(t) == nullptr, std::get<0>(u), std::get<1>(u));
    return 0;
}
}

namespace p_ps1 {
struct Small { static constexpr int n = 4; };
struct Large { static constexpr int n = 64; };
template <class X = Small> class PreciseSum { public: int size() const { return X::n; } };
template <class S> static int calc(S& s) { return s.size(); }
int run() { PreciseSum<Large> big; PreciseSum sum; return printf("%d %d\n", calc(big), calc(sum)) < 0; }
}

namespace p_ex1 {
namespace J { namespace Math { using std::exp; using std::log; } }
int run() { printf("%.4f %.4f %.4f\n", J::Math::exp(1.0), J::Math::log(2.0), J::Math::exp(1.0f)); return 0; }
}

namespace p_cp1 {
struct Payload {
    using DataType = std::variant<int, double>;
    Payload(Payload&& o) : d(o.d) { }
    ~Payload() { }
    explicit Payload(DataType&& x) : d(x) { }
    static Payload makeEmpty() { return Payload({ }); }
    DataType d;
};
int run() { auto p = Payload::makeEmpty(); printf("%d\n", (int)p.d.index()); return 0; }
}

namespace p_cs1 {
template <class C, class I> struct Range { const C& c; I b, e; Range(const C& cc, I bb, I ee) : c(cc), b(bb), e(ee) { } };
template <class C, class I> Range<C, I> makeRange(const C& c, I b, I e) { return Range<C, I>(c, b, e); }
struct M {
    struct VIt { int* p; }; struct CVIt { const int* p; };
    struct It { int* p; VIt values() const { return VIt { p }; } };
    struct CIt { const int* p; CVIt values() const { return CVIt { p }; } };
    int d[2] = { 1, 2 };
    It begin() { return It { d }; } It end() { return It { d + 2 }; }
    CIt begin() const { return CIt { d }; } CIt end() const { return CIt { d + 2 }; }
    using VRange = Range<M, VIt>; using CVRange = Range<M, CVIt>;
    VRange values() { return makeRange(*this, begin().values(), end().values()); }
    const CVRange values() const { return makeRange(*this, begin().values(), end().values()); }
};
int run() { const M m; auto r = m.values(); printf("%d\n", (int)(r.e.p - r.b.p)); return 0; }
}

int main() {
    p_vi3::run();
    p_rq3::run();
    p_rq4::run();
    p_il2::run();
    p_es4::run();
    p_dd1::run();
    p_ct6::run();
    p_tp1::run();
    p_ps1::run();
    p_ex1::run();
    p_cp1::run();
    p_cs1::run();
    return 0;
}
