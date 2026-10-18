// Forms from WTF's headers (4): '::template' member alias templates, explicit specializations declared before use, instances made while a signature is read, enum aliases' enumerators, braced default arguments, unique_ptr<T[]>, constrained overloads with explicit packs, lambdas as rvalues, member template overloads told apart by their parameters, deduction of 'const T *' and of function pointers, base member types in qualified names, functions as constexpr arguments, at most one user-defined conversion.
#include <stdio.h>
#include <stddef.h>
#include <optional>
#include <memory>
#include <type_traits>
#include <utility>
#include <concepts>
#include <functional>

namespace p_at2 {
template<typename K, typename V, typename M> struct Table { K k; V v; M m; };
struct Traits;
template<typename T, typename TT = Traits> class Set {
    using TableType = typename TT::template TableType<T, T, long>;
public:
    TableType t;
};
struct Traits { template<typename K, typename V, typename M> using TableType = Table<K, V, M>; };
struct User { Set<int> s; };
void run() { User u; u.s.t.k = 3; printf("%d %zu\n", u.s.t.k, sizeof(u)); }
}

namespace p_ds1 {
template<typename> struct DH;
template<typename T> struct CP { T* p; };
template<> struct DH<CP<int>>;
template<typename P> struct DH<CP<P>> { static int h() { return 1; } };
template<typename T, typename H = DH<T>> struct Set { int g() { return H::h(); } };
using Entry = CP<int>;
struct Table { Set<Entry>* s; };
template<> struct DH<CP<int>> { static int h() { return 2; } };
void run() { Set<Entry> s; Set<CP<long>> t; printf("%d %d\n", s.g(), t.g()); }
}

namespace p_vb3 {
template<typename T> class Base { public: T* p = nullptr; size_t cap = 0; };
template<typename T, size_t N, typename M = int> class Buffer;
template<typename T, typename M> class Buffer<T, 0, M> : private Base<T> { public: size_t c0() const { return 0; } };
template<typename T, size_t N, typename M> class Buffer : private Base<T> { public: size_t c0() const { return N; } T inl[N]; };
template<typename T, size_t N = 0> class Vec : private Buffer<T, N> { public: size_t size() const { return Buffer<T, N>::c0(); } };
using Buf = Vec<unsigned char, 2048>;
class P {
public:
    template<typename C> std::optional<Buf> dom(const C&);
    static Buf dec(int) { return Buf(); }
};
void run() { printf("%zu\n", P::dec(1).size()); }
}

namespace p_ea1 {
enum class TSP : unsigned char { Other, Realtime };
using G = TSP;
class T {
public:
    using SP = TSP;
    bool rt() const { return m == SP::Realtime; }
    SP m { SP::Other };
};
void run() { T t; G g = G::Realtime; t.m = TSP::Realtime; printf("%d %d\n", t.rt(), (int)g); }
}

namespace p_bd1 {
struct Spec { int a = 4; long b = 0; };
struct T { static int make(int x, Spec s = { }) { return x + s.a; } int m(Spec s = {7, 1}) { return s.a + (int)s.b; } };
int g(int x, Spec s = {}, int y = {}) { return x + s.a + y; }
void run() { T t; printf("%d %d %d\n", T::make(1), t.m(), g(2)); }
}

namespace p_ua1 {
struct F { void operator()(void** p) const { printf("free\n"); delete[] p; } };
void run() {
    auto a = std::make_unique<int[]>(4);
    a[2] = 7;
    std::unique_ptr<void*[], F> b(new void*[3]);
    b[1] = &b;
    auto c = std::make_unique<int>(5);
    printf("%d %d %d %d\n", a[2], a[0], b[1] == &b, *c);
}
}

namespace p_ua4 {
template <typename T, class... A> int mk(A&&... a) requires (!std::is_array_v<T>) { return 1; }
template <typename T> int mk(size_t n) requires std::is_array_v<T> { return 2; }
void run() { printf("%d\n", mk<int[]>(4)); printf("%d\n", mk<int>(3)); }
}

namespace p_lm1 {
template<typename C> int f(C&&) { return std::is_rvalue_reference_v<C&&> ? 1 : 0; }
struct S { int x; void go() { printf("%d %d\n", f([this] () { return x; }), f([] { return 1; })); } };
void run() { S s{1}; s.go(); auto l = [] { return 2; }; printf("%d %d\n", f(l), f(std::move(l))); }
}

namespace p_hm1 {
template<typename K, typename M> class Map {
public:
    using KeyType = K;
    template<typename V> int add(const KeyType&, V&&);
    template<typename V> int add(KeyType&&, V&&);
};
template<typename K, typename M>
template<typename T>
auto Map<K, M>::add(const KeyType& key, T&& mapped) -> int { return 1; }
template<typename K, typename M>
template<typename T>
auto Map<K, M>::add(KeyType&& key, T&& mapped) -> int { return 2; }
void run() { Map<int, double> m; int k = 3; printf("%d %d\n", m.add(k, 1.0), m.add(4, 2.0)); }
}

