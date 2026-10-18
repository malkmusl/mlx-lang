// WebKit JSC forms: nested dependent names, operator lookup past class members, static member templates, local classes in late bodies, requires in instances, typedef over enum, base pointer to overloads
#include <stdio.h>

namespace p_tn1 {
template <class T, int N> struct Vec { T a[N]; int size() const { return N; } };
struct Stream { using Offset = unsigned; };
template <class Tr> struct Block { using InstructionStreamType = Stream; };
struct Traits { using OpcodeID = int; };
template <class Tr>
static int count(typename Tr::OpcodeID id, const Vec<typename Block<Tr>::InstructionStreamType::Offset, 4>& v, unsigned off)
{
    return id + v.size() + (int)off + (int)sizeof(v.a[0]);
}
int run() {
    Vec<unsigned, 4> v;
    printf("%d\n", count<Traits>(1, v, 2));
    return 0;
}
}

namespace p_op1 {
namespace N {
struct Id { int v; explicit operator bool() const { return v != 0; } };
struct W { int w; };
bool operator!(const W& w) { return w.w == 0; }
class PS {
public:
    int m;
    bool operator!() const { return m == 0; }
    static int compute(Id id, W w) { if (!id) return 1; if (!w) return 2; return 3; }
    int self() const { return !*this ? 7 : 8; }
};
}
int run() {
    N::PS p { 0 };
    printf("%d %d %d %d\n", N::PS::compute(N::Id { 0 }, N::W { 1 }), N::PS::compute(N::Id { 1 }, N::W { 0 }), N::PS::compute(N::Id { 1 }, N::W { 1 }), p.self());
    return 0;
}
}

namespace p_sm1 {
struct G {
    int k;
    template <class N, class U> static int generate(int a, N* n, U* u) { G g { a }; unsigned s; return g.generate(s) + *n + *u; }
    int generate(unsigned& s) { s = 1; return k; }
};
static int run(int a) {
    int n = 2; long u = 3;
    int r = G::generate(a, &n, &u);
    return r;
}
int run() { printf("%d\n", run(10)); return 0; }
}

namespace p_sm7 {
struct B {
    virtual int f(int) { return 1; }
    template <class T> int f(T*, T*) { return 5; }
    virtual ~B() { }
};
struct D : B {
    int f(int) override { return 2 + B::f(0); }
};
int run() { D d; B& b = d; int x; printf("%d %d\n", b.f(0), d.B::f(&x, &x)); return 0; }
}

namespace p_lc5 {
template <class T> auto makeSum(T base)
{
    constexpr int batch = 3;
    struct Task {
        T b;
        int sum() const { int s = 0; for (int i = 0; i < batch; ++i) s += b + i; return s; }
    };
    return Task { base };
}
struct Holder {
    decltype(makeSum(1)) task = makeSum(10);
    int get() const { return task.sum(); }
};
int run() { Holder h; printf("%d\n", h.get()); return 0; }
}

namespace p_rq1 {
template <class T> struct Traits { static constexpr bool weak = false; };
template <class T> struct Table {
    T v[2];
    int size() const requires (Traits<T>::weak) { return 99; }
    int count() const { return 2; }
};
struct Set {
    template <class F> int each(const F& f, const Table<int>& t);
};
template <class F> int Set::each(const F& f, const Table<int>& t) { int s = 0; for (int i = 0; i < t.count(); ++i) s += f(t.v[i]); return s; }
int run() { Set s; Table<int> t { { 3, 4 } }; printf("%d\n", s.each([](int x) { return x * 2; }, t)); return 0; }
}

namespace p_en1 {
namespace J { enum class HashTableType { Key, KeyValue }; }
namespace W {
template <typename HashTable> struct It {
    using HashTableType = HashTable;
    static int probe(int x) { return HashTableType::isEmpty(x) ? 1 : 2; }
};
struct HT { static bool isEmpty(int x) { return x == 0; } };
}
namespace J {
using namespace W;
int run() { return It<HT>::probe(0) * 10 + It<HT>::probe(3) + (int)HashTableType::KeyValue * 100; }
}
int run() { printf("%d\n", J::run()); return 0; }
}

namespace p_hs4 {
template <class V> struct Set {
    int n = 0;
    int add(const V&) { return ++n; }
    int add(V&&) { return ++n; }
    template <class Tr, class T> int add(const T&) { return 100; }
};
struct Base { int b = 1; };
template <class T, int N> struct Vec : Base {
    Set<Base*>* m_set = nullptr;
    int fill(Set<Base*>& s) { m_set = &s; return m_set->add(this); }
};
int run() { Set<Base*> s; Vec<int, 8> v; printf("%d\n", v.fill(s)); return 0; }
}

int main() {
    p_tn1::run();
    p_op1::run();
    p_sm1::run();
    p_sm7::run();
    p_lc5::run();
    p_rq1::run();
    p_en1::run();
    p_hs4::run();
    return 0;
}
