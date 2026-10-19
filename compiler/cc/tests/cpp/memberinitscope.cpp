// A default member initializer is read in the class's scope, not the
// constructor's: a parameter named as a member function (`root` of
// WebCore's InlineItemsBuilder, whose `m_hasTextAutospace { !root()...
// }` called the parameter: "the object is not callable") or as a data
// member hides neither; its `this` is the object's, a lambda's too.
#include <stdio.h>

struct Builder {
    Builder(int root, int base);
    Builder(int root) : Builder(root, 0) {}
    int root() const { return m_root * 2; }
    int m_root;
    int base = 100;
    int doubled { root() + 1 };
    int fromBase = base + 1;
    int viaThis = this->m_root + 1;
    int lambda = [this] { return base * 2 + m_root; }();
};

Builder::Builder(int root, int base)
    : m_root(root)
{
    printf("%d %d\n", root, base);
}

int main(int argc, char **argv)
{
    Builder b(7, 3);
    Builder c(4);
    printf("%d %d %d %d %d %d\n", b.doubled, b.fromBase, c.doubled, c.fromBase, b.viaThis, c.lambda);
    return argc;
}
