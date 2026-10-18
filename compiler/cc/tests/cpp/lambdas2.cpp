// Init-captures (by copy, by reference, of move-only values), the implicit move constructor moving members, a base's enum named in a constructor's initializer, a destroying operator delete and a qualified destructor call.
#include <stdio.h>
#include <memory>
#include <utility>
#include <string>
#include <new>
#include <stdlib.h>

namespace p_ic1 {
struct MoveOnly { std::unique_ptr<int> p; int get() const { return *p; } };
template<typename F> int run(F&& f) { return f(); }
void run() {
    int base = 5;
    auto a = [x = base * 2, &r = base] { return x + r; };
    base = 7;
    MoveOnly m { std::make_unique<int>(41) };
    auto b = [m = std::move(m)]() mutable { return m.get() + 1; };
    std::string s = "abc";
    auto c = [t = std::move(s), n{3}] { return (int)t.size() + n; };
    printf("%d %d %d %d\n", a(), run(b), c(), m.p ? 1 : 0);
}
}

namespace p_ic3 {
struct MoveOnly { std::unique_ptr<int> p; int get() const { return *p; } };
void run() {
    MoveOnly m { std::make_unique<int>(41) };
    MoveOnly n = std::move(m);
    printf("%d %d\n", n.get(), m.p ? 1 : 0);
}
}

namespace p_be1 {
struct WB { enum class Type { Serial, Concurrent }; WB(const char* n, Type t) : t(t) {} Type t; };
struct WQ : WB { WQ(const char* n) : WB(n, Type::Serial) {} bool serial() const { return t == Type::Serial; } };
void run() { WQ q("x"); printf("%d\n", q.serial()); }
}

namespace p_od1 {
struct T {
    int v = 3;
    ~T() { printf("~T %d\n", v); }
    static void operator delete(void* p) { printf("free\n"); free(p); }
    void operator delete(T* object, std::destroying_delete_t, size_t size) {
        object->T::~T();
        T::operator delete(object);
    }
    using Did __attribute__((unused)) = int;
};
void run() { T* t = new T; delete t; }
}

int main() {
    p_ic1::run();
    p_ic3::run();
    p_be1::run();
    p_od1::run();
    return 0;
}
