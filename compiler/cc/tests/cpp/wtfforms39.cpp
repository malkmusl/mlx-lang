// WTF/JSC forms: late member definitions for specializations, qualified member access by template-id, operator new and operator= member templates, extern constexpr constants, overloaded functions to function pointers
#include <stdio.h>
#include <stddef.h>
#include <utility>

namespace p_wt2 {
struct VM { int w = 0; };
struct Unknown {};
template<class T> struct Tr {};
template<class T, class R = Tr<T>> struct WB { T* p = nullptr; void set(VM&, T* v); };
template<> struct WB<Unknown, Tr<Unknown>> { long v = 0; void set(VM&, long x); };
struct P { int k = 9; };
struct G { WB<P> a; WB<Unknown> b; void init(VM& vm, P* p) { a.set(vm, p); b.set(vm, 4); } };
template<class T, class R> inline void WB<T, R>::set(VM& vm, T* v) { vm.w++; p = v; }
inline void WB<Unknown, Tr<Unknown>>::set(VM& vm, long x) { vm.w += 10; v = x; }
int run() { VM vm; P p; G g; g.init(vm, &p); printf("%d %d %ld\n", vm.w, g.a.p->k, g.b.v); return 0; }
}

namespace p_wt3 {
struct VM { int n = 0; };
struct Base { virtual ~Base() {} };
template<typename K, typename V> class Map : public Base {
public:
    explicit Map(VM&);
    ~Map() final;
    V get() const { return v; }
    V v;
    VM* vm;
};
struct Holder { Map<int, int> m; Holder(VM& vm) : m(vm) {} };
template<typename K, typename V> inline Map<K, V>::Map(VM& vm) : v(5), vm(&vm) { vm.n++; }
template<typename K, typename V> inline Map<K, V>::~Map() { vm->n += 10; }
int run() { VM vm; { Holder h(vm); printf("%d\n", h.m.get()); } printf("%d\n", vm.n); return 0; }
}

namespace p_wt6 {
struct B1 { static int put(int x) { return x + 1; } };
template<class Parent> class CB final : public Parent {
public:
    static int put(CB* self, int x);
    int direct(int x) { return x * 10; }
};
int useEarly(CB<B1>* c) { return CB<B1>::put(c, 3); }
template<class Parent> int CB<Parent>::put(CB* thisObject, int x) {
    return Parent::put(x) + thisObject->CB<Parent>::direct(2);
}
int run() { CB<B1> c; printf("%d\n", useEarly(&c)); return 0; }
}

namespace p_m2 {
struct Arena { char buf[256]; size_t used = 0; template<typename T> void* alloc(size_t n) { void* p = buf + used; used += n + sizeof(T) * 0; return p; } };
struct Deletable {
    virtual ~Deletable() {}
    template<typename T> void* operator new(size_t, Arena&);
};
struct Node : Deletable {
    void* operator new(size_t size, Arena& arena) { return Deletable::operator new<Node>(size, arena); }
    int v; Node(int x) : v(x) {}
};
template<typename T> inline void* Deletable::operator new(size_t size, Arena& arena) { return arena.alloc<T>(size); }
int run() { Arena a; Node* n = new (a) Node(7); printf("%d %d\n", n->v, (int)a.used); return 0; }
}

namespace p_wp1 {
struct Impl {};
template<typename T, typename I = Impl> class WP {
public:
    WP() = default;
    WP(T* p) : m(p) {}
    WP& operator=(const WP&) = default;
    WP& operator=(WP&&) = default;
    template<typename U> WP& operator=(const WP<U, I>&);
    template<typename U> WP& operator=(WP<U, I>&&);
    T* m = nullptr;
    int via = 0;
};
template<typename T, typename I> template<typename U> inline WP<T, I>& WP<T, I>::operator=(const WP<U, I>& o) { m = o.m; via = 1; return *this; }
template<typename T, typename I> template<typename U> inline WP<T, I>& WP<T, I>::operator=(WP<U, I>&& o) { m = o.m; via = 2; return *this; }
struct CC { int k = 3; };
struct DD : CC {};
int run() {
    CC c; DD d; WP<CC> a; WP<CC> b(&c); WP<DD> e(&d);
    a = std::move(b); int v1 = a.via;
    a = std::move(e); int v2 = a.via;
    WP<DD> f(&d); a = f; int v3 = a.via;
    printf("%d %d %d %d\n", v1, v2, v3, a.m->k);
    return 0;
}
}

namespace p_mt1 {
struct SV { int k = 1; };
struct ASV { int k = 2; };
struct Cell {};
struct Table { void (*a)(Cell*, SV&); void (*b)(Cell*, ASV&); int (*c)(int); };
struct Base {
    static void visitChildren(Cell*, SV& v) { v.k += 10; }
    static void visitChildren(Cell*, ASV& v) { v.k += 20; }
    static int other(int x) { return x; }
};
struct Derived : Base {
    static void visitChildren(Cell*, SV& v) { v.k += 100; }
    static void visitChildren(Cell*, ASV& v) { v.k += 200; }
};
struct Info { const char* name; Table t; };
static const Info s_info = { "D", { &Derived::visitChildren, &Derived::visitChildren, &Derived::other } };
static const Info s_info2 = { "B", { &Base::visitChildren, &Base::visitChildren, &Base::other } };
int run() { SV s; ASV a; Cell c; s_info.t.a(&c, s); s_info.t.b(&c, a); s_info2.t.b(&c, a); printf("%d %d %d\n", s.k, a.k, s_info.t.c(4)); return 0; }
}

namespace p_mt2 {
struct SV { int k = 1; };
struct ASV { int k = 2; };
struct Cell {};
struct Derived {
    static void visitChildren(Cell*, SV& v) { v.k += 100; }
    static void visitChildren(Cell*, ASV& v) { v.k += 200; }
};
struct Table { void (*a)(Cell*, SV&); void (*b)(Cell*, ASV&); };
void (*gp)(Cell*, ASV&) = &Derived::visitChildren;
int run() {
    ASV a; Cell c;
    void (*p)(Cell*, ASV&) = &Derived::visitChildren; p(&c, a);
    gp(&c, a);
    Table t = { &Derived::visitChildren, &Derived::visitChildren }; t.b(&c, a);
    printf("%d\n", a.k);
    return 0;
}
}

int main() {
    p_wt2::run();
    p_wt3::run();
    p_wt6::run();
    p_m2::run();
    p_wp1::run();
    p_mt1::run();
    p_mt2::run();
    return 0;
}
