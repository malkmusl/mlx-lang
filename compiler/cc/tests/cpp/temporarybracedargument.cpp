// A statement starting with a temporary made by parentheses whose
// arguments hold braces, its member then used (WebCore's
// `Styleable(element, Style::PseudoElementIdentifier { Marker }).cancel();`),
// is an expression, not a declaration.
#include <stdio.h>

struct Identifier { int type; };

struct Styleable {
    int element;
    Identifier id;
    Styleable(int e, Identifier i) : element(e), id(i) { }
    void cancel() const { printf("cancel %d %d\n", element, id.type); }
    int value() const { return element + id.type; }
};

int main(int argc, char **)
{
    Styleable(4, Identifier { 7 }).cancel();
    int v = Styleable(1, Identifier { 2 }).value();
    Styleable kept(3, Identifier { 5 });
    kept.cancel();
    printf("%d\n", v);
    return argc;
}
