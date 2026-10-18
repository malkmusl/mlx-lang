// Forms from WTF's headers (3): constexpr calls returning references (std::max), static_assert as a statement, member template packs defined later, parameter attributes, using-declared classes, temporaries as statements, the copy deduction candidate, forwarding reference packs, overloaded member templates defined outside, constrained overloads declared first.
#include <stdio.h>
#include <algorithm>
#include <concepts>
#include <string>
#include <utility>
#include <type_traits>
#include <new>
#include <stddef.h>
#include <limits>
#include <stdint.h>

namespace p_mx1 {
constexpr unsigned long KB = 1024; constexpr unsigned long Ceil = 4 * KB;
constexpr unsigned long Sz = std::max(Ceil, 16 * KB);
static_assert(Sz == 16384);
constexpr int m2 = std::min(3, 7) + std::max<int>(2, -1);
static_assert(m2 == 5);
void run() { printf("%lu %d\n", Sz, m2); }
}

namespace p_mx2 {
constexpr const unsigned long &mx(const unsigned long &a, const unsigned long &b) { return a < b ? b : a; }
constexpr unsigned long mv(const unsigned long &a, const unsigned long &b) { return a < b ? b : a; }
constexpr unsigned long KB = 1024; constexpr unsigned long Ceil = 4 * KB;
constexpr unsigned long A = mv(Ceil, 16 * KB);
static_assert(A == 16384);
constexpr unsigned long B = mx(3, 9);
static_assert(B == 9);
constexpr unsigned long C = mx(Ceil, 16 * KB);
static_assert(C == 16384);
void run() { printf("%lu %lu %lu\n", A, B, C); }
}

namespace p_mx3 {
constexpr const unsigned long &mx(const unsigned long &a, const unsigned long &b) { return a < b ? b : a; }
enum { E1 = mx(3, 9), E2 = mx(9, 3) };
void run() { printf("%d %d\n", (int)E1, (int)E2); }
}

namespace p_sa1 {
template<int N> int f() {
    if constexpr (N == 0) {
        return 1;
    } else
        static_assert(N > 0, "positive");
    if (N > 5) static_assert(sizeof(int) == 4);
    return N;
}
void run() { printf("%d %d\n", f<0>(), f<7>()); }
}

namespace p_cp1 {
template<typename T> concept Small = sizeof(T) <= 8;
template<typename T> struct Ad { T v; Ad(const T& x) : v(x) {} long get() const { return (long)v; } };
struct B {
    long total = 0;
    void add(long x) { total += x; }
    template<typename... As> void fromAdapters(const As&... as) { (add(as.get()), ...); }
    template<Small... Ts> void append(const Ts&...);
    void append(const char*) { total += 1000; }
};
inline void early(B& b) { b.append(5, 6); }
template<Small... Ts> void B::append(const Ts&... strings)
{
    fromAdapters(Ad<Ts>(strings)...);
}
void run() { B b; b.append(1, 2L, (short)3); b.append("x"); early(b); printf("%ld\n", b.total); }
}

namespace p_at1 {
extern "C" {
static void logIt([[maybe_unused]] int* channel, const char* buffer) { fputs(buffer, stdout); }
}
static int two([[maybe_unused]] int a, [[maybe_unused]] int b) { return 2; }
void run() { logIt(nullptr, "hi\n"); printf("%d\n", two(1, 2)); }
}

namespace p_us1 {
namespace W {
class P {
public:
    P(int a, const char* p = "") : a(a), p(p) {}
    void dump() const { printf("%s%d\n", p, a); }
private:
    int a; const char* p;
};
}
using W::P;
extern "C" {
void show(int x, const char* pre) { P { x, pre }.dump(); P(x).dump(); }
}
void run() { show(4, "> "); }
}

namespace p_us3 {
namespace W {
class P {
public:
    P(int a, const char* p = "") : a(a), p(p) {}
    void dump() const { printf("%s%d\n", p, a); }
private:
    int a; const char* p;
};
}
using W::P;
void show1(int x) { P(x).dump(); }
void show2(int x) { P q(x); q.dump(); }
void show3(int x, const char* pre) { P { x, pre }.dump(); }
void run() { show1(1); show2(2); show3(3, "> "); }
}

