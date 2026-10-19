// std::ranges algorithms with a pointer to a member function as the
// projection (`std::ranges::sort(properties, codePointCompareLessThan,
// &JSC::Identifier::string)` in WebCore's KeyframeEffect.cpp: "no
// matching constructor for WTF::StringView"), the comparison taking
// another type the projection converts to; a data member's still.
#include <stdio.h>
#include <algorithm>
#include <vector>

struct View {
    const char *s;
    View(const char *text) : s(text) {}
};
struct Name {
    const char *text;
    int rank;
    const char *const &string() const { return text; }
    int doubled() { return rank * 2; }
};
bool lessThan(View a, View b) { return a.s[0] < b.s[0]; }

int main(int argc, char **argv)
{
    std::vector<Name> names { { "c", 1 }, { "a", 3 }, { "b", 2 } };
    std::ranges::sort(names, lessThan, &Name::string);
    printf("%s %s %s\n", names[0].text, names[1].text, names[2].text);
    std::ranges::sort(names, {}, &Name::rank);
    printf("%s %s %s\n", names[0].text, names[1].text, names[2].text);
    std::ranges::sort(names, [](int a, int b) { return a > b; }, &Name::doubled);
    printf("%s %s %s\n", names[0].text, names[1].text, names[2].text);
    return argc;
}
