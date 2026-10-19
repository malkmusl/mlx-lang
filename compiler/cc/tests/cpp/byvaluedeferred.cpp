// A function defined with a parameter by value of a class template's
// instance named only so far (through an alias) makes the instance
// first: its destructor ran nowhere (the function took it for a class
// without one), and its size was the incomplete class's.
#include <stdio.h>
template<class T> struct Logged {
    T v;
    T w[3];
    ~Logged() { printf("destroyed %d\n", (int)v); }
};
using L = Logged<int>;
void take(L) { printf("take\n"); }
int second(L l) { return l.w[1]; }

int main(int argc, char **)
{
    take(L { 5 });
    printf("after\n");
    int s = second(L { 6, { 1, 2, 3 } });
    printf("%d\n", s);
    return argc;
}
