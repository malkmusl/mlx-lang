// `if (T { a } != T { b })`: temporaries made by braces compared, not a
// declaration (InlineInvalidation's TextBreakingPositionContext).
#include <stdio.h>
struct Style { int size; };
struct Context {
    Context(const Style& style) : size(style.size) { }
    bool operator!=(const Context& other) const { return size != other.size; }
    int size;
};
int changed(const Style& oldStyle, const Style& newStyle)
{
    if (Context { oldStyle } != Context { newStyle })
        return 1;
    return 0;
}
int main(int argc, char** argv)
{
    Style a { 3 }, b { 4 };
    printf("%d %d\n", changed(a, b), changed(a, a));
    return argc - 1;
}
