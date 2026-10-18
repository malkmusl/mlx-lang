// WebKit JSC forms: deduced struct results, explicit specializations by reference, enumerators beside a type of the name, trivially copyable classes, implicit move on return, conversion to a base reference, captures through nested lambdas, the FOR_EACH macro trick, invoke of member pointers, argument-dependent lookup beside a template
#include <stdio.h>
#include <array>
#include <utility>
#include <type_traits>
#include <compare>
#include <vector>
#include <functional>

namespace p_ar5 {
template <size_t... Is> constexpr auto make(std::index_sequence<Is...>) { return std::array<long, sizeof...(Is)> { (long)(Is * 3)... }; }
template <size_t N> constexpr auto names() { return make(std::make_index_sequence<N> { }); }
auto plain() { std::array<int, 8> a { }; for (int i = 0; i < 8; ++i) a[i] = i * i; return a; }
int run() { auto a = names<5>(); auto b = plain(); auto l = [](int k) { std::array<int, 6> r { }; r[5] = k; return r; }; printf("%ld %ld %d %d %d\n", a[4], a[2], b[7], (int)a.size(), l(9)[5]); return 0; }
}

namespace p_es1 {
struct V { int x = 7; void clear() { x = 0; } };
template<typename T> void clearElement(T& element) { element.clear(); }
template<> void clearElement(double& element);
template<> void clearElement<int>(int& element) { element = -1; }
template <class T> void clearAll(T* a, int n) { for (int i = 0; i < n; ++i) clearElement(a[i]); }
int run() { V v[2]; double d[2] = { 1, 2 }; int k[1] = { 5 }; clearAll(v, 2); clearAll(d, 2); clearAll(k, 1); printf("%d %g %d\n", v[1].x, d[1], k[0]); return 0; }
template<> void clearElement(double& element) { element = 0.5; }
}

namespace p_en2 {
class EI {
public:
    enum class FieldID : unsigned char { InstPC, Divot, Start };
    using InstPC = unsigned;
    struct Diff;
    static int run(int k);
};
struct EI::Diff {
    using FieldID = EI::FieldID;
    InstPC instPC { 0 };
    template <unsigned bits> void set(FieldID f, unsigned v) {
        switch (f) {
        case FieldID::InstPC: instPC += v * bits; break;
        case FieldID::Divot: instPC += 100; break;
        default: break;
        }
    }
};
int EI::run(int k) { Diff d; d.set<2>(FieldID::InstPC, k); d.set<1>(FieldID::Divot, 0); return d.instPC; }
int run() { printf("%d\n", EI::run(5)); return 0; }
}

namespace p_tc1 {
struct D { };
class BI {
public:
    BI() = default;
    BI(D) : m_bits(1) { }
    explicit BI(unsigned off, int cp = 0) : m_bits(off << 2 | cp) { }
    friend auto operator<=>(const BI&, const BI&) = default;
    unsigned offset() const { return m_bits >> 2; }
private:
    unsigned m_bits { 0xffffffff };
};
class A { public: A() = default; private: unsigned x { 3 }; };
class B { public: friend bool operator==(const B&, const B&) = default; int y; };
int run() { printf("%d %d %d\n", (int)std::is_trivially_copyable_v<BI>, (int)std::is_trivially_copyable_v<A>, (int)std::is_trivially_copyable_v<B>); return 0; }
}

namespace p_rp1 {
struct Base { int k = 1; virtual ~Base() { } };
struct Arr : Base { int n = 5; };
template <class T> struct Ref {
    T* p;
    explicit Ref(T* x) : p(x) { }
    Ref(Ref&& o) : p(o.p) { o.p = nullptr; }
    template <class X> Ref(Ref<X>&& o) : p(o.p) { o.p = nullptr; }
    ~Ref() { }
};
template <class T> struct RefPtr {
    T* p = nullptr;
    RefPtr(std::nullptr_t) { }
    template <class X> RefPtr(Ref<X>&& o) : p(o.p) { o.p = nullptr; }
    RefPtr(RefPtr&& o) : p(o.p) { o.p = nullptr; }
};
static RefPtr<Base> make(bool b) {
    if (!b)
        return nullptr;
    Ref<Arr> arr(new Arr);
    return arr;
}
int run() { auto r = make(true); printf("%d\n", static_cast<Arr*>(r.p)->n); return 0; }
}

namespace p_cv1 {
struct VM { VM(int) { } };
struct Uniq { int u = 1; };
struct Sym : Uniq { int s = 2; };
struct RegSym : Sym { int r = 3; };
struct PrivName { Sym* p; explicit PrivName(Sym& s) : p(&s) { } };
template <class T> struct Ref { T* p; Ref(T& x) : p(&x) { } operator T&() const { return *p; } T* operator->() const { return p; } };
struct Ident {
    int v;
    static Ident fromUid(VM&, Uniq* u) { return Ident { u->u }; }
    static Ident fromUid(const PrivName& n) { return Ident { 100 + n.p->s }; }
    static Ident fromUid(Sym& s) { return Ident { 10 + s.s }; }
};
int run() { RegSym rs; Ref<RegSym> symbol(rs); printf("%d\n", Ident::fromUid(symbol).v); return 0; }
}

