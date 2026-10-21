// A class declared in a namespace hides an enumerator (or a variable) of
// the same name in a namespace around it: `struct Tree;` of
// WebCore::CSSCalc beside WebCore's `enum TreeType { Tree, ... }`
// (WebCore's CSSCalcTree+Simplification.h). In the outer namespace the
// name is still the enumerator.
#include <stdio.h>

namespace Outer {
enum TreeType { Tree, Shadow };
int count = 5;

namespace Calc {
struct Tree;
struct count { int value; };
int depth(const Tree &);
struct Tree { int d; };
int depth(const Tree &tree) { return tree.d; }
int counted(count c) { return c.value * 2; }
}

int kind() { return Tree + count; }
}

int main(int argc, char **)
{
    Outer::Calc::Tree t { 7 };
    printf("%d %d %d\n", Outer::Calc::depth(t), Outer::Calc::counted({ 4 }), Outer::kind());
    return argc;
}
