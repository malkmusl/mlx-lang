// WebKit JSC forms: parenthesized comparisons in template arguments, constructors inherited through alias templates, overloaded function names to function pointers, range-for with an init statement, auto without the initializer's const, ADL bringing a function template
#include <stdio.h>
#include <type_traits>
#include <vector>
#include <stdint.h>

namespace p_bs4 {
template<unsigned long size> using WT = std::conditional_t<(size <= 32 && sizeof(unsigned long) > sizeof(unsigned)), unsigned, unsigned long>;
template<unsigned long N, typename W = WT<N>> struct BS { W w; };
BS<256> g;
template<typename C> int enc(const BS<256>& b, C c) { return (int)sizeof(b.w) + (int)c; }
int run() { printf("%d %d\n", (int)sizeof(WT<256>), enc(g, 1)); return 0; }
}

namespace p_bs11 {
template<bool B> struct Z { int z = B; };
template<typename C> int enc2(const Z<(8 > 4)>& b, C c) { return b.z + (int)c; }
int run() { Z<true> z; printf("%d\n", enc2(z, 1)); return 0; }
}

namespace p_ih4 {
namespace mp { template <class... T> struct var { int idx = -1; var() = default; template <class U> var(U* u) : idx(sizeof(U)) { } int index() const { return idx; } }; }
template <class... T> using Var = mp::var<T...>;
struct A { int a; }; struct B { int b; long c; };
struct Ref : Var<A*, B*> { using Var<A*, B*>::Var; };
static Ref make(B* b) { return b; }
int run() { B b; printf("%d\n", make(&b).index()); return 0; }
}

namespace p_ov1 {
void enableSuper() { }
static long enableSuper(int* g, char* f) { return 42; }
template <class Out, class... In> struct FPtr { Out (*p)(In...); constexpr FPtr(Out (*ptr)(In...)) : p(ptr) { } };
using NativeFunction = FPtr<long, int*, char*>;
static long call(NativeFunction f) { return f.p(nullptr, nullptr); }
int run() { long (*q)(int*, char*) = enableSuper; printf("%ld %ld\n", call(enableSuper), q(nullptr, nullptr)); return 0; }
}

namespace p_rf1 {
int run() { std::vector<int> v { 5, 6, 7 }; int s = 0; for (unsigned index = 0; const int& x : v) s += x * (int)index++; for (int k = 1; auto y : v) s += y * k; printf("%d\n", s); return 0; }
}

namespace p_au4 {
template <int N> constexpr bool divi(uint32_t& n) { n /= 10; return N > 0; }
template <int K, class T> constexpr T power(T a) { T r = 1; for (int i = 0; i < K; ++i) r *= a; return r; }
template <class F> struct Impl {
    static constexpr int kappa = 2;
    static uint32_t delta() { return 40; }
    static uint32_t run(uint32_t zi) {
        auto const deltai = delta();
        constexpr auto small_divisor = power<kappa>(static_cast<uint32_t>(10));
        auto r = static_cast<uint32_t>(zi);
        auto dist = r - (deltai / 2) + (small_divisor / 2);
        bool const b = divi<kappa>(dist);
        return dist + b;
    }
};
int run() { printf("%u\n", Impl<int>::run(25)); return 0; }
}

namespace p_mk2 {
namespace W {
struct Lit { const char* s; };
template <class... Ts> int makeString(const Ts&... s) { return (int)sizeof...(Ts); }
}
namespace J { namespace Wasm {
struct ION { int i; };
int makeString(const ION& ion) { if (ion.i) return makeString(W::Lit { "a" }, ion.i, W::Lit { "b" }); return -1; }
} }
int run() { printf("%d\n", J::Wasm::makeString(J::Wasm::ION { 2 })); return 0; }
}

int main() {
    p_bs4::run();
    p_bs11::run();
    p_ih4::run();
    p_ov1::run();
    p_rf1::run();
    p_au4::run();
    p_mk2::run();
    return 0;
}
