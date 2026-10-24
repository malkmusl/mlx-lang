// A declaration as an if's (or a loop's) statement, no braces: in a
// scope of its own, its object made and destroyed there (WebCore's
// `if (CheckedPtr block = ...) RelayoutScopeForScrollbarChange
// relayoutScope { *block, m_inOverflowRelayout };`).
#include <stdio.h>

static int depth = 0;

struct Scope {
    int id;
    Scope(int i, int extra) : id(i + extra) { printf("enter %d at %d\n", id, ++depth); }
    ~Scope() { printf("leave %d at %d\n", id, depth--); }
};

int main(int argc, char **)
{
    if (argc)
        Scope first { 1, 10 };
    else
        Scope second { 2, 20 };
    for (int i = 0; i < 2; ++i)
        Scope each(i, 100);
    int n = 0;
    while (n < 1)
        Scope inLoop { n++, 1000 };
    printf("depth %d\n", depth);
    return argc;
}
