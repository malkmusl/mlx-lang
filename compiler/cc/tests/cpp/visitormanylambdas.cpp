// WTF's Visitor of many lambdas (WebCore's StyleRuleBase::visitDerived(
// WTF::makeVisitor(...)) of 25 in StyleRule.cpp), each case through
// std::invoke: `using Visitor<A>::operator(); using Visitor<B...>::
// operator();` names both bases by the template's name, and each base
// once is the class's using-declarations (twice made a lookup of
// operator() walk 2^n bases: css-19 filled the arena).
#include <cstdio>
#include <functional>
#include <utility>

template<class A, class... B> struct Visitor : Visitor<A>, Visitor<B...> {
    Visitor(A a, B... b)
        : Visitor<A>(a)
        , Visitor<B...>(b...)
    {
    }

    using Visitor<A>::operator ();
    using Visitor<B...>::operator ();
};

template<class A> struct Visitor<A> : A {
    Visitor(A a)
        : A(a)
    {
    }

    using A::operator();
};

template<class... F> Visitor<F...> makeVisitor(F... f) { return Visitor<F...>(f...); }

struct Rule {
    int kind;
    template<typename V> decltype(auto) visitDerived(V&&);
    template<typename V> decltype(auto) visitDerived(V&&) const;
};
struct Rule0 : Rule { int value() const { return 0; } };
struct Rule1 : Rule { int value() const { return 1; } };
struct Rule2 : Rule { int value() const { return 2; } };
struct Rule3 : Rule { int value() const { return 3; } };
struct Rule4 : Rule { int value() const { return 4; } };
struct Rule5 : Rule { int value() const { return 5; } };
struct Rule6 : Rule { int value() const { return 6; } };
struct Rule7 : Rule { int value() const { return 7; } };
struct Rule8 : Rule { int value() const { return 8; } };
struct Rule9 : Rule { int value() const { return 9; } };
struct Rule10 : Rule { int value() const { return 10; } };
struct Rule11 : Rule { int value() const { return 11; } };
struct Rule12 : Rule { int value() const { return 12; } };
struct Rule13 : Rule { int value() const { return 13; } };
struct Rule14 : Rule { int value() const { return 14; } };
struct Rule15 : Rule { int value() const { return 15; } };

template<typename V> decltype(auto) Rule::visitDerived(V&& visitor)
{
    switch (kind) {
    case 0:
        return std::invoke(std::forward<V>(visitor), static_cast<Rule0&>(*this));
    case 1:
        return std::invoke(std::forward<V>(visitor), static_cast<Rule1&>(*this));
    case 2:
        return std::invoke(std::forward<V>(visitor), static_cast<Rule2&>(*this));
    case 3:
        return std::invoke(std::forward<V>(visitor), static_cast<Rule3&>(*this));
    case 4:
        return std::invoke(std::forward<V>(visitor), static_cast<Rule4&>(*this));
    case 5:
        return std::invoke(std::forward<V>(visitor), static_cast<Rule5&>(*this));
    case 6:
        return std::invoke(std::forward<V>(visitor), static_cast<Rule6&>(*this));
    case 7:
        return std::invoke(std::forward<V>(visitor), static_cast<Rule7&>(*this));
    case 8:
        return std::invoke(std::forward<V>(visitor), static_cast<Rule8&>(*this));
    case 9:
        return std::invoke(std::forward<V>(visitor), static_cast<Rule9&>(*this));
    case 10:
        return std::invoke(std::forward<V>(visitor), static_cast<Rule10&>(*this));
    case 11:
        return std::invoke(std::forward<V>(visitor), static_cast<Rule11&>(*this));
    case 12:
        return std::invoke(std::forward<V>(visitor), static_cast<Rule12&>(*this));
    case 13:
        return std::invoke(std::forward<V>(visitor), static_cast<Rule13&>(*this));
    case 14:
        return std::invoke(std::forward<V>(visitor), static_cast<Rule14&>(*this));
    case 15:
        return std::invoke(std::forward<V>(visitor), static_cast<Rule15&>(*this));
    }
    return std::invoke(std::forward<V>(visitor), static_cast<Rule0&>(*this));
}

template<typename V> decltype(auto) Rule::visitDerived(V&& visitor) const
{
    return const_cast<Rule&>(*this).visitDerived([&](auto& value) {
        return std::invoke(std::forward<V>(visitor), std::as_const(value));
    });
}

int main()
{
    int total = 0;
    int generic = 0;
    for (int k = 0; k < 16; ++k) {
        Rule rule { k };
        total += rule.visitDerived(makeVisitor(
            [&](Rule0& r) -> int { return r.value() * 1; },
            [&](Rule1& r) -> int { return r.value() * 2; },
            [&](Rule2& r) -> int { return r.value() * 3; },
            [&](Rule3& r) -> int { return r.value() * 4; },
            [&](Rule4& r) -> int { return r.value() * 5; },
            [&](Rule5& r) -> int { return r.value() * 6; },
            [&](Rule6& r) -> int { return r.value() * 7; },
            [&](Rule7& r) -> int { return r.value() * 8; },
            [&](Rule8& r) -> int { return r.value() * 9; },
            [&](Rule9& r) -> int { return r.value() * 10; },
            [&](Rule10& r) -> int { return r.value() * 11; },
            [&](Rule11& r) -> int { return r.value() * 12; },
            [&](Rule12& r) -> int { return r.value() * 13; },
            [&](Rule13& r) -> int { return r.value() * 14; },
            [&](Rule14& r) -> int { return r.value() * 15; },
            [&](Rule15& r) -> int { return r.value() * 16; }
        ));
        const Rule& constant = rule;
        constant.visitDerived(makeVisitor(
            [&](const Rule3&) { generic += 100; },
            [&](const Rule7& r) { generic += r.value(); },
            [&](const auto& r) { generic += r.value() + 1; }));
    }
    printf("%d %d\n", total, generic);
    return 0;
}
