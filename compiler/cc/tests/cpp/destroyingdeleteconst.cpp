// `delete static_cast<const T *>(this)` of a class with a destroying
// operator delete passes the pointer as the parameter's `T *` (WTF's
// ThreadSafeRefCounted::deref; the call of a const pointer failed an
// enclosing SFINAE check once arguments were weighed there), also where
// a trailing return type asks for the destructor's call.
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <utility>

struct Node {
    int value = 7;
    ~Node() { printf("~Node %d\n", value); }
    static void operator delete(Node *node, std::destroying_delete_t, size_t size)
    {
        printf("destroying delete %d %d\n", node->value, (int)(size == sizeof(Node)));
        node->~Node();
        ::operator delete(node);
    }
    void release() const { delete static_cast<const Node *>(this); }
};

template<class T> auto releaseAll(const T *p) -> decltype(p->release(), 1) { p->release(); return 1; }

int main(int argc, char **)
{
    const Node *n = new Node;
    n->release();
    Node *m = new Node;
    m->value = 9;
    printf("%d\n", releaseAll(m));
    return argc;
}
