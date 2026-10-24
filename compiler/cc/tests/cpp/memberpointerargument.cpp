// A pointer to a member as a template argument: `&C::f` for a parameter
// of a pointer to a member's type or an `auto` one, a member function's
// (virtual too) or a data member's (WebCore's StyleExtractor calls
// `paddingIsLayoutDependent.template operator()<&ComputedStyle::paddingTop>`
// of a lambda). An `auto` parameter given 13 is still the value 13.
#include <stdio.h>

struct Style {
    int top { 3 };
    int right { 4 };
    int paddingTop() const { return top; }
    int paddingRight() const { return right; }
    virtual int kind() const { return 1; }
    virtual ~Style() { }
};

struct Derived : Style {
    int kind() const override { return 2; }
};

template<auto acc> int call(const Style &s) { return (s.*acc)(); }
template<int (Style::*acc)() const> int typed(const Style &s) { return (s.*acc)() + 10; }
template<int Style::*field> int read(const Style &s) { return s.*field; }
template<auto v> int value() { return v * 2; }

struct Holder {
    template<auto acc> int operator()(const Style &s) const { return (s.*acc)() + 100; }
};

int run(int which, const Style &style)
{
    auto padding = [&]<auto accessor>(const Style &s, int extra) -> bool { return (s.*accessor)() + extra > 5; };
    switch (which) {
    case 1:
        return padding.template operator()<&Style::paddingTop>(style, 2);
    default:
        return padding.template operator()<&Style::paddingRight>(style, 2);
    }
}

int main(int argc, char **)
{
    Style s;
    Derived d;
    Holder h;
    printf("%d %d %d %d\n", call<&Style::paddingTop>(s), call<&Style::paddingRight>(s), typed<&Style::paddingTop>(s), h.template operator()<&Style::paddingRight>(s));
    printf("%d %d %d %d\n", call<&Style::kind>(s), call<&Style::kind>(d), read<&Style::right>(s), read<&Style::top>(d));
    printf("%d %d %d\n", run(1, s), run(2, s), value<13>());
    return argc;
}
