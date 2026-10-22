// Argument-dependent lookup sees the namespaces of an argument's base
// classes: WebCore's `map(styleProperties, f)` finds WTF::map through
// RefCounted<StyleProperties>.
#include <stdio.h>
namespace W {
template<typename T> struct RefCounted { int refs = 0; };
template<typename S, typename F> int map(S& source, F function)
{
    int sum = 0;
    for (int i = 0; i < source.count; i++)
        sum += function(source.items[i]);
    return sum;
}
}
namespace C {
struct Properties : W::RefCounted<Properties> {
    int items[3] = { 1, 2, 3 };
    int count = 3;
};
int total()
{
    Properties p;
    return map(p, [](int v) { return v * 10; });
}
}
int main(int argc, char** argv)
{
    printf("%d\n", C::total());
    return argc - 1;
}