namespace p_lm5 {
namespace W { template <class A, class B> bool equal(const A& a, const B& b) { return false; } }
using W::equal;
template <class F> void call1(F f) { f(1); }
struct E {
    std::vector<int> v;
    bool same(const E& other) const {
        auto equal = [&] (const std::vector<int>& a, const std::vector<int>& b) { return a.size() == b.size(); };
        bool result = false;
        call1([&] (int) { call1([&] (int) { result = equal(v, other.v); }); });
        return result;
    }
};
int run() { E a { { 1, 2 } }, b { { 3, 4 } }; printf("%d\n", (int)a.same(b)); return 0; }
}

namespace p_fe3 {
#define WTF_PARENS ()
#define WTF_EXPAND(...) WTF_EXPAND4(WTF_EXPAND4(WTF_EXPAND4(WTF_EXPAND4(__VA_ARGS__))))
#define WTF_EXPAND4(...) WTF_EXPAND3(WTF_EXPAND3(WTF_EXPAND3(WTF_EXPAND3(__VA_ARGS__))))
#define WTF_EXPAND3(...) WTF_EXPAND2(WTF_EXPAND2(WTF_EXPAND2(WTF_EXPAND2(__VA_ARGS__))))
#define WTF_EXPAND2(...) WTF_EXPAND1(WTF_EXPAND1(WTF_EXPAND1(WTF_EXPAND1(__VA_ARGS__))))
#define WTF_EXPAND1(...) __VA_ARGS__
#define WTF_FOR_EACH_HELPER(macro, a1, ...) macro(a1) __VA_OPT__(, WTF_FOR_EACH_AGAIN WTF_PARENS (macro, __VA_ARGS__))
#define WTF_FOR_EACH_AGAIN() WTF_FOR_EACH_HELPER
#define WTF_FOR_EACH(macro, ...) __VA_OPT__(WTF_EXPAND(WTF_FOR_EACH_HELPER(macro, __VA_ARGS__)))
namespace N { static int twice(int x) { return 2 * x; } }
#define SAFE_PRINTF_TYPE(...) WTF_FOR_EACH(N::twice, __VA_ARGS__)
#define SAFE_PRINTF(format, ...) printf(format __VA_OPT__(, SAFE_PRINTF_TYPE(__VA_ARGS__)))
int run() { int a = 1;
    SAFE_PRINTF("%d %d %d %d %d %d\n",
        a,
        2,
        3,
        static_cast<int>(4),
        a + 4,
        a); SAFE_PRINTF("x\n"); return 0; }
}

namespace p_fn1 {
struct Val { int k; int asArray() { return k * 2; } int plus(int a) const { return k + a; } };
struct BI { int v; explicit BI(int x) : v(x) { } };
struct CB { int off(int* p) const { return *p + 1; } };
struct CSI { int i; explicit CSI(BI b) : i(b.v) { } };
int run() {
    std::function<int(Val&)> f = &Val::asArray;
    std::function<int(const Val*, int)> g = &Val::plus;
    Val v { 4 };
    CB cb; CB* codeBlock = &cb; int pc = 9;
    CSI csi(BI(codeBlock->off(&pc)));
    printf("%d %d %d %d\n", f(v), g(&v, 3), std::invoke(&Val::plus, v, 1), csi.i);
    return 0;
}
}

namespace p_mk1 {
namespace W {
template <class T> struct Adapter;
template <> struct Adapter<int> { int v; Adapter(int x) : v(x) { } int length() const { return 1; } };
template <class T> concept Adaptable = requires { sizeof(Adapter<T>); };
template <Adaptable... Ts> int makeString(const Ts&... s) { return (Adapter<Ts>(s).length() + ...); }
}
using W::makeString;
namespace J {
namespace Wasm {
struct IndexOrName { int i; };
inline int makeString(const IndexOrName& n) { return 100 + n.i; }
}
struct Visitor {
    Wasm::IndexOrName m { 7 };
    int line() const { return makeString(m); }
    int qual() const { return Wasm::makeString(m); }
};
}
int run() { J::Visitor v; printf("%d %d %d\n", v.line(), v.qual(), makeString(5)); return 0; }
}

int main() {
    p_ar5::run();
    p_es1::run();
    p_en2::run();
    p_tc1::run();
    p_rp1::run();
    p_cv1::run();
    p_lm5::run();
    p_fe3::run();
    p_fn1::run();
    p_mk1::run();
    return 0;
}
