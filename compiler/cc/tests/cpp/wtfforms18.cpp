// WebKit forms 18: JavaScriptCore's (partial specializations' members defined outside, a base template's injected name, alignas class templates)
#include <stdio.h>

namespace p_partdef {
struct Unknown {};
template<typename Base, typename T> struct Conv { int which() const { return 1; } };
template<typename Base> struct Conv<Base, Unknown> {
    int which() const;
    template<typename U> int plus(U u) const;
};
template<typename T> struct Handle : Conv<Handle<T>, T> { int v = 4; };
template<typename Base> int Conv<Base, Unknown>::which() const { return 10 + static_cast<const Base *>(this)->v; }
template<typename Base> template<typename U> int Conv<Base, Unknown>::plus(U u) const { return which() + (int)u; }
int run() {
    Handle<int> a;
    Handle<Unknown> b;
    printf("%d %d %d\n", a.which(), b.which(), b.plus(2.5));
    return 0;
}
}

namespace p_baseinj {
template<typename T> struct Counted { unsigned count() const { return 3; } static int kind() { return 7; } };
struct Env { virtual unsigned count() const = 0; virtual ~Env() {} };
struct Ctl final : Env, Counted<Ctl> {
    unsigned count() const final { return Counted::count() + 1; }
    int k() const { return Counted::kind(); }
};
int run() { Ctl c; printf("%u %d\n", c.count(), c.k()); return 0; }
}

namespace p_alignt {
template <typename T>
class alignas(64) Lexer {
public:
    T value;
    Lexer(T v) : value(v) {}
};
template <typename T> struct [[nodiscard]] Tagged { T v; };
int run() { Lexer<int> l(3); Tagged<int> t { 5 }; printf("%d %d %d\n", (int)alignof(Lexer<int>), l.value, t.v); return 0; }
}

namespace p_latemt2 {
template<typename T> struct Aux {
    template<typename U> void set(int k, const U &v);
    T m_value;
};
struct Holder { Aux<long> b; };
template<typename T> template<typename U> void Aux<T>::set(int k, const U &v) { m_value = v + k; }
int run() {
    Holder h;
    h.b.set(2, 40L);
    printf("%ld\n", h.b.m_value);
    return 0;
}
}

int main() {
    p_partdef::run();
    p_baseinj::run();
    p_alignt::run();
    p_latemt2::run();
    return 0;
}