namespace p_inv1 {
enum class SVK : bool { No, Yes };
struct Tr {};
template<typename K> struct Table {
    template<SVK s = SVK::Yes> int add(const K& v) { return add<Tr, s>(v, [&]() { return v; }); }
    template<typename H, SVK> int add(auto&& key, const std::invocable<> auto& functor);
};
template<typename K>
template<typename H, SVK s>
int Table<K>::add(auto&& key, const std::invocable<> auto& functor) { return key + functor(); }
void run() { Table<int> t; printf("%d\n", t.add(4)); }
}

namespace p_iv4 {
template<typename H> void inv(const H*) { printf("inv %zu\n", sizeof(H)); }
template<typename K> struct HT {
    K k;
    template<typename T> int add(T key);
    template<typename T> int add2(T key) { inv(this); return 1; }
    void g();
};
template<typename K> template<typename T> int HT<K>::add(T key) { printf("%zu\n", sizeof(*this)); inv(this); return key; }
template<typename K> void HT<K>::g() { inv(this); }
void run() { HT<int> h; h.g(); h.add2(1); printf("%d\n", h.add(2)); }
}

namespace p_iv6 {
template<typename H> void inv(const H*) { printf("inv %zu\n", sizeof(H)); }
template<typename H> void inw(H*) { printf("inw %zu\n", sizeof(H)); }
template<typename H> void inx(H) { printf("inx %zu\n", sizeof(H)); }
template<typename K> struct HT {
    K k;
    template<typename T> int add2(T key) { inx(this); inw(this); inv(this); return 1; }
};
void run() { HT<int> h; h.add2(1); }
}

namespace p_iv7 {
template<typename H> void inv(const H*) { printf("inv %zu\n", sizeof(H)); }
struct P { int a; void g() { inv(this); } template<class T> void h(T) { inv(this); } };
void run() { int x; inv(&x); P p; p.g(); p.h(1); }
}

namespace p_iv8 {
template<typename H> int pick(const H*) { return 1; }
template<typename H> int pick(H*) { return 2; }
template<typename H> void inv(const H*) { printf("inv %zu\n", sizeof(H)); }
void run() { int x = 0; const int y = 0; printf("%d %d\n", pick(&x), pick(&y)); inv(&x); }
}

namespace p_ft4 {
template<typename Functor, typename Signature> concept Invocable = requires(std::decay_t<Functor>&& f, std::function<Signature> expected) {
    { expected = std::move(f) };
};
template<typename T> struct GB { using TraitType = T; };
template<typename T> struct HTr : GB<T> {};
template<typename K, typename V> struct KVH : GB<int> { typedef K KeyTraits; typedef V ValueTraits; };
template<typename V> struct Tr {
    template<typename U> static int translate(auto& loc, U&& key, const Invocable<typename V::ValueTraits::TraitType()> auto& f) { loc = key + f(); return loc; }
};
template<typename K, typename M> struct Map {
    struct KVT : KVH<HTr<K>, HTr<M>> { static constexpr bool x = true; };
    int add(int& l) { return Tr<KVT>::translate(l, 2, [] { return 5; }); }
};
void run() { int l = 0; Map<int, int> m; printf("%d\n", m.add(l)); }
}

namespace p_pc1 {
template<typename... Args> struct PCI { static constexpr size_t value = sizeof...(Args); };
template<typename R, typename... Args> constexpr size_t pc(R(*)(Args...)) { return PCI<Args...>::value; }
struct FM { static void free(void* p) {} static int two(int, long) { return 0; } };
void g(int, int, int) {}
template<typename M> struct MS { size_t n() { if constexpr (pc(M::free) == 2) return 22; else return pc(M::free); } };
void run() { MS<FM> m; printf("%zu %zu %zu %zu\n", pc(g), pc(FM::two), pc(&FM::free), m.n()); }
}

namespace p_cl1 {
struct Str;
struct View { const char* p; View(const char* s) : p(s) {} View(const Str& s); };
struct Str { const char* p; Str(const char* s) : p(s) {} Str(View v) : p(v.p) {} };
View::View(const Str& s) : p(s.p) {}
bool same(const Str& a, View b) { return a.p == b.p; }
bool operator==(const Str& a, const Str& b) { return a.p[0] == b.p[0]; }
void run() { const char* t = "x"; Str s(t); View v(t); printf("%d %d %d\n", same(s, v), s == Str(v), same(Str(v), s)); }
}

int main() {
    p_at2::run();
    p_ds1::run();
    p_vb3::run();
    p_ea1::run();
    p_bd1::run();
    p_ua1::run();
    p_ua4::run();
    p_lm1::run();
    p_hm1::run();
    p_inv1::run();
    p_iv4::run();
    p_iv6::run();
    p_iv7::run();
    p_iv8::run();
    p_ft4::run();
    p_pc1::run();
    p_cl1::run();
    return 0;
}
