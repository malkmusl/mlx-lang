// WTF/JSC forms 23: inherited typedefs in lambdas and member template instances, local types in lambdas, if constexpr skipping do-while
#include <stdio.h>

namespace p_lamtypedef {
template<typename T> struct Table { using Storage = T; using Helper = Table<T>; static int find(Storage& s, int k) { return s.v + k; } Storage* m_storage = nullptr; Storage& storageRef() { return *m_storage; } };
struct Traits { int v = 5; };
struct Map : Table<Traits> {
    using Base = Table<Traits>;
    template<typename F> int getImpl(const F& f) { Storage& storage = storageRef(); return f(storage); }
    int get(int key) { int result = getImpl([&](Storage& storage) { return Helper::find(storage, key); }); return result; }
};
int run() { Traits t; Map m; m.m_storage = &t; printf("%d\n", m.get(2)); return 0; }
}

namespace p_mtinh {
template<typename T> struct Table { using Storage = T; Storage* m_storage = nullptr; Storage& storageRef() { return *m_storage; } };
struct Traits { int v = 5; };
struct Map : Table<Traits> {
    template<typename F> int getImpl(const F& f) { Storage& storage = storageRef(); return f(storage.v); }
};
struct Add { int k; int operator()(int x) const { return x + k; } };
int run() { Traits t; Map m; m.m_storage = &t; printf("%d\n", m.getImpl(Add{2})); return 0; }
}

namespace p_lamlocal {
int run() {
    enum class Type { Query, Fallback, Stars };
    typedef int Count;
    struct Task { Type type; Count n; };
    auto name = [] (Type type) -> const char* { switch (type) { case Type::Query: return "query"; case Type::Fallback: return "fallback"; default: return "stars"; } };
    auto total = [](const Task& t) { Count c = t.n * 2; return c; };
    Task t { Type::Fallback, 21 };
    printf("%s %d\n", name(t.type), total(t));
    return 0;
}
}

namespace p_lamlocal2 {
int run() {
    enum class Type { Query, Fallback, Stars };
    Type t = Type::Fallback;
    auto f = [] (Type type) { return type == Type::Stars; };
    printf("%d %d\n", (int)t, f(Type::Stars));
    return 0;
}
}

namespace p_ifcx {
enum class E { A, B };
template<E e> int f(int v) {
    if (v > 0) {
        if constexpr (e == E::A)
            do { if (v > 100) [[unlikely]] { if (v > 200) return -1; } } while (false);
        else
            (void)v;
        return v * 2;
    }
    return 0;
}
int run() { printf("%d %d\n", f<E::A>(3), f<E::B>(4)); return 0; }
}

int main() {
    p_lamtypedef::run();
    p_mtinh::run();
    p_lamlocal::run();
    p_lamlocal2::run();
    p_ifcx::run();
    return 0;
}
