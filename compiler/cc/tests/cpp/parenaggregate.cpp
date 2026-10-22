// C++20's parenthesized aggregate initialization: `Q q(1, 2.5)` makes
// the aggregate's elements from the arguments in order, and
// std::is_constructible says so, which std::optional's converting
// constructor asks (WebCore assigning `{ StepsEasingParameters::JumpStart
// { ... } }` to an std::optional<StepsEasingParameters>, an aggregate
// holding a Variant).
#include <cstdio>
#include <optional>
#include <type_traits>
#include <variant>

struct A { int x; bool operator==(const A&) const = default; };
struct B { int y; bool operator==(const B&) const = default; };
struct P {
    std::variant<A, B> value;
    bool operator==(const P&) const = default;
};
struct Q { int a; double b; };
struct Base { int base; };
struct Derived : Base { int more; };

static std::optional<P> make(int which)
{
    std::optional<P> parameters;
    switch (which) {
    case 0:
        parameters = { A { 3 } };
        break;
    case 1:
        parameters = { B { 4 } };
        break;
    default:
        return { };
    }
    return parameters;
}

int main()
{
    P p(A { 3 });
    Q q(1, 2.5);
    Q partial(7);
    Derived d(Base { 1 }, 2);
    printf("%d %d %g %d %g %d %d\n", (int)p.value.index(), q.a, q.b, partial.a, partial.b, d.base, d.more);
    printf("%d %d %d %d %d\n", (int)std::is_constructible_v<P, A>, (int)std::is_constructible_v<Q, int, double>, (int)std::is_constructible_v<Q, int, const char*>, (int)std::is_constructible_v<Q, int, double, int>, (int)std::is_constructible_v<Derived, Base, int>);
    auto first = make(0);
    auto second = make(1);
    auto none = make(2);
    printf("%d %d %d\n", first ? (int)first->value.index() : -1, second ? (int)second->value.index() : -1, none ? 1 : 0);
    printf("%d %d\n", std::get<A>(first->value).x, std::get<B>(second->value).y);
    std::optional<P> o;
    o = std::optional<P> { A { 4 } };
    printf("%d\n", std::get<A>(o->value).x);
    return 1;
}
