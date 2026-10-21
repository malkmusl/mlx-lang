// `c ? nullptr : p` is of p's pointer type (it was void *): WebCore's
// RenderHighlight deduces `CheckedPtr span = c ? nullptr : p->spanner()`,
// a CheckedPtr<void> whose destructor dereferenced a void pointer.
#include <stdio.h>
#include <type_traits>
struct Box { int v = 4; };
template<class T> struct Ptr { T* p; Ptr(T* q) : p(q) {} };
int main(int argc, char**)
{
    Box b;
    Box* bp = &b;
    static_assert(std::is_same_v<decltype(argc ? nullptr : bp), Box*>);
    static_assert(std::is_same_v<decltype(argc ? bp : nullptr), Box*>);
    Ptr span = argc > 5 ? nullptr : bp;
    Ptr span2 = argc > 0 ? bp : nullptr;
    auto a = argc ? nullptr : bp;
    printf("%d %d %d\n", span.p->v, span2.p->v, a == nullptr);
    return argc;
}