namespace p_da1 {
struct P {
    P(int a, int b = 7) : a(a), b(b) {}
    int sum(int x, int y = 100) const { return a + b + x + y; }
    int a, b;
};
struct Q { Q(int a, const char* p = "") : a(a), p(p) {} int a; const char* p; };
void run() { P q(1); printf("%d\n", q.sum(1)); Q z(3); printf("%d[%s]\n", z.a, z.p); }
}

namespace p_ct1 {
template<typename T> struct Tr { static const char* n() { return "T"; } };
struct Imp { int v = 5; };
template<typename T, typename D = Tr<T>> class RP {
public:
    RP() : p(nullptr) {}
    RP(T* x) : p(x) {}
    RP(T& x) : p(&x) {}
    RP(const RP& o) : p(o.p) {}
    template<typename X, typename Y> RP(const RP<X, Y>& o) : p(o.get()) {}
    RP(RP&& o) : p(o.p) { o.p = nullptr; }
    T* get() const { return p; }
private:
    T* p;
};
Imp g;
RP<Imp> make() { return RP<Imp>(&g); }
void run() {
    RP r = make();
    printf("%d\n", r.get()->v);
    RP<Imp> q(&g);
    RP s = q;
    printf("%d\n", s.get()->v);
}
}

namespace p_ct2 {
template<typename T, typename D = int> struct RP { T* p; };
RP<int> make() { return RP<int>{nullptr}; }
template<class T> int f(T&) { return 1; }
template<class T, class D> int f(RP<T, D>) { return 2; }
template<class T> int g(T&) { return 1; }
int g(...) { return 3; }
void run() { RP<int> q{nullptr}; printf("%d %d %d %d\n", f(make()), f(q), g(make()), g(q)); }
}

namespace p_ms1 {
struct R { R(const std::string& n) : name(n) {} std::string name; };
template <typename T, class... Args> T* mk(Args &&... args) { return new T(std::forward<Args>(args)...); }
template <typename T, class A> T* mk1(A && a) { return new T(std::forward<A>(a)); }
void run() {
    R* a = mk1<R>(std::string("one"));
    printf("%s\n", a->name.c_str());
    R* b = mk<R>(std::string("two"));
    printf("%s\n", b->name.c_str());
}
}

namespace p_ms2 {
struct S { int v; };
template<class... A> int f(A&&...) { return (0 + ... + (std::is_lvalue_reference_v<A> ? 1 : 0)); }
template<class... A> int g(A&&...) { return (0 + ... + (std::is_rvalue_reference_v<A&&> ? 10 : 0)); }
void run() { int i = 1; S s{1}; printf("%d %d %d %d\n", f(3), f(i), f(s), g(3, s, i)); }
}

namespace p_ms3 {
struct T { int v; };
struct R { R(const T& n) : v(n.v) {} int v; };
template <typename X, class... Args> X* mk(Args &&... args) { return new X(static_cast<Args&&>(args)...); }
template <class... Args> int cnt(Args &&... args) { return sizeof...(args); }
void run() {
    T t{4};
    printf("%d\n", cnt(T{1}));
    printf("%d\n", mk<R>(t)->v);
    printf("%d\n", mk<R>(T{2})->v);
}
}

namespace p_rc1 {
struct S { int v; };
template<class A> int h(A&&) { return std::is_rvalue_reference_v<A&&> ? 10 : 0; }
template<class... A> int g(A&&...) { return (0 + ... + (std::is_rvalue_reference_v<A&&> ? 10 : 0)); }
template<class... A> int k(A&&...) { return (0 + ... + (std::is_lvalue_reference_v<A&> ? 1 : 0)); }
void run() { S s{1}; printf("%d %d %d %d\n", h(s), h(S{2}), g(3, s), k(3, s)); }
}

namespace p_rc2 {
struct S { int v; };
template<class A> int h(A&&) { return std::is_rvalue_reference_v<A&&> ? 10 : 0; }
template<class A> int h2(A&&) { return std::is_lvalue_reference_v<A> ? 1 : 0; }
template<class A> int h3(A&&) { using R = A&&; return std::is_rvalue_reference_v<R> ? 10 : 0; }
void run() { S s{1}; printf("%d %d %d\n", h(s), h2(s), h3(s)); }
}

namespace p_rc3 {
void run() { printf("%d %d %d %d\n", std::is_rvalue_reference_v<int&>, std::is_rvalue_reference_v<int&&>, std::is_lvalue_reference_v<int&&>, std::is_same_v<std::remove_reference_t<int&&>, int>); }
}

