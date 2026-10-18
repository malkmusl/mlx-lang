// <string_view>: find, rfind, the find_*_of family, starts_with,
// substr, comparisons with views and C strings, the ""sv literal,
// std::string to and from a view, std::char_traits; a constructor
// reached through another class's conversion operator.
#include <string_view>
#include <string>
#include <stdio.h>
using namespace std::literals;
static size_t len(std::string_view v) { return v.size(); }
int main() {
    std::string_view a = "hello world";
    std::string s = "hello";
    std::string_view b = s;
    printf("%zu %zu %d %d\n", a.size(), len(s), (int)a.find("world"), (int)a.rfind('o'));
    printf("%d %d %d %d\n", a.starts_with("hello"), a.ends_with("xx"), b == a.substr(0, 5), a < b);
    auto c = "abc"sv;
    printf("%zu %d %d\n", c.size(), c == "abc", (int)a.find_first_of("ow"));
    std::string t(c);
    printf("%s %zu %d\n", t.c_str(), std::char_traits<char>::length("four"), std::char_traits<char16_t>::compare(u"ab", u"ac", 2));
    a.remove_prefix(6);
    for (char ch : a) putchar(ch);
    printf(" %d\n", (int)a.find_last_not_of("d"));
    return 0;
}
