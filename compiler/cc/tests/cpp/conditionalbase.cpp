// `c ? derived : base` of pointers to a class and to one derived from
// it: a pointer to the base, the derived side moved to its base part,
// whichever side it is on (WebCore's Range.cpp: `dynamicDowncast<
// ContainerNode>(r ? r->parentNode() : &startContainer())` took the
// ContainerNode* side's type and failed TypeCasts.h's "Should be a
// downcast"; a second base's members were read at the derived offsets).
#include <stdio.h>
#include <type_traits>
struct A { int a = 1; virtual ~A() {} };
struct N { int n = 2; virtual ~N() {} };
struct C : A, N { int c = 3; };

N *first(C *c, N *n, bool b) { return b ? c : n; }
N *second(C *c, N *n, bool b) { return b ? n : c; }
template <class T, class S> int downcast(S *s) {
    static_assert(!std::is_same_v<S, T>, "same type");
    return s ? 1 : 0;
}

int main(int argc, char **)
{
    C c;
    N n;
    static_assert(std::is_same_v<decltype(argc ? &c : &n), N *>);
    static_assert(std::is_same_v<decltype(argc ? &n : &c), N *>);
    static_assert(std::is_same_v<decltype(argc ? &c : (const N *)&n), const N *>);
    auto p = argc ? &c : &n;
    auto q = argc > 5 ? &c : &n;
    printf("%d %d\n", p->n, q->n);
    printf("%d %d %d %d\n", first(&c, &n, true)->n, first(&c, &n, false)->n, second(&c, &n, true)->n, second(&c, &n, false)->n);
    C *none = nullptr;
    N *maybe = argc ? none : &n;
    printf("%d\n", maybe == nullptr);
    printf("%d\n", downcast<C>(argc ? &c : &n));
    const N *r = argc > 5 ? &n : (const C *)&c;
    printf("%d %d\n", r->n, (int)((const char *)r - (const char *)&c) == (int)((const char *)static_cast<N *>(&c) - (const char *)&c));
    return argc;
}
