// More forms WTF's and bmalloc's sources use: a conversion operator named in an expression before a class template's operator[]; default arguments given by a declaration before the definition
#include <stdio.h>
#include <stddef.h>

namespace p_convname {
struct P {
    int v;
    operator int*() { return &v; }
};
template<class T> struct Arr {
    T e[4];
    T& operator[](unsigned long i) { return e[i]; }
};
int run() {
    P p { 3 };
    int* q = p.operator int*();
    Arr<char> a;
    a[1] = 'x';
    a[2] = a[1] + 1;
    printf("%d %c %c\n", *q, a[1], a[2]);
    return 0;
}
}

namespace p_defarg {
namespace bm {
enum class Tag { Malloc = 7, Other };
inline void purge(void* p, size_t n, Tag usage = Tag::Malloc);
inline void purge(void* p, size_t n, Tag usage)
{
    printf("%d %zu\n", (int)usage, n);
}
void* use(size_t size)
{
    static char buffer[16];
    purge(buffer, size);
    return buffer;
}
}
int run() {
    bm::use(16);
    bm::purge(nullptr, 3, bm::Tag::Other);
    return 0;
}
}

int main() {
    p_convname::run();
    p_defarg::run();
    return 0;
}