namespace p_nd1 {
template<typename T> class ND {
public:
    template<typename... Args> ND(Args&&... args) { MaybeRelax<T>(new (ptr()) T(std::forward<Args>(args)...)); }
    T& get() { return *ptr(); }
private:
    T* ptr() { return reinterpret_cast<T*>(storage); }
    template<typename P, bool R = false> struct MaybeRelax { explicit MaybeRelax(P*) { printf("relax\n"); } };
    alignas(T) unsigned char storage[sizeof(T)];
};
struct V { int a, b; V(int x, int y) : a(x), b(y) {} };
void run() { static ND<V> n(3, 4); printf("%d %d\n", n.get().a, n.get().b); int k = 2; (void)k; }
}

namespace p_ec1 {
enum class FA { Crash, Report };
template<typename T, size_t N> class Vec {
public:
    void grow(size_t s) { growImpl<FA::Crash>(s); }
    template<FA> bool growImpl(size_t size);
    template<FA> bool expand(size_t m);
    template<FA> T* expand(size_t m, T*);
    template<FA, typename U> U* expand(size_t m, U*);
    size_t cap = 0;
};
template<typename T, size_t N>
template<FA a>
bool Vec<T, N>::expand(size_t m) { cap = m; return true; }
template<typename T, size_t N>
template<FA a>
T* Vec<T, N>::expand(size_t m, T* p) { bool ok = expand<a>(m); (void)ok; return p; }
template<typename T, size_t N>
template<FA a, typename U>
U* Vec<T, N>::expand(size_t m, U* p) { bool ok = expand<a>(m); (void)ok; return p; }
template<typename T, size_t N>
template<FA a>
bool Vec<T, N>::growImpl(size_t size) {
    if (size > cap) {
        bool success = expand<a>(size);
        if constexpr (a == FA::Report) { if (!success) return false; }
    }
    return true;
}
void run() { Vec<char, 256> v; v.grow(10); int x; printf("%zu %d\n", v.cap, v.expand<FA::Crash>(3, &x) == &x); }
}

namespace p_ec4 {
enum class FA { Crash, Report };
template<typename T> class Vec {
public:
    template<FA> bool expand(size_t m);
    template<FA> T* expand(size_t m, T*);
    void grow(size_t s) { expand<FA::Crash>(s); }
    size_t cap = 0;
};
template<typename T>
template<FA a>
bool Vec<T>::expand(size_t m) { cap = m; return true; }
template<typename T>
template<FA a>
T* Vec<T>::expand(size_t m, T* p) { cap = m + 1; return p; }
void run() { Vec<char> v; v.grow(10); printf("%zu\n", v.cap); }
}

namespace p_ss1 {
template<std::signed_integral S> S sat(S, S);
template<std::unsigned_integral U> constexpr U sat(U, U);
template<> inline int32_t sat<int32_t>(int32_t a, int32_t b) { return a + b; }
template<std::unsigned_integral U> constexpr U sat(U a, U b) { auto s = a + b; return s < a ? std::numeric_limits<U>::max() : s; }
template<typename I, typename... A> constexpr I sat(I v, A... args) { return sat<I>(v, sat<I>(args...)); }
void run() { unsigned m = 5; printf("%u %u %d\n", sat<uint32_t>(m, 3u, 4u), sat<uint32_t>(0xfffffff0u, 0x20u), sat<int32_t>(1, 2)); }
}

namespace p_ss3 {
template<std::signed_integral S> S sat(S a, S b) { return a + b + 1000; }
template<std::unsigned_integral U> constexpr U sat(U a, U b) { return a + b; }
template<typename I, typename... A> constexpr I sat(I v, A... args) { return sat<I>(v, sat<I>(args...)); }
void run() { printf("%u\n", sat<unsigned>(3u, 4u, 5u)); }
}

int main() {
    p_mx1::run();
    p_mx2::run();
    p_mx3::run();
    p_sa1::run();
    p_cp1::run();
    p_at1::run();
    p_us1::run();
    p_us3::run();
    p_da1::run();
    p_ct1::run();
    p_ct2::run();
    p_ms1::run();
    p_ms2::run();
    p_ms3::run();
    p_rc1::run();
    p_rc2::run();
    p_rc3::run();
    p_nd1::run();
    p_ec1::run();
    p_ec4::run();
    p_ss1::run();
    p_ss3::run();
    return 0;
}
