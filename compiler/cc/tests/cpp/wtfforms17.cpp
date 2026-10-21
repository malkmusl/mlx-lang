// WebKit forms 17: JavaScriptCore's (deleters, move iterators, late member templates, braced member initializers)
#include <stdio.h>
#include <stdlib.h>
#include <memory>

namespace p_updel {
struct Seg { int v; };
template<typename T> struct NonDestructing { void operator()(T *p) const { printf("free %d\n", p->v); free(p); } };
struct Counting { int *count; void operator()(int *p) const { ++*count; delete p; } };
struct Base { virtual ~Base() { printf("~Base\n"); } };
struct Derived : Base { ~Derived() override { printf("~Derived\n"); } };
static void byFunction(Seg *p) { printf("fn %d\n", p->v); delete p; }
int run() {
    using SegmentPtr = std::unique_ptr<Seg, NonDestructing<Seg>>;
    Seg *raw = static_cast<Seg *>(malloc(sizeof(Seg)));
    raw->v = 7;
    SegmentPtr a(raw, { });
    printf("%d %d\n", (int)sizeof(SegmentPtr), (int)sizeof(std::unique_ptr<int>));
    SegmentPtr b(std::move(a));
    printf("%d %d\n", a.get() == nullptr, b->v);
    int count = 0;
    {
        std::unique_ptr<int, Counting> c(new int(3), Counting { &count });
        c.reset(new int(4));
        std::unique_ptr<int, Counting> d(std::move(c));
        printf("%d %d\n", count, (int)sizeof(d));
    }
    printf("%d\n", count);
    std::unique_ptr<Seg, void (*)(Seg *)> e(new Seg { 9 }, byFunction);
    std::unique_ptr<int[]> arr(new int[3]());
    arr[1] = 5;
    printf("%d\n", arr[1]);
    { std::unique_ptr<Base> base = std::unique_ptr<Derived>(new Derived); }
    return 0;
}
}

namespace p_latemt {
template<typename T> struct Aux {
    template<typename U> void set(int, U &&);
    T m_value {};
};
struct Holder;
inline void use(Holder &h);
struct Holder { Aux<int> a; Aux<long> b; };
inline void use(Holder &h) { h.a.set(1, 5); }
template<typename T> struct Box { Aux<T> aux; void put(T v) { aux.set(0, v); } };
template<typename T> template<typename U> void Aux<T>::set(int k, U &&v) { m_value = v + k; }
int run() {
    Holder h;
    use(h);
    h.b.set(2, 40L);
    Box<short> box;
    box.put(3);
    printf("%d %ld %d\n", h.a.m_value, h.b.m_value, box.aux.m_value);
    return 0;
}
}

namespace p_unionmi {
typedef long (*Fn)(int);
static long twice(int x) { return x * 2; }
static long thrice(int x) { return x * 3; }
struct Value {
    enum AccessorTag { Accessor };
    enum ConstantTag { Constant };
    union Storage {
        constexpr Storage(AccessorTag, Fn getter, Fn setter) : accessor({ getter, setter, }) { }
        constexpr Storage(ConstantTag, long long c) : constant({ c, 0, }) { }
        struct { Fn getter; Fn setter; } accessor;
        struct { long long value; long long unused; } constant;
    } storage;
};
int run() {
    Value a { Value::Storage(Value::Accessor, twice, thrice) };
    Value b { Value::Storage(Value::Constant, 42) };
    printf("%ld %ld %lld\n", a.storage.accessor.getter(4), a.storage.accessor.setter(4), b.storage.constant.value);
    return 0;
}
}

namespace p_structmi {
struct P { int a, b; };
struct S { S(int x) : p({ x, x + 1 }) {} P p; };
int run() { S s(3); printf("%d %d\n", s.p.a, s.p.b); return 0; }
}

namespace p_aggit {
template<typename IteratorType> struct AddRes { IteratorType iterator; bool isNewEntry; };
template<typename T> class Map {
public:
    class iterator {
    public:
        iterator() = default;
        iterator(T *c, T *e) : m_current(c), m_end(e) {}
        T &operator*() const { return *m_current; }
    private:
        T *m_current { nullptr };
        T *m_end { nullptr };
    };
    using AddResult = AddRes<iterator>;
    template<typename K> AddResult add(K &&k) {
        for (unsigned i = 0; i < 2; ++i)
            if (data[i] == k) return { iterator { &data[i], data + 2 }, false };
        return { iterator { &data[1], data + 2 }, true };
    }
    T data[2] = { 4, 5 };
};
int run() {
    Map<int> m;
    auto r = m.add(5);
    auto q = m.add(9);
    printf("%d %d %d %d\n", *r.iterator, (int)r.isNewEntry, *q.iterator, (int)q.isNewEntry);
    return 0;
}
}

int main() {
    p_updel::run();
    p_latemt::run();
    p_unionmi::run();
    p_structmi::run();
    p_aggit::run();
    return 0;
}
