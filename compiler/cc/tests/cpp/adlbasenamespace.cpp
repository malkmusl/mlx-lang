// Argument-dependent lookup takes the namespaces of the argument's
// bases too, beside a function of the name the unqualified lookup found
// first (`*protect(builderState)` in WebCore's Style::Calculation,
// whose own protect(Value &) hides WTF's: WTF::protect(T *) found
// through CanMakeCheckedPtr<BuilderState>).
#include <cstdio>
namespace W {
template<typename T> struct CanCheck { int checked = 5; };
template<typename T> struct CheckedPtr { T* p; T& operator*() const { return *p; } };
template<typename T> struct Ref { T* p; T& operator*() const { return *p; } };
template<typename T> CheckedPtr<T> protect(T* p) { return CheckedPtr<T> { p }; }
struct Plain { int v = 11; };
int describe(const Plain& p) { return p.v; }
}
using W::protect;
namespace S {
struct Builder : W::CanCheck<Builder> { int v = 7; };
struct Derived : W::Plain { };
namespace C {
class Value { public: int v = 9; };
W::Ref<Value> protect(Value& v) { return W::Ref<Value> { &v }; }
int describe(const Value& v) { return v.v; }
int g(Builder* p) { return (*protect(p)).v; }
int h(Value& v) { return (*protect(v)).v; }
int k(const Derived& d) { return describe(d); }
}
}
int main() {
    S::Builder b;
    S::C::Value v;
    S::Derived d;
    printf("%d %d %d\n", S::C::g(&b), S::C::h(v), S::C::k(d));
}
