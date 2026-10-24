// An explicit specialization of a function template whose name a plain
// function declared after it in the namespace also has (WTF's
// containerSize beside JSONValues.h's, specialized by
// StylePropertyShorthand.h): the template is specialized ("not a
// function template").
#include <stdio.h>
#include <stddef.h>

namespace W {
template<typename C> size_t containerSize(const C& c) { return c.size(); }
struct Json { size_t length() const { return 7; } };
inline size_t containerSize(const Json& a) { return a.length(); }
template<typename C> size_t total(const C& c) { return containerSize(c) + 1; }
}
namespace X {
struct Short { size_t length() const { return 3; } };
}
namespace W {
template<> inline size_t containerSize(const X::Short& s) { return s.length() * 10; }
}

int main(int argc, char **argv)
{
    printf("%zu %zu\n", W::total(X::Short()), W::total(W::Json()));
    return argc;
}
