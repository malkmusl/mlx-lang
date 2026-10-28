// A template's default argument `CSS::All` names its namespace's CSS where
// it is read for a class whose base has a member type CSS (WebCore's
// Style::Integer), and that member type in the class's own code.
#include <stdio.h>
namespace W {
namespace CSS { struct Range { int min; int max; }; constexpr Range All { -1000, 1000 }; template<Range R = All> struct Integer { static constexpr int span = R.max - R.min; }; }
namespace Style {
template<CSS::Range R = CSS::All, typename V = int> struct Integer { static constexpr int span = R.max - R.min; V value; };
struct Base { using CSS = W::CSS::Integer<>; };
struct Derived : Base {
    int f() { Integer<> i { 3 }; return Integer<>::span + i.value + CSS::span; }
};
}
}
int main() { W::Style::Derived d; printf("%d\n", d.f()); return 0; }
