// More forms WTF's sources use: a conversion operator named in an expression before a class template's operator[]
#include <stdio.h>

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

int main() {
    p_convname::run();
    return 0;
}
