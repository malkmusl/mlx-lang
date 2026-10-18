// WTF/JSC forms: members of class templates defined after their first use, functions named rather than called, results made in the caller's object (C++17), extern "C" declarations
#include <stdio.h>

namespace p_wt1 {
template<typename T> struct WB { T* p = nullptr; void set(int vm, T* v); T* get() const { return p; } };
template<typename T> struct Counter { Counter(); int n; void bump(); };
struct Proto { int k = 42; };
struct G { WB<Proto> proto; Counter<int> c; void init(Proto* p) { proto.set(1, p); c.bump(); } };
template<typename T> inline void WB<T>::set(int vm, T* v) { p = v; }
template<typename T> Counter<T>::Counter() : n(5) {}
template<typename T> void Counter<T>::bump() { n++; }
int run() { Proto pr; G g; g.init(&pr); printf("%d %d\n", g.proto.get()->k, g.c.n); return 0; }
}

namespace p_sl1 {
struct Base { int (*impl)(void*, int); void* arg; int call(int x) { return impl(arg, x); } };
template<typename F> struct Functor : Base {
    Functor(const F& f) : m_f(f) { impl = &implFunction; arg = this; }
    static int implFunction(void* a, int x) { return static_cast<Functor*>(a)->m_f(x); }
    F m_f;
};
template<typename F> Functor<F> make(const F& f) { return Functor<F>(f); }
struct G { int n; template<typename T> T apply(T x); };
template<typename T> T G::apply(T x) { auto f = make([&](int v) { return v + n; }); return f.call(x); }
int run() { G g{5}; printf("%d\n", g.apply(10)); return 0; }
}

namespace p_sl4 {
struct Base { int (*impl)(void*, int); void* arg; int call(int x) { return impl(arg, x); } };
template<typename F> struct Functor : Base {
    Functor(const F& f) : m_f(f) { impl = &implFunction; arg = this; }
    static int implFunction(void* a, int x) { return static_cast<Functor*>(a)->m_f(x); }
    F m_f;
};
int run(int n, int x) { auto l = [&](int v) { return v + n; }; Functor<decltype(l)> f(l); return f.call(x); }
int run() { printf("%d\n", run(5, 10)); return 0; }
}

namespace p_sl6 {
struct Base { int (*impl)(void*, int); void* arg; int call(int x) { return impl(arg, x); } };
template<typename F> struct Functor : Base {
    Functor(const Functor& o) : m_f(o.m_f) { impl = &implFunction; arg = this; }
    Functor(const F& f) : m_f(f) { impl = &implFunction; arg = this; }
    static int implFunction(void* a, int x) { return static_cast<Functor*>(a)->m_f(x); }
    F m_f;
};
template<typename F> Functor<F> make(const F& f) { return Functor<F>(f); }
int run(int n, int x) { auto f = make([&](int v) { return v + n; }); return f.call(x); }
int run() { printf("%d\n", run(5, 10)); return 0; }
}

namespace p_sl7 {
struct Base { void* arg; int k; };
struct Fn : Base {
    Fn(int v) : m(v) { arg = this; k = 1; }
    Fn(const Fn& o) : m(o.m) { arg = this; k = 2; }
    int m;
};
Fn make(int v) { return Fn(v); }
int run() { Fn f = make(7); printf("%d %d %d\n", f.arg == (void*)&f, f.k, f.m); return 0; }
}

namespace p_ec1 {
namespace N {
extern "C" int g_slots[];
extern "C" int g_count;
extern "C" const int g_k = 7;
int g_slots[4] = { 1, 2, 3, 4 };
int g_count = 5;
}
extern "C" int h(int x) { return x + 1; }
int run() { printf("%d %d %d %d\n", N::g_slots[2], N::g_count, N::g_k, h(1)); return 0; }
}

int main() {
    p_wt1::run();
    p_sl1::run();
    p_sl4::run();
    p_sl6::run();
    p_sl7::run();
    p_ec1::run();
    return 0;
}
