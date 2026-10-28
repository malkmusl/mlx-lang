// `new T[n] {a, b}` and `new (where) T[n] {...}` (B3's adjacency lists:
// `new (childrenAlloc()) Value*[sizeof...(arguments)] { arguments... }`):
// the elements given first, the others value-initialized; placed, the
// array has no cookie.
#include <stdio.h>
#include <new>
struct S { S() : v(7) { } S(int x) : v(x) { } ~S() { printf("~%d ", v); } int v; };
struct V { int x; };
template<typename... A> void build(void* p, A... a) { new (p) V*[sizeof...(a)] { a... }; }
int main()
{
    int* a = new int[5] { 1, 2, 3 };
    printf("%d %d %d %d %d\n", a[0], a[1], a[2], a[3], a[4]);
    delete[] a;
    S* s = new S[4] { 10, 20 };
    printf("%d %d %d %d\n", s[0].v, s[1].v, s[2].v, s[3].v);
    delete[] s;
    printf("\n");
    alignas(S) char buffer[3 * sizeof(S)];
    S* p = new (buffer) S[3] { 1 };
    printf("%d %d %d %d\n", p[0].v, p[1].v, p[2].v, (char*)p == buffer);
    long n = 3;
    int* z = new int[n]();
    printf("%d %d %d\n", z[0], z[1], z[2]);
    delete[] z;
    V x { 1 }, y { 2 }, w { 3 };
    V* list[3];
    build(list, &x, &y, &w);
    printf("%d %d %d\n", list[0]->x, list[1]->x, list[2]->x);
    return 0;
}
